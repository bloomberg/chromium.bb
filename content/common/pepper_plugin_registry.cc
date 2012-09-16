// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_plugin_registry.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

// Appends any plugins from the command line to the given vector.
void ComputePluginsFromCommandLine(
    std::vector<content::PepperPluginInfo>* plugins) {
  bool out_of_process =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kPpapiOutOfProcess);
  const std::string value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRegisterPepperPlugins);
  if (value.empty())
    return;

  // FORMAT:
  // command-line = <plugin-entry> + *( LWS + "," + LWS + <plugin-entry> )
  // plugin-entry =
  //    <file-path> +
  //    ["#" + <name> + ["#" + <description> + ["#" + <version>]]] +
  //    *1( LWS + ";" + LWS + <mime-type> )
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

    content::PepperPluginInfo plugin;
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
    if (name_parts.size() > 2)
      plugin.description = name_parts[2];
    if (name_parts.size() > 3)
      plugin.version = name_parts[3];
    for (size_t j = 1; j < parts.size(); ++j) {
      webkit::WebPluginMimeType mime_type(parts[j],
                                          std::string(),
                                          plugin.description);
      plugin.mime_types.push_back(mime_type);
    }

    // Command-line plugins get full permissions.
    plugin.permissions = ppapi::PERMISSION_DEV |
                         ppapi::PERMISSION_PRIVATE |
                         ppapi::PERMISSION_BYPASS_USER_GESTURE;

    plugins->push_back(plugin);
  }
}

}  // namespace

webkit::WebPluginInfo content::PepperPluginInfo::ToWebPluginInfo() const {
  webkit::WebPluginInfo info;

  info.type = is_out_of_process ?
      (is_sandboxed ?
        webkit::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS :
        webkit::WebPluginInfo::PLUGIN_TYPE_PEPPER_UNSANDBOXED) :
      webkit::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;

  info.name = name.empty() ?
      path.BaseName().LossyDisplayName() : UTF8ToUTF16(name);
  info.path = path;
  info.version = ASCIIToUTF16(version);
  info.desc = ASCIIToUTF16(description);
  info.mime_types = mime_types;
  info.pepper_permissions = permissions;

  // TODO(brettw) bug 147507: remove this logging.
  LOG(INFO) << "PepperPluginInfo::ToWebPluginInfo \""
            << UTF16ToUTF8(info.path.LossyDisplayName()) << "\" "
            << "permissions = " << permissions;

  return info;
}

bool MakePepperPluginInfo(const webkit::WebPluginInfo& webplugin_info,
                          content::PepperPluginInfo* pepper_info) {
  if (!webkit::IsPepperPlugin(webplugin_info))
    return false;

  pepper_info->is_out_of_process = webkit::IsOutOfProcessPlugin(webplugin_info);
  pepper_info->is_sandboxed = webplugin_info.type !=
      webkit::WebPluginInfo::PLUGIN_TYPE_PEPPER_UNSANDBOXED;

  pepper_info->path = FilePath(webplugin_info.path);
  pepper_info->name = UTF16ToASCII(webplugin_info.name);
  pepper_info->description = UTF16ToASCII(webplugin_info.desc);
  pepper_info->version = UTF16ToASCII(webplugin_info.version);
  pepper_info->mime_types = webplugin_info.mime_types;
  pepper_info->permissions = webplugin_info.pepper_permissions;

  LOG(INFO) << "PepperPluginInfo::ToWebPluginInfo \""
            << UTF16ToUTF8(pepper_info->path.LossyDisplayName()) << "\" "
            << "permissions = " << pepper_info->permissions;

  return true;
}

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
void PepperPluginRegistry::ComputeList(
    std::vector<content::PepperPluginInfo>* plugins) {
  content::GetContentClient()->AddPepperPlugins(plugins);
  ComputePluginsFromCommandLine(plugins);
}

// static
void PepperPluginRegistry::PreloadModules() {
  std::vector<content::PepperPluginInfo> plugins;
  ComputeList(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (!plugins[i].is_internal && plugins[i].is_sandboxed) {
      std::string error;
      base::NativeLibrary library = base::LoadNativeLibrary(plugins[i].path,
                                                            &error);
      DLOG_IF(WARNING, !library) << "Unable to load plugin "
                                 << plugins[i].path.value() << " "
                                 << error;
      (void)library;  // Prevent release-mode warning.
    }
  }
}

const content::PepperPluginInfo* PepperPluginRegistry::GetInfoForPlugin(
    const webkit::WebPluginInfo& info) {
  for (size_t i = 0; i < plugin_list_.size(); ++i) {
    if (info.path == plugin_list_[i].path)
      return &plugin_list_[i];
  }
  // We did not find the plugin in our list. But wait! the plugin can also
  // be a latecomer, as it happens with pepper flash. This information
  // is actually in |info| and we can use it to construct it and add it to
  // the list. This same deal needs to be done in the browser side in
  // PluginService.
  content::PepperPluginInfo plugin;
  if (!MakePepperPluginInfo(info, &plugin))
    return NULL;

  plugin_list_.push_back(plugin);
  return &plugin_list_[plugin_list_.size() - 1];
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

void PepperPluginRegistry::PluginModuleDead(
    webkit::ppapi::PluginModule* dead_module) {
  // DANGER: Don't dereference the dead_module pointer! It may be in the
  // process of being deleted.

  // Modules aren't destroyed very often and there are normally at most a
  // couple of them. So for now we just do a brute-force search.
  for (NonOwningModuleMap::iterator i = live_modules_.begin();
       i != live_modules_.end(); ++i) {
    if (i->second == dead_module) {
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
    const content::PepperPluginInfo& current = plugin_list_[i];
    if (current.is_out_of_process)
      continue;  // Out of process plugins need no special pre-initialization.

    // TODO(brettw) bug 147507: Remove this logging.
    LOG(INFO) << "PepperPluginRegistry::PepperPluginRegistry \""
              << UTF16ToUTF8(current.path.LossyDisplayName()) << "\" "
              << " permissions =" << current.permissions;

    scoped_refptr<webkit::ppapi::PluginModule> module =
        new webkit::ppapi::PluginModule(current.name, current.path, this,
            ppapi::PpapiPermissions(current.permissions));
    AddLiveModule(current.path, module);
    if (current.is_internal) {
      if (!module->InitAsInternalPlugin(current.internal_entry_points)) {
        DLOG(ERROR) << "Failed to load pepper module: " << current.path.value();
        continue;
      }
    } else {
      // Preload all external plugins we're not running out of process.
      if (!module->InitAsLibrary(current.path)) {
        DLOG(ERROR) << "Failed to load pepper module: " << current.path.value();
        continue;
      }
    }
    preloaded_modules_[current.path] = module;
  }
}

