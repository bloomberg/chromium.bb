// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_plugin_registry.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "remoting/client/plugin/pepper_entrypoints.h"

namespace {

const char* kPDFPluginName = "Chrome PDF Viewer";
const char* kPDFPluginMimeType = "application/pdf";
const char* kPDFPluginExtension = "pdf";
const char* kPDFPluginDescription = "Portable Document Format";

const char* kNaClPluginName = "Chrome NaCl";
const char* kNaClPluginMimeType = "application/x-nacl";
const char* kNaClPluginExtension = "nexe";
const char* kNaClPluginDescription = "Native Client Executable";

const char* kRemotingPluginMimeType = "pepper-application/x-chromoting";

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<PepperPluginInfo>* plugins) {
  // PDF.
  //
  // Once we're sandboxed, we can't know if the PDF plugin is available or not;
  // but (on Linux) this function is always called once before we're sandboxed.
  // So the first time through test if the file is available and then skip the
  // check on subsequent calls if yes.
  static bool skip_pdf_file_check = false;
  FilePath path;
  if (PathService::Get(chrome::FILE_PDF_PLUGIN, &path)) {
    if (skip_pdf_file_check || file_util::PathExists(path)) {
      PepperPluginInfo pdf;
      pdf.path = path;
      pdf.name = kPDFPluginName;
      pdf.mime_types.push_back(kPDFPluginMimeType);
      pdf.file_extensions = kPDFPluginExtension;
      pdf.type_descriptions = kPDFPluginDescription;
      plugins->push_back(pdf);

      skip_pdf_file_check = true;
    }
  }

  // Native client.
  //
  // Verify that we enable nacl on the command line. The name of the switch
  // varies between the browser and renderer process.
  if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path) &&
      file_util::PathExists(path)) {
    PepperPluginInfo nacl;
    nacl.path = path;
    nacl.name = kNaClPluginName;
    nacl.mime_types.push_back(kNaClPluginMimeType);

    // TODO(bbudge) Remove this mime type after NaCl tree has been updated.
    const char* kNaClPluginOldMimeType = "application/x-ppapi-nacl-srpc";
    nacl.mime_types.push_back(kNaClPluginOldMimeType);

    nacl.file_extensions = kNaClPluginExtension;
    nacl.type_descriptions = kNaClPluginDescription;
    plugins->push_back(nacl);
  }

  // Remoting.
#if defined(ENABLE_REMOTING)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRemoting)) {
    PepperPluginInfo info;
    info.is_internal = true;
    info.path = FilePath(FILE_PATH_LITERAL("internal-chromoting"));
    info.mime_types.push_back(kRemotingPluginMimeType);
    info.internal_entry_points.get_interface = remoting::PPP_GetInterface;
    info.internal_entry_points.initialize_module =
        remoting::PPP_InitializeModule;
    info.internal_entry_points.shutdown_module = remoting::PPP_ShutdownModule;

    plugins->push_back(info);
  }
#endif
}

// Appends any plugins from the command line to the given vector.
void ComputePluginsFromCommandLine(std::vector<PepperPluginInfo>* plugins) {
  const std::string value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRegisterPepperPlugins);
  if (value.empty())
    return;

  bool out_of_process =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kPpapiOutOfProcess);

  // FORMAT:
  // command-line = <plugin-entry> + *( LWS + "," + LWS + <plugin-entry> )
  // plugin-entry = <file-path> + ["#" + <name> + ["#" + <description>]] +
  //                *1( LWS + ";" + LWS + <mime-type> )

  std::vector<std::string> modules;
  base::SplitString(value, ',', &modules);
  for (size_t i = 0; i < modules.size(); ++i) {
    std::vector<std::string> parts;
    base::SplitString(modules[i], ';', &parts);
    if (parts.size() < 2) {
      DLOG(ERROR) << "Required mime-type not found";
      continue;
    }

    std::vector<std::string> name_parts;
    base::SplitString(parts[0], '#', &name_parts);

    PepperPluginInfo plugin;
    plugin.is_out_of_process = out_of_process;
#if defined(OS_WIN)
    // This means we can't provide plugins from non-ASCII paths, but
    // since this switch is only for development I don't think that's
    // too awful.
    plugin.path = FilePath(ASCIIToUTF16(name_parts[0]));
#else
    plugin.path = FilePath(name_parts[0]);
#endif
    if (name_parts.size() > 1)
      plugin.name = name_parts[1];
    if (name_parts.size() > 2) {
      plugin.description = name_parts[2];
      plugin.type_descriptions = name_parts[2];
    }
    for (size_t j = 1; j < parts.size(); ++j)
      plugin.mime_types.push_back(parts[j]);

    plugins->push_back(plugin);
  }
}

}  // namespace

const char* PepperPluginRegistry::kPDFPluginName = ::kPDFPluginName;

PepperPluginInfo::PepperPluginInfo()
    : is_internal(false),
      is_out_of_process(false) {
}

PepperPluginInfo::~PepperPluginInfo() {}

// static
PepperPluginRegistry* PepperPluginRegistry::GetInstance() {
  static PepperPluginRegistry* registry = NULL;
  // This object leaks.  It is a temporary hack to work around a crash.
  // http://code.google.com/p/chromium/issues/detail?id=63234
  if (!registry)
    registry = new PepperPluginRegistry;
  return registry;
}

// static
void PepperPluginRegistry::ComputeList(std::vector<PepperPluginInfo>* plugins) {
  ComputeBuiltInPlugins(plugins);
  ComputePluginsFromCommandLine(plugins);
}

// static
void PepperPluginRegistry::PreloadModules() {
  std::vector<PepperPluginInfo> plugins;
  ComputeList(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (!plugins[i].is_internal && !plugins[i].is_out_of_process) {
      base::NativeLibrary library = base::LoadNativeLibrary(plugins[i].path);
      LOG_IF(WARNING, !library) << "Unable to load plugin "
                                << plugins[i].path.value();
    }
  }
}

const PepperPluginInfo* PepperPluginRegistry::GetInfoForPlugin(
    const FilePath& path) const {
  for (size_t i = 0; i < plugin_list_.size(); ++i) {
    if (path == plugin_list_[i].path)
      return &plugin_list_[i];
  }
  return NULL;
}

webkit::ppapi::PluginModule* PepperPluginRegistry::GetLiveModule(
    const FilePath& path) {
  NonOwningModuleMap::iterator it = live_modules_.find(path);
  if (it == live_modules_.end())
    return NULL;
  return it->second;
}

void PepperPluginRegistry::AddLiveModule(const FilePath& path,
                                         webkit::ppapi::PluginModule* module) {
  DCHECK(live_modules_.find(path) == live_modules_.end());
  live_modules_[path] = module;
}

void PepperPluginRegistry::PluginModuleDestroyed(
    webkit::ppapi::PluginModule* destroyed_module) {
  // Modules aren't destroyed very often and there are normally at most a
  // couple of them. So for now we just do a brute-force search.
  for (NonOwningModuleMap::iterator i = live_modules_.begin();
       i != live_modules_.end(); ++i) {
    if (i->second == destroyed_module) {
      live_modules_.erase(i);
      return;
    }
  }
  NOTREACHED();  // Should have always found the module above.
}

PepperPluginRegistry::~PepperPluginRegistry() {
  // Explicitly clear all preloaded modules first. This will cause callbacks
  // to erase these modules from the live_modules_ list, and we don't want
  // that to happen implicitly out-of-order.
  preloaded_modules_.clear();

  DCHECK(live_modules_.empty());
}

PepperPluginRegistry::PepperPluginRegistry() {
  ComputeList(&plugin_list_);

  // Note that in each case, AddLiveModule must be called before completing
  // initialization. If we bail out (in the continue clauses) before saving
  // the initialized module, it will still try to unregister itself in its
  // destructor.
  for (size_t i = 0; i < plugin_list_.size(); i++) {
    const PepperPluginInfo& current = plugin_list_[i];
    if (current.is_out_of_process)
      continue;  // Out of process plugins need no special pre-initialization.

    scoped_refptr<webkit::ppapi::PluginModule> module =
        new webkit::ppapi::PluginModule(current.name, this);
    AddLiveModule(current.path, module);
    if (current.is_internal) {
      if (!module->InitAsInternalPlugin(current.internal_entry_points)) {
        DLOG(ERROR) << "Failed to load pepper module: " << current.path.value();
        continue;
      }
    } else {
      // Preload all external plugins we're not running out of process.
      module = new webkit::ppapi::PluginModule(current.name, this);
      AddLiveModule(current.path, module);
      if (!module->InitAsLibrary(current.path)) {
        DLOG(ERROR) << "Failed to load pepper module: " << current.path.value();
        continue;
      }
    }
    preloaded_modules_[current.path] = module;
  }
}
