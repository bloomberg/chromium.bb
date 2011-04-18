// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/pepper_plugin_registry.h"
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

#if defined(ENABLE_REMOTING)
const char* kRemotingPluginMimeType = "pepper-application/x-chromoting";
#endif

const char* kFlashPluginName = "Shockwave Flash";
const char* kFlashPluginSwfMimeType = "application/x-shockwave-flash";
const char* kFlashPluginSwfExtension = "swf";
const char* kFlashPluginSwfDescription = "Shockwave Flash";
const char* kFlashPluginSplMimeType = "application/futuresplash";
const char* kFlashPluginSplExtension = "spl";
const char* kFlashPluginSplDescription = "FutureSplash Player";

#if !defined(NACL_WIN64)  // The code this needs isn't linked on Win64 builds.

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
      webkit::npapi::WebPluginMimeType pdf_mime_type(kPDFPluginMimeType,
                                                     kPDFPluginExtension,
                                                     kPDFPluginDescription);
      pdf.mime_types.push_back(pdf_mime_type);
      plugins->push_back(pdf);

      skip_pdf_file_check = true;
    }
  }

  // Handle the Native Client plugin just like the PDF plugin.
  static bool skip_nacl_file_check = false;
  if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
    if (skip_nacl_file_check || file_util::PathExists(path)) {
      PepperPluginInfo nacl;
      nacl.path = path;
      nacl.name = kNaClPluginName;
      // Enable the Native Client Plugin based on the command line.
      nacl.enabled = CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNaCl);
      webkit::npapi::WebPluginMimeType nacl_mime_type(kNaClPluginMimeType,
                                                      kNaClPluginExtension,
                                                      kNaClPluginDescription);
      nacl.mime_types.push_back(nacl_mime_type);
      plugins->push_back(nacl);

      skip_nacl_file_check = true;
    }
  }

  // Remoting.
#if defined(ENABLE_REMOTING)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRemoting)) {
    PepperPluginInfo info;
    info.is_internal = true;
    info.path = FilePath(FILE_PATH_LITERAL("internal-chromoting"));
    webkit::npapi::WebPluginMimeType remoting_mime_type(kRemotingPluginMimeType,
                                                        std::string(),
                                                        std::string());
    info.mime_types.push_back(remoting_mime_type);
    info.internal_entry_points.get_interface = remoting::PPP_GetInterface;
    info.internal_entry_points.initialize_module =
        remoting::PPP_InitializeModule;
    info.internal_entry_points.shutdown_module = remoting::PPP_ShutdownModule;

    plugins->push_back(info);
  }
#endif
}

void AddOutOfProcessFlash(std::vector<PepperPluginInfo>* plugins) {
  // Flash being out of process is handled separately than general plugins
  // for testing purposes.
  bool flash_out_of_process = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPpapiFlashInProcess);

  // Handle any Pepper Flash first.
  const CommandLine::StringType flash_path =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath);
  if (flash_path.empty())
    return;

  PepperPluginInfo plugin;
  plugin.is_out_of_process = flash_out_of_process;
  plugin.path = FilePath(flash_path);
  plugin.name = kFlashPluginName;

  const std::string flash_version =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kPpapiFlashVersion);
  std::vector<std::string> flash_version_numbers;
  base::SplitString(flash_version, '.', &flash_version_numbers);
  if (flash_version_numbers.size() < 1)
    flash_version_numbers.push_back("10");
  // |SplitString()| puts in an empty string given an empty string. :(
  else if (flash_version_numbers[0].empty())
    flash_version_numbers[0] = "10";
  if (flash_version_numbers.size() < 2)
    flash_version_numbers.push_back("2");
  if (flash_version_numbers.size() < 3)
    flash_version_numbers.push_back("999");
  if (flash_version_numbers.size() < 4)
    flash_version_numbers.push_back("999");
  // E.g., "Shockwave Flash 10.2 r154":
  plugin.description = plugin.name + " " + flash_version_numbers[0] + "." +
      flash_version_numbers[1] + " r" + flash_version_numbers[2];
  plugin.version = JoinString(flash_version_numbers, '.');
  webkit::npapi::WebPluginMimeType swf_mime_type(kFlashPluginSwfMimeType,
                                                 kFlashPluginSwfExtension,
                                                 kFlashPluginSwfDescription);
  plugin.mime_types.push_back(swf_mime_type);
  webkit::npapi::WebPluginMimeType spl_mime_type(kFlashPluginSplMimeType,
                                                 kFlashPluginSplExtension,
                                                 kFlashPluginSplDescription);
  plugin.mime_types.push_back(spl_mime_type);
  plugins->push_back(plugin);
}

#endif  // !defined(NACL_WIN64)

}  // namespace

namespace chrome {

const char* ChromeContentClient::kPDFPluginName = ::kPDFPluginName;

void ChromeContentClient::SetActiveURL(const GURL& url) {
  child_process_logging::SetActiveURL(url);
}

void ChromeContentClient::SetGpuInfo(const GPUInfo& gpu_info) {
  child_process_logging::SetGpuInfo(gpu_info);
}

void ChromeContentClient::AddPepperPlugins(
    std::vector<PepperPluginInfo>* plugins) {
#if !defined(NACL_WIN64)  // The code this needs isn't linked on Win64 builds.
  ComputeBuiltInPlugins(plugins);
  AddOutOfProcessFlash(plugins);
#endif
}

}  // namespace chrome
