// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_process_type.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "grit/common_resources.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "remoting/client/plugin/pepper_entrypoints.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/plugin_constants.h"
#include "webkit/plugins/plugin_switches.h"
#include "webkit/user_agent/user_agent_util.h"

#include "flapper_version.h"  // In SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"
#elif defined(OS_MACOSX)
#include "chrome/common/chrome_sandbox_type_mac.h"
#endif

namespace {

const char kPDFPluginName[] = "Chrome PDF Viewer";
const char kPDFPluginMimeType[] = "application/pdf";
const char kPDFPluginExtension[] = "pdf";
const char kPDFPluginDescription[] = "Portable Document Format";
const char kPDFPluginPrintPreviewMimeType
   [] = "application/x-google-chrome-print-preview-pdf";
const uint32 kPDFPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                     ppapi::PERMISSION_DEV;

const char kNaClPluginName[] = "Native Client";
const char kNaClPluginMimeType[] = "application/x-nacl";
const char kNaClPluginExtension[] = "nexe";
const char kNaClPluginDescription[] = "Native Client Executable";
const uint32 kNaClPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                      ppapi::PERMISSION_DEV;

const char kNaClOldPluginName[] = "Chrome NaCl";

const char kO3DPluginName[] = "Google Talk Plugin Video Accelerator";
const char kO3DPluginMimeType[] ="application/vnd.o3d.auto";
const char kO3DPluginExtension[] = "";
const char kO3DPluginDescription[] = "O3D MIME";
const uint32 kO3DPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                     ppapi::PERMISSION_DEV;

const char kGTalkPluginName[] = "Google Talk Plugin";
const char kGTalkPluginMimeType[] ="application/googletalk";
const char kGTalkPluginExtension[] = ".googletalk";
const char kGTalkPluginDescription[] = "Google Talk Plugin";
const uint32 kGTalkPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                       ppapi::PERMISSION_DEV;

#if defined(WIDEVINE_CDM_AVAILABLE)
const char kWidevineCdmPluginExtension[] = "";
const uint32 kWidevineCdmPluginPermissions = ppapi::PERMISSION_PRIVATE |
#if defined(OS_CHROMEOS)
// TODO(xhwang): Make permission requirements the same on all OS.
// See http://crbug.com/222252
                                             ppapi::PERMISSION_FLASH |
#endif  // !defined(OS_CHROMEOS)
                                             ppapi::PERMISSION_DEV;
#endif  // WIDEVINE_CDM_AVAILABLE

#if defined(ENABLE_REMOTING)
#if defined(GOOGLE_CHROME_BUILD)
const char kRemotingViewerPluginName[] = "Chrome Remote Desktop Viewer";
#else
const char kRemotingViewerPluginName[] = "Chromoting Viewer";
#endif  // defined(GOOGLE_CHROME_BUILD)
const char kRemotingViewerPluginDescription[] =
    "This plugin allows you to securely access other computers that have been "
    "shared with you. To use this plugin you must first install the "
    "<a href=\"https://chrome.google.com/remotedesktop\">"
    "Chrome Remote Desktop</a> webapp.";
const base::FilePath::CharType kRemotingViewerPluginPath[] =
    FILE_PATH_LITERAL("internal-remoting-viewer");
// Use a consistent MIME-type regardless of branding.
const char kRemotingViewerPluginMimeType[] =
    "application/vnd.chromium.remoting-viewer";
const char kRemotingViewerPluginMimeExtension[] = "";
const char kRemotingViewerPluginMimeDescription[] = "";
const uint32 kRemotingViewerPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                                ppapi::PERMISSION_DEV;
#endif  // defined(ENABLE_REMOTING)

const char kInterposeLibraryPath[] =
    "@executable_path/../../../libplugin_carbon_interpose.dylib";

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<content::PepperPluginInfo>* plugins) {
  // PDF.
  //
  // Once we're sandboxed, we can't know if the PDF plugin is available or not;
  // but (on Linux) this function is always called once before we're sandboxed.
  // So the first time through test if the file is available and then skip the
  // check on subsequent calls if yes.
  static bool skip_pdf_file_check = false;
  base::FilePath path;
  if (PathService::Get(chrome::FILE_PDF_PLUGIN, &path)) {
    if (skip_pdf_file_check || file_util::PathExists(path)) {
      content::PepperPluginInfo pdf;
      pdf.path = path;
      pdf.name = kPDFPluginName;
      webkit::WebPluginMimeType pdf_mime_type(kPDFPluginMimeType,
                                              kPDFPluginExtension,
                                              kPDFPluginDescription);
      webkit::WebPluginMimeType print_preview_pdf_mime_type(
          kPDFPluginPrintPreviewMimeType,
          kPDFPluginExtension,
          kPDFPluginDescription);
      pdf.mime_types.push_back(pdf_mime_type);
      pdf.mime_types.push_back(print_preview_pdf_mime_type);
      pdf.permissions = kPDFPluginPermissions;
      plugins->push_back(pdf);

      skip_pdf_file_check = true;
    }
  }

  // Handle the Native Client just like the PDF plugin. This means that it is
  // enabled by default. This allows apps installed from the Chrome Web Store
  // to use NaCl even if the command line switch isn't set. For other uses of
  // NaCl we check for the command line switch.
  static bool skip_nacl_file_check = false;
  if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
    if (skip_nacl_file_check || file_util::PathExists(path)) {
      content::PepperPluginInfo nacl;
      nacl.path = path;
      nacl.name = kNaClPluginName;
      webkit::WebPluginMimeType nacl_mime_type(kNaClPluginMimeType,
                                               kNaClPluginExtension,
                                               kNaClPluginDescription);
      nacl.mime_types.push_back(nacl_mime_type);
      nacl.permissions = kNaClPluginPermissions;
      plugins->push_back(nacl);

      skip_nacl_file_check = true;
    }
  }

  static bool skip_o3d_file_check = false;
  if (PathService::Get(chrome::FILE_O3D_PLUGIN, &path)) {
    if (skip_o3d_file_check || file_util::PathExists(path)) {
      content::PepperPluginInfo o3d;
      o3d.path = path;
      o3d.name = kO3DPluginName;
      o3d.is_out_of_process = true;
      o3d.is_sandboxed = false;
      o3d.permissions = kO3DPluginPermissions;
      webkit::WebPluginMimeType o3d_mime_type(kO3DPluginMimeType,
                                              kO3DPluginExtension,
                                              kO3DPluginDescription);
      o3d.mime_types.push_back(o3d_mime_type);
      plugins->push_back(o3d);

      skip_o3d_file_check = true;
    }
  }

  static bool skip_gtalk_file_check = false;
  if (PathService::Get(chrome::FILE_GTALK_PLUGIN, &path)) {
    if (skip_gtalk_file_check || file_util::PathExists(path)) {
      content::PepperPluginInfo gtalk;
      gtalk.path = path;
      gtalk.name = kGTalkPluginName;
      gtalk.is_out_of_process = true;
      gtalk.is_sandboxed = false;
      gtalk.permissions = kGTalkPluginPermissions;
      webkit::WebPluginMimeType gtalk_mime_type(kGTalkPluginMimeType,
                                                kGTalkPluginExtension,
                                                kGTalkPluginDescription);
      gtalk.mime_types.push_back(gtalk_mime_type);
      plugins->push_back(gtalk);

      skip_gtalk_file_check = true;
    }
  }

#if defined(WIDEVINE_CDM_AVAILABLE)
  static bool skip_widevine_cdm_file_check = false;
  if (PathService::Get(chrome::FILE_WIDEVINE_CDM_PLUGIN, &path)) {
    if (skip_widevine_cdm_file_check || file_util::PathExists(path)) {
      content::PepperPluginInfo widevine_cdm;
      widevine_cdm.is_out_of_process = true;
      widevine_cdm.path = path;
      widevine_cdm.name = kWidevineCdmPluginName;
      widevine_cdm.description = kWidevineCdmPluginDescription;
      widevine_cdm.version = WIDEVINE_CDM_VERSION_STRING;
      webkit::WebPluginMimeType widevine_cdm_mime_type(
          kWidevineCdmPluginMimeType,
          kWidevineCdmPluginExtension,
          kWidevineCdmPluginMimeTypeDescription);
      widevine_cdm.mime_types.push_back(widevine_cdm_mime_type);
      widevine_cdm.permissions = kWidevineCdmPluginPermissions;
      plugins->push_back(widevine_cdm);

      skip_widevine_cdm_file_check = true;
    }
  }
#endif  // WIDEVINE_CDM_AVAILABLE

  // The Remoting Viewer plugin is built-in.
#if defined(ENABLE_REMOTING)
  content::PepperPluginInfo info;
  info.is_internal = true;
  info.is_out_of_process = true;
  info.name = kRemotingViewerPluginName;
  info.description = kRemotingViewerPluginDescription;
  info.path = base::FilePath(kRemotingViewerPluginPath);
  webkit::WebPluginMimeType remoting_mime_type(
      kRemotingViewerPluginMimeType,
      kRemotingViewerPluginMimeExtension,
      kRemotingViewerPluginMimeDescription);
  info.mime_types.push_back(remoting_mime_type);
  info.internal_entry_points.get_interface = remoting::PPP_GetInterface;
  info.internal_entry_points.initialize_module =
      remoting::PPP_InitializeModule;
  info.internal_entry_points.shutdown_module = remoting::PPP_ShutdownModule;
  info.permissions = kRemotingViewerPluginPermissions;

  plugins->push_back(info);
#endif
}

content::PepperPluginInfo CreatePepperFlashInfo(const base::FilePath& path,
                                                const std::string& version) {
  content::PepperPluginInfo plugin;

  // Flash being out of process is handled separately than general plugins
  // for testing purposes.
  plugin.is_out_of_process = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPpapiFlashInProcess);
  plugin.name = kFlashPluginName;
  plugin.path = path;
  plugin.permissions = kPepperFlashPermissions;

  std::vector<std::string> flash_version_numbers;
  base::SplitString(version, '.', &flash_version_numbers);
  if (flash_version_numbers.size() < 1)
    flash_version_numbers.push_back("11");
  // |SplitString()| puts in an empty string given an empty string. :(
  else if (flash_version_numbers[0].empty())
    flash_version_numbers[0] = "11";
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
  webkit::WebPluginMimeType swf_mime_type(kFlashPluginSwfMimeType,
                                          kFlashPluginSwfExtension,
                                          kFlashPluginSwfDescription);
  plugin.mime_types.push_back(swf_mime_type);
  webkit::WebPluginMimeType spl_mime_type(kFlashPluginSplMimeType,
                                          kFlashPluginSplExtension,
                                          kFlashPluginSplDescription);
  plugin.mime_types.push_back(spl_mime_type);

  return plugin;
}

void AddPepperFlashFromCommandLine(
    std::vector<content::PepperPluginInfo>* plugins) {
  const CommandLine::StringType flash_path =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath);
  if (flash_path.empty())
    return;

  // Also get the version from the command-line. Should be something like 11.2
  // or 11.2.123.45.
  std::string flash_version =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPpapiFlashVersion);

  plugins->push_back(
      CreatePepperFlashInfo(base::FilePath(flash_path), flash_version));
}

bool GetBundledPepperFlash(content::PepperPluginInfo* plugin) {
#if defined(FLAPPER_AVAILABLE)
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // Ignore bundled Pepper Flash if there is Pepper Flash specified from the
  // command-line.
  if (command_line->HasSwitch(switches::kPpapiFlashPath))
    return false;

  bool force_disable =
      command_line->HasSwitch(switches::kDisableBundledPpapiFlash);
  if (force_disable)
    return false;

// For Linux ia32, Flapper requires SSE2.
#if defined(OS_LINUX) && defined(ARCH_CPU_X86)
  if (!base::CPU().has_sse2())
    return false;
#endif  // ARCH_CPU_X86

  base::FilePath flash_path;
  if (!PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &flash_path))
    return false;

  *plugin = CreatePepperFlashInfo(flash_path, FLAPPER_VERSION_STRING);
  return true;
#else
  return false;
#endif  // FLAPPER_AVAILABLE
}

}  // namespace

namespace chrome {

const char* const ChromeContentClient::kPDFPluginName = ::kPDFPluginName;
const char* const ChromeContentClient::kNaClPluginName = ::kNaClPluginName;
const char* const ChromeContentClient::kNaClOldPluginName =
    ::kNaClOldPluginName;

void ChromeContentClient::SetActiveURL(const GURL& url) {
  child_process_logging::SetActiveURL(url);
}

void ChromeContentClient::SetGpuInfo(const content::GPUInfo& gpu_info) {
  child_process_logging::SetGpuInfo(gpu_info);
}

void ChromeContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
  ComputeBuiltInPlugins(plugins);
  AddPepperFlashFromCommandLine(plugins);

  content::PepperPluginInfo plugin;
  if (GetBundledPepperFlash(&plugin))
    plugins->push_back(plugin);
}

void ChromeContentClient::AddNPAPIPlugins(
    webkit::npapi::PluginList* plugin_list) {
}

void ChromeContentClient::AddAdditionalSchemes(
    std::vector<std::string>* standard_schemes,
    std::vector<std::string>* savable_schemes) {
  standard_schemes->push_back(extensions::kExtensionScheme);
  savable_schemes->push_back(extensions::kExtensionScheme);
  standard_schemes->push_back(kExtensionResourceScheme);
  savable_schemes->push_back(kExtensionResourceScheme);
  standard_schemes->push_back(chrome::kChromeSearchScheme);
  savable_schemes->push_back(chrome::kChromeSearchScheme);
#if defined(OS_CHROMEOS)
  standard_schemes->push_back(kCrosScheme);
#endif
}

bool ChromeContentClient::CanHandleWhileSwappedOut(
    const IPC::Message& msg) {
  // Any Chrome-specific messages (apart from those listed in
  // CanSendWhileSwappedOut) that must be handled by the browser when sent from
  // swapped out renderers.
  return false;
}

std::string ChromeContentClient::GetProduct() const {
  chrome::VersionInfo version_info;
  std::string product("Chrome/");
  product += version_info.is_valid() ? version_info.Version() : "0.0.0.0";
  return product;
}

std::string ChromeContentClient::GetUserAgent() const {
  std::string product = GetProduct();
#if defined(OS_ANDROID)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return webkit_glue::BuildUserAgentFromProduct(product);
}

string16 ChromeContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* ChromeContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

gfx::Image& ChromeContentClient::GetNativeImageNamed(int resource_id) const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
}

std::string ChromeContentClient::GetProcessTypeNameInEnglish(int type) {
  switch(type) {
    case PROCESS_TYPE_PROFILE_IMPORT:
      return "Profile Import helper";
    case PROCESS_TYPE_NACL_LOADER:
      return "Native Client module";
    case PROCESS_TYPE_NACL_BROKER:
      return "Native Client broker";
  }

  DCHECK(false) << "Unknown child process type!";
  return "Unknown"; 
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool ChromeContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  DCHECK(sandbox_profile_resource_id);
  if (sandbox_type == CHROME_SANDBOX_TYPE_NACL_LOADER) {
    *sandbox_profile_resource_id = IDR_NACL_SANDBOX_PROFILE;
    return true;
  }
  return false;
}

std::string ChromeContentClient::GetCarbonInterposePath() const {
  return std::string(kInterposeLibraryPath);
}
#endif

}  // namespace chrome
