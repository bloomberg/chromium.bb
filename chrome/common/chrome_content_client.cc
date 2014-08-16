// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/debug/crash_logging.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "extensions/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

#include "flapper_version.h"  // In SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX)
#include "components/nacl/common/nacl_sandbox_type_mac.h"
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "ppapi/native_client/src/trusted/plugin/ppapi_entrypoints.h"
#endif

#if defined(ENABLE_REMOTING)
#include "remoting/client/plugin/pepper_entrypoints.h"
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) && \
    !defined(WIDEVINE_CDM_IS_COMPONENT)
#include "chrome/common/widevine_cdm_constants.h"
#endif

namespace {

const char kPDFPluginMimeType[] = "application/pdf";
const char kPDFPluginExtension[] = "pdf";
const char kPDFPluginDescription[] = "Portable Document Format";
const char kPDFPluginPrintPreviewMimeType[] =
   "application/x-google-chrome-print-preview-pdf";
const char kPDFPluginOutOfProcessMimeType[] =
   "application/x-google-chrome-pdf";
const uint32 kPDFPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                     ppapi::PERMISSION_DEV;

const char kO1DPluginName[] = "Google Talk Plugin Video Renderer";
const char kO1DPluginMimeType[] ="application/o1d";
const char kO1DPluginExtension[] = "";
const char kO1DPluginDescription[] = "Google Talk Plugin Video Renderer";
const uint32 kO1DPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                     ppapi::PERMISSION_DEV;

const char kEffectsPluginName[] = "Google Talk Effects Plugin";
const char kEffectsPluginMimeType[] ="application/x-ppapi-hangouts-effects";
const char kEffectsPluginExtension[] = "";
const char kEffectsPluginDescription[] = "Google Talk Effects Plugin";
const uint32 kEffectsPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                         ppapi::PERMISSION_DEV;

const char kGTalkPluginName[] = "Google Talk Plugin";
const char kGTalkPluginMimeType[] ="application/googletalk";
const char kGTalkPluginExtension[] = ".googletalk";
const char kGTalkPluginDescription[] = "Google Talk Plugin";
const uint32 kGTalkPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                       ppapi::PERMISSION_DEV;

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
// Use a consistent MIME-type regardless of branding.
const char kRemotingViewerPluginMimeType[] =
    "application/vnd.chromium.remoting-viewer";
const char kRemotingViewerPluginMimeExtension[] = "";
const char kRemotingViewerPluginMimeDescription[] = "";
const uint32 kRemotingViewerPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                                ppapi::PERMISSION_DEV;
#endif  // defined(ENABLE_REMOTING)

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
    if (skip_pdf_file_check || base::PathExists(path)) {
      content::PepperPluginInfo pdf;
      pdf.path = path;
      pdf.name = ChromeContentClient::kPDFPluginName;
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kOutOfProcessPdf)) {
        pdf.is_out_of_process = true;
        content::WebPluginMimeType pdf_mime_type(kPDFPluginOutOfProcessMimeType,
                                                 kPDFPluginExtension,
                                                 kPDFPluginDescription);
        pdf.mime_types.push_back(pdf_mime_type);
        // TODO(raymes): Make print preview work with out of process PDF.
      } else {
        content::WebPluginMimeType pdf_mime_type(kPDFPluginMimeType,
                                                 kPDFPluginExtension,
                                                 kPDFPluginDescription);
        content::WebPluginMimeType print_preview_pdf_mime_type(
            kPDFPluginPrintPreviewMimeType,
            kPDFPluginExtension,
            kPDFPluginDescription);
        pdf.mime_types.push_back(pdf_mime_type);
        pdf.mime_types.push_back(print_preview_pdf_mime_type);
      }
      pdf.permissions = kPDFPluginPermissions;
      plugins->push_back(pdf);

      skip_pdf_file_check = true;
    }
  }

#if !defined(DISABLE_NACL)
  // Handle Native Client just like the PDF plugin. This means that it is
  // enabled by default for the non-portable case.  This allows apps installed
  // from the Chrome Web Store to use NaCl even if the command line switch
  // isn't set.  For other uses of NaCl we check for the command line switch.
  if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
    content::PepperPluginInfo nacl;
    // The nacl plugin is now built into the Chromium binary.
    nacl.is_internal = true;
    nacl.path = path;
    nacl.name = nacl::kNaClPluginName;
    content::WebPluginMimeType nacl_mime_type(nacl::kNaClPluginMimeType,
                                              nacl::kNaClPluginExtension,
                                              nacl::kNaClPluginDescription);
    nacl.mime_types.push_back(nacl_mime_type);
    content::WebPluginMimeType pnacl_mime_type(nacl::kPnaclPluginMimeType,
                                               nacl::kPnaclPluginExtension,
                                               nacl::kPnaclPluginDescription);
    nacl.mime_types.push_back(pnacl_mime_type);
    nacl.internal_entry_points.get_interface = nacl_plugin::PPP_GetInterface;
    nacl.internal_entry_points.initialize_module =
        nacl_plugin::PPP_InitializeModule;
    nacl.internal_entry_points.shutdown_module =
        nacl_plugin::PPP_ShutdownModule;
    nacl.permissions = ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
    plugins->push_back(nacl);
  }
#endif  // !defined(DISABLE_NACL)

  static bool skip_o1d_file_check = false;
  if (PathService::Get(chrome::FILE_O1D_PLUGIN, &path)) {
    if (skip_o1d_file_check || base::PathExists(path)) {
      content::PepperPluginInfo o1d;
      o1d.path = path;
      o1d.name = kO1DPluginName;
      o1d.is_out_of_process = true;
      o1d.is_sandboxed = false;
      o1d.permissions = kO1DPluginPermissions;
      content::WebPluginMimeType o1d_mime_type(kO1DPluginMimeType,
                                               kO1DPluginExtension,
                                               kO1DPluginDescription);
      o1d.mime_types.push_back(o1d_mime_type);
      plugins->push_back(o1d);

      skip_o1d_file_check = true;
    }
  }

  // TODO(vrk): Remove this when NaCl effects plugin replaces the ppapi effects
  // plugin.
  static bool skip_effects_file_check = false;
  if (PathService::Get(chrome::FILE_EFFECTS_PLUGIN, &path)) {
    if (skip_effects_file_check || base::PathExists(path)) {
      content::PepperPluginInfo effects;
      effects.path = path;
      effects.name = kEffectsPluginName;
      effects.is_out_of_process = true;
      effects.is_sandboxed = true;
      effects.permissions = kEffectsPluginPermissions;
      content::WebPluginMimeType effects_mime_type(kEffectsPluginMimeType,
                                                   kEffectsPluginExtension,
                                                   kEffectsPluginDescription);
      effects.mime_types.push_back(effects_mime_type);
      plugins->push_back(effects);

      skip_effects_file_check = true;
    }
  }

  static bool skip_gtalk_file_check = false;
  if (PathService::Get(chrome::FILE_GTALK_PLUGIN, &path)) {
    if (skip_gtalk_file_check || base::PathExists(path)) {
      content::PepperPluginInfo gtalk;
      gtalk.path = path;
      gtalk.name = kGTalkPluginName;
      gtalk.is_out_of_process = true;
      gtalk.is_sandboxed = false;
      gtalk.permissions = kGTalkPluginPermissions;
      content::WebPluginMimeType gtalk_mime_type(kGTalkPluginMimeType,
                                                 kGTalkPluginExtension,
                                                 kGTalkPluginDescription);
      gtalk.mime_types.push_back(gtalk_mime_type);
      plugins->push_back(gtalk);

      skip_gtalk_file_check = true;
    }
  }

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) && \
    !defined(WIDEVINE_CDM_IS_COMPONENT)
  static bool skip_widevine_cdm_file_check = false;
  if (PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &path)) {
    if (skip_widevine_cdm_file_check || base::PathExists(path)) {
      content::PepperPluginInfo widevine_cdm;
      widevine_cdm.is_out_of_process = true;
      widevine_cdm.path = path;
      widevine_cdm.name = kWidevineCdmDisplayName;
      widevine_cdm.description = kWidevineCdmDescription;
      widevine_cdm.version = WIDEVINE_CDM_VERSION_STRING;
      content::WebPluginMimeType widevine_cdm_mime_type(
          kWidevineCdmPluginMimeType,
          kWidevineCdmPluginExtension,
          kWidevineCdmPluginMimeTypeDescription);

      // Add the supported codecs as if they came from the component manifest.
      std::vector<std::string> codecs;
      codecs.push_back(kCdmSupportedCodecVorbis);
      codecs.push_back(kCdmSupportedCodecVp8);
      codecs.push_back(kCdmSupportedCodecVp9);
#if defined(USE_PROPRIETARY_CODECS)
// TODO(ddorwin): Rename these macros to reflect their real meaning: whether the
// CDM Chrome was built [and shipped] with support these types.
#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
      codecs.push_back(kCdmSupportedCodecAac);
#endif
#if defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
      codecs.push_back(kCdmSupportedCodecAvc1);
#endif
#endif  // defined(USE_PROPRIETARY_CODECS)
      std::string codec_string =
          JoinString(codecs, kCdmSupportedCodecsValueDelimiter);
      widevine_cdm_mime_type.additional_param_names.push_back(
          base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
      widevine_cdm_mime_type.additional_param_values.push_back(
          base::ASCIIToUTF16(codec_string));

      widevine_cdm.mime_types.push_back(widevine_cdm_mime_type);
      widevine_cdm.permissions = kWidevineCdmPluginPermissions;
      plugins->push_back(widevine_cdm);

      skip_widevine_cdm_file_check = true;
    }
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) &&
        // !defined(WIDEVINE_CDM_IS_COMPONENT)

  // The Remoting Viewer plugin is built-in.
#if defined(ENABLE_REMOTING)
  content::PepperPluginInfo info;
  info.is_internal = true;
  info.is_out_of_process = true;
  info.name = kRemotingViewerPluginName;
  info.description = kRemotingViewerPluginDescription;
  info.path = base::FilePath::FromUTF8Unsafe(
      ChromeContentClient::kRemotingViewerPluginPath);
  content::WebPluginMimeType remoting_mime_type(
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

  plugin.is_out_of_process = true;
  plugin.name = content::kFlashPluginName;
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
  content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                           content::kFlashPluginSwfExtension,
                                           content::kFlashPluginSwfDescription);
  plugin.mime_types.push_back(swf_mime_type);
  content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                           content::kFlashPluginSplExtension,
                                           content::kFlashPluginSplDescription);
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

std::string GetProduct() {
  chrome::VersionInfo version_info;
  return version_info.is_valid() ?
      version_info.ProductNameAndVersionForUserAgent() : std::string();
}

}  // namespace

std::string GetUserAgent() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent))
    return command_line->GetSwitchValueASCII(switches::kUserAgent);

  std::string product = GetProduct();
#if defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
}

void ChromeContentClient::SetActiveURL(const GURL& url) {
  base::debug::SetCrashKeyValue(crash_keys::kActiveURL,
                                url.possibly_invalid_spec());
}

void ChromeContentClient::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
#if !defined(OS_ANDROID)
  base::debug::SetCrashKeyValue(crash_keys::kGPUVendorID,
      base::StringPrintf("0x%04x", gpu_info.gpu.vendor_id));
  base::debug::SetCrashKeyValue(crash_keys::kGPUDeviceID,
      base::StringPrintf("0x%04x", gpu_info.gpu.device_id));
#endif
  base::debug::SetCrashKeyValue(crash_keys::kGPUDriverVersion,
      gpu_info.driver_version);
  base::debug::SetCrashKeyValue(crash_keys::kGPUPixelShaderVersion,
      gpu_info.pixel_shader_version);
  base::debug::SetCrashKeyValue(crash_keys::kGPUVertexShaderVersion,
      gpu_info.vertex_shader_version);
#if defined(OS_MACOSX)
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLVersion, gpu_info.gl_version);
#elif defined(OS_POSIX)
  base::debug::SetCrashKeyValue(crash_keys::kGPUVendor, gpu_info.gl_vendor);
  base::debug::SetCrashKeyValue(crash_keys::kGPURenderer, gpu_info.gl_renderer);
#endif
}

void ChromeContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
  ComputeBuiltInPlugins(plugins);
  AddPepperFlashFromCommandLine(plugins);

  content::PepperPluginInfo plugin;
  if (GetBundledPepperFlash(&plugin))
    plugins->push_back(plugin);
}

void ChromeContentClient::AddAdditionalSchemes(
    std::vector<std::string>* standard_schemes,
    std::vector<std::string>* savable_schemes) {
  standard_schemes->push_back(extensions::kExtensionScheme);
  savable_schemes->push_back(extensions::kExtensionScheme);
  standard_schemes->push_back(chrome::kChromeNativeScheme);
  standard_schemes->push_back(extensions::kExtensionResourceScheme);
  savable_schemes->push_back(extensions::kExtensionResourceScheme);
  standard_schemes->push_back(chrome::kChromeSearchScheme);
  savable_schemes->push_back(chrome::kChromeSearchScheme);
  standard_schemes->push_back(dom_distiller::kDomDistillerScheme);
  savable_schemes->push_back(dom_distiller::kDomDistillerScheme);
#if defined(OS_CHROMEOS)
  standard_schemes->push_back(chrome::kCrosScheme);
#endif
}

std::string ChromeContentClient::GetProduct() const {
  return ::GetProduct();
}

std::string ChromeContentClient::GetUserAgent() const {
  return ::GetUserAgent();
}

base::string16 ChromeContentClient::GetLocalizedString(int message_id) const {
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
  switch (type) {
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
  if (sandbox_type == NACL_SANDBOX_TYPE_NACL_LOADER) {
    *sandbox_profile_resource_id = IDR_NACL_SANDBOX_PROFILE;
    return true;
  }
  return false;
}
#endif
