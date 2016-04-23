// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "components/data_reduction_proxy/content/common/data_reduction_proxy_messages.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "extensions/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_LINUX)
#include <fcntl.h>
#include "chrome/common/component_flash_hint_file_linux.h"
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_sandbox_type.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/extensions/features/feature_util.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#include "flapper_version.h"  // nogncheck  In SHARED_INTERMEDIATE_DIR.
#include "ppapi/shared_impl/ppapi_permissions.h"
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) && \
    !defined(WIDEVINE_CDM_IS_COMPONENT)
#define WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT
#include "chrome/common/widevine_cdm_constants.h"
#endif

namespace {

#if defined(ENABLE_PLUGINS)
#if defined(ENABLE_PDF)
const char kPDFPluginExtension[] = "pdf";
const char kPDFPluginDescription[] = "Portable Document Format";
const char kPDFPluginOutOfProcessMimeType[] =
    "application/x-google-chrome-pdf";
const uint32_t kPDFPluginPermissions =
    ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
#endif  // defined(ENABLE_PDF)

content::PepperPluginInfo::GetInterfaceFunc g_pdf_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_pdf_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_pdf_shutdown_module;

#if !defined(DISABLE_NACL)
content::PepperPluginInfo::GetInterfaceFunc g_nacl_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_nacl_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_nacl_shutdown_module;
#endif

#if defined(WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT)
bool IsWidevineAvailable(base::FilePath* adapter_path,
                         base::FilePath* cdm_path,
                         std::vector<std::string>* codecs_supported) {
  static enum {
    NOT_CHECKED,
    FOUND,
    NOT_FOUND,
  } widevine_cdm_file_check = NOT_CHECKED;
  // TODO(jrummell): We should add a new path for DIR_WIDEVINE_CDM and use that
  // to locate the CDM and the CDM adapter.
  if (PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, adapter_path)) {
    *cdm_path = adapter_path->DirName().AppendASCII(kWidevineCdmFileName);
    if (widevine_cdm_file_check == NOT_CHECKED) {
      widevine_cdm_file_check =
          (base::PathExists(*adapter_path) && base::PathExists(*cdm_path))
              ? FOUND
              : NOT_FOUND;
    }
    if (widevine_cdm_file_check == FOUND) {
      // Add the supported codecs as if they came from the component manifest.
      // This list must match the CDM that is being bundled with Chrome.
      codecs_supported->push_back(kCdmSupportedCodecVp8);
      codecs_supported->push_back(kCdmSupportedCodecVp9);
#if defined(USE_PROPRIETARY_CODECS)
      codecs_supported->push_back(kCdmSupportedCodecAvc1);
#endif  // defined(USE_PROPRIETARY_CODECS)
      return true;
    }
  }

  return false;
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT)

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<content::PepperPluginInfo>* plugins) {
#if defined(ENABLE_PDF)
  content::PepperPluginInfo pdf_info;
  pdf_info.is_internal = true;
  pdf_info.is_out_of_process = true;
  pdf_info.name = ChromeContentClient::kPDFPluginName;
  pdf_info.description = kPDFPluginDescription;
  pdf_info.path = base::FilePath::FromUTF8Unsafe(
      ChromeContentClient::kPDFPluginPath);
  content::WebPluginMimeType pdf_mime_type(
      kPDFPluginOutOfProcessMimeType,
      kPDFPluginExtension,
      kPDFPluginDescription);
  pdf_info.mime_types.push_back(pdf_mime_type);
  pdf_info.internal_entry_points.get_interface = g_pdf_get_interface;
  pdf_info.internal_entry_points.initialize_module = g_pdf_initialize_module;
  pdf_info.internal_entry_points.shutdown_module = g_pdf_shutdown_module;
  pdf_info.permissions = kPDFPluginPermissions;
  plugins->push_back(pdf_info);
#endif  // defined(ENABLE_PDF)

#if !defined(DISABLE_NACL)
  // Handle Native Client just like the PDF plugin. This means that it is
  // enabled by default for the non-portable case.  This allows apps installed
  // from the Chrome Web Store to use NaCl even if the command line switch
  // isn't set.  For other uses of NaCl we check for the command line switch.
  base::FilePath path;
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
    nacl.internal_entry_points.get_interface = g_nacl_get_interface;
    nacl.internal_entry_points.initialize_module = g_nacl_initialize_module;
    nacl.internal_entry_points.shutdown_module = g_nacl_shutdown_module;
    nacl.permissions = ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
    plugins->push_back(nacl);
  }
#endif  // !defined(DISABLE_NACL)

#if defined(WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT)
  base::FilePath adapter_path;
  base::FilePath cdm_path;
  std::vector<std::string> codecs_supported;
  if (IsWidevineAvailable(&adapter_path, &cdm_path, &codecs_supported)) {
    content::PepperPluginInfo widevine_cdm;
    widevine_cdm.is_out_of_process = true;
    widevine_cdm.path = adapter_path;
    widevine_cdm.name = kWidevineCdmDisplayName;
    widevine_cdm.description =
        base::StringPrintf("%s (version: " WIDEVINE_CDM_VERSION_STRING ")",
                           kWidevineCdmDescription);
    widevine_cdm.version = WIDEVINE_CDM_VERSION_STRING;
    content::WebPluginMimeType widevine_cdm_mime_type(
        kWidevineCdmPluginMimeType, kWidevineCdmPluginExtension,
        kWidevineCdmPluginMimeTypeDescription);

    widevine_cdm_mime_type.additional_param_names.push_back(
        base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
    widevine_cdm_mime_type.additional_param_values.push_back(base::ASCIIToUTF16(
        base::JoinString(codecs_supported,
                         std::string(1, kCdmSupportedCodecsValueDelimiter))));

    widevine_cdm.mime_types.push_back(widevine_cdm_mime_type);
    widevine_cdm.permissions = kWidevineCdmPluginPermissions;
    plugins->push_back(widevine_cdm);
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT)
}

// Creates a PepperPluginInfo for the specified plugin.
// |path| is the full path to the plugin.
// |version| is a string representation of the plugin version.
// |is_debug| is whether the plugin is the debug version or not.
// |is_external| is whether the plugin is supplied external to Chrome e.g. a
//     system installation of Adobe Flash.
// |is_bundled| distinguishes between component updated plugin and a bundled
//     plugin.
content::PepperPluginInfo CreatePepperFlashInfo(const base::FilePath& path,
                                                const std::string& version,
                                                bool is_debug,
                                                bool is_external,
                                                bool is_bundled) {
  content::PepperPluginInfo plugin;

  plugin.is_out_of_process = true;
  plugin.name = content::kFlashPluginName;
  plugin.path = path;
#if defined(OS_WIN)
  plugin.is_on_local_drive = !base::IsOnNetworkDrive(path);
#endif
  plugin.permissions = chrome::kPepperFlashPermissions;
  plugin.is_debug = is_debug;
  plugin.is_external = is_external;
  plugin.is_bundled = is_bundled;

  std::vector<std::string> flash_version_numbers = base::SplitString(
      version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (flash_version_numbers.size() < 1)
    flash_version_numbers.push_back("11");
  if (flash_version_numbers.size() < 2)
    flash_version_numbers.push_back("2");
  if (flash_version_numbers.size() < 3)
    flash_version_numbers.push_back("999");
  if (flash_version_numbers.size() < 4)
    flash_version_numbers.push_back("999");
  // E.g., "Shockwave Flash 10.2 r154":
  plugin.description = plugin.name + " " + flash_version_numbers[0] + "." +
      flash_version_numbers[1] + " r" + flash_version_numbers[2];
  plugin.version = base::JoinString(flash_version_numbers, ".");
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
  const base::CommandLine::StringType flash_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath);
  if (flash_path.empty())
    return;

  // Also get the version from the command-line. Should be something like 11.2
  // or 11.2.123.45.
  std::string flash_version =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPpapiFlashVersion);

  plugins->push_back(
      CreatePepperFlashInfo(base::FilePath(flash_path),
                            flash_version, false, true, false));
}

#if defined(OS_LINUX)
// This function tests if DIR_USER_DATA can be accessed, as a simple check to
// see if the zygote has been sandboxed at this point.
bool IsUserDataDirAvailable() {
  base::FilePath user_data_dir;
  return PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
}

// This method is used on Linux only because of architectural differences in how
// it loads the component updated flash plugin, and not because the other
// platforms do not support component updated flash. On other platforms, the
// component updater sends an IPC message to all threads, at undefined points in
// time, with the URL of the component updated flash. Because the linux zygote
// thread has no access to the file system after it warms up, it must preload
// the component updated flash.
bool GetComponentUpdatedPepperFlash(content::PepperPluginInfo* plugin) {
#if defined(FLAPPER_AVAILABLE)
  if (component_flash_hint_file::DoesHintFileExist()) {
    base::FilePath flash_path;
    std::string version;
    if (component_flash_hint_file::VerifyAndReturnFlashLocation(&flash_path,
                                                                &version)) {
      // Test if the file can be mapped as executable. If the user's home
      // directory is mounted noexec, the component flash plugin will not load.
      // By testing for this, Chrome can fallback to the bundled flash plugin.
      if (!component_flash_hint_file::TestExecutableMapping(flash_path)) {
        LOG(WARNING) << "The component updated flash plugin could not be "
                        "mapped as executable. Attempting to fallback to the "
                        "bundled or system plugin.";
        return false;
      }
      *plugin = CreatePepperFlashInfo(flash_path, version, false, false, false);
      return true;
    }
    LOG(ERROR)
        << "Failed to locate and load the component updated flash plugin.";
  }
#endif  // defined(FLAPPER_AVAILABLE)
  return false;
}
#endif  // defined(OS_LINUX)

bool GetBundledPepperFlash(content::PepperPluginInfo* plugin) {
#if defined(FLAPPER_AVAILABLE)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Ignore bundled Pepper Flash if there is Pepper Flash specified from the
  // command-line.
  if (command_line->HasSwitch(switches::kPpapiFlashPath))
    return false;

  bool force_disable =
      command_line->HasSwitch(switches::kDisableBundledPpapiFlash);
  if (force_disable)
    return false;

  base::FilePath flash_path;
  if (!PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &flash_path))
    return false;

  *plugin = CreatePepperFlashInfo(flash_path, FLAPPER_VERSION_STRING, false,
                                  false, true);
  return true;
#else
  return false;
#endif  // FLAPPER_AVAILABLE
}

bool GetSystemPepperFlash(content::PepperPluginInfo* plugin) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Do not try and find System Pepper Flash if there is a specific path on
  // the commmand-line.
  if (command_line->HasSwitch(switches::kPpapiFlashPath))
    return false;

  base::FilePath flash_filename;
  if (!PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
                        &flash_filename))
    return false;

  if (!base::PathExists(flash_filename))
    return false;

  base::FilePath manifest_path(
      flash_filename.DirName().AppendASCII("manifest.json"));

  std::string manifest_data;
  if (!base::ReadFileToString(manifest_path, &manifest_data))
    return false;
  std::unique_ptr<base::Value> manifest_value(
      base::JSONReader::Read(manifest_data, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!manifest_value.get())
    return false;
  base::DictionaryValue* manifest = NULL;
  if (!manifest_value->GetAsDictionary(&manifest))
    return false;

  Version version;
  if (!chrome::CheckPepperFlashManifest(*manifest, &version))
    return false;

  *plugin = CreatePepperFlashInfo(flash_filename,
                                  version.GetString(),
                                  chrome::IsSystemFlashScriptDebuggerPresent(),
                                  true,
                                  false);
  return true;
}
#endif  //  defined(ENABLE_PLUGINS)

std::string GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

}  // namespace

std::string GetUserAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      return ua;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  std::string product = GetProduct();
#if defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
}

#if !defined(DISABLE_NACL)
void ChromeContentClient::SetNaClEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_nacl_get_interface = get_interface;
  g_nacl_initialize_module = initialize_module;
  g_nacl_shutdown_module = shutdown_module;
}
#endif

#if defined(ENABLE_PLUGINS)
void ChromeContentClient::SetPDFEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_pdf_get_interface = get_interface;
  g_pdf_initialize_module = initialize_module;
  g_pdf_shutdown_module = shutdown_module;
}
#endif

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

#if defined(ENABLE_PLUGINS)
// static
content::PepperPluginInfo* ChromeContentClient::FindMostRecentPlugin(
    const std::vector<content::PepperPluginInfo*>& plugins) {
  if (plugins.empty())
    return nullptr;

  using PluginSortKey = std::tuple<base::Version, bool, bool, bool, bool>;

  std::map<PluginSortKey, content::PepperPluginInfo*> plugin_map;

  for (const auto& plugin : plugins) {
    Version version(plugin->version);
    DCHECK(version.IsValid());
    plugin_map[PluginSortKey(version, plugin->is_debug,
                             plugin->is_bundled, plugin->is_on_local_drive,
                             !plugin->is_external)] = plugin;
  }

  return plugin_map.rbegin()->second;
}
#endif  // defined(ENABLE_PLUGINS)

void ChromeContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
#if defined(ENABLE_PLUGINS)
  ComputeBuiltInPlugins(plugins);
  AddPepperFlashFromCommandLine(plugins);

#if defined(OS_LINUX)
  // Depending on the sandbox configurtion, the user data directory
  // is not always available. If it is not available, do not try and load any
  // flash plugin. The flash player, if any, preloaded before the sandbox
  // initialization will continue to be used.
  if (!IsUserDataDirAvailable()) {
    return;
  }
#endif  // defined(OS_LINUX)

  ScopedVector<content::PepperPluginInfo> flash_versions;

#if defined(OS_LINUX)
  std::unique_ptr<content::PepperPluginInfo> component_flash(
      new content::PepperPluginInfo);
  if (GetComponentUpdatedPepperFlash(component_flash.get()))
    flash_versions.push_back(component_flash.release());
#endif  // defined(OS_LINUX)

  std::unique_ptr<content::PepperPluginInfo> bundled_flash(
      new content::PepperPluginInfo);
  if (GetBundledPepperFlash(bundled_flash.get()))
    flash_versions.push_back(bundled_flash.release());

  std::unique_ptr<content::PepperPluginInfo> system_flash(
      new content::PepperPluginInfo);
  if (GetSystemPepperFlash(system_flash.get()))
    flash_versions.push_back(system_flash.release());

  // This function will return only the most recent version of the flash plugin.
  content::PepperPluginInfo* max_flash =
      FindMostRecentPlugin(flash_versions.get());
  if (max_flash)
    plugins->push_back(*max_flash);
#endif  // defined(ENABLE_PLUGINS)
}

void ChromeContentClient::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms) {
// TODO(jrummell): Need to have a better flag to indicate systems Widevine
// is available on. For now we continue to use ENABLE_PEPPER_CDMS so that
// we can experiment between pepper and mojo.
#if defined(WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT)
  base::FilePath adapter_path;
  base::FilePath cdm_path;
  std::vector<std::string> codecs_supported;
  if (IsWidevineAvailable(&adapter_path, &cdm_path, &codecs_supported)) {
    // CdmInfo needs |path| to be the actual Widevine library,
    // not the adapter, so adjust as necessary. It will be in the
    // same directory as the installed adapter.
    const base::Version version(WIDEVINE_CDM_VERSION_STRING);
    DCHECK(version.IsValid());
    cdms->push_back(content::CdmInfo(kWidevineCdmType, version, cdm_path,
                                     codecs_supported));
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE_NOT_COMPONENT)

  // TODO(jrummell): Add External Clear Key CDM for testing, if it's available.
}

#if defined(OS_CHROMEOS)
static const int kNumChromeStandardURLSchemes = 6;
#else
static const int kNumChromeStandardURLSchemes = 5;
#endif
static const url::SchemeWithType kChromeStandardURLSchemes[
    kNumChromeStandardURLSchemes] = {
  {extensions::kExtensionScheme, url::SCHEME_WITHOUT_PORT},
  {chrome::kChromeNativeScheme, url::SCHEME_WITHOUT_PORT},
  {extensions::kExtensionResourceScheme, url::SCHEME_WITHOUT_PORT},
  {chrome::kChromeSearchScheme, url::SCHEME_WITHOUT_PORT},
  {dom_distiller::kDomDistillerScheme, url::SCHEME_WITHOUT_PORT},
#if defined(OS_CHROMEOS)
  {chrome::kCrosScheme, url::SCHEME_WITHOUT_PORT},
#endif
};

void ChromeContentClient::AddAdditionalSchemes(
    std::vector<url::SchemeWithType>* standard_schemes,
    std::vector<url::SchemeWithType>* referrer_schemes,
    std::vector<std::string>* savable_schemes) {
  for (int i = 0; i < kNumChromeStandardURLSchemes; i++)
    standard_schemes->push_back(kChromeStandardURLSchemes[i]);

#if defined(OS_ANDROID)
  referrer_schemes->push_back(
      {chrome::kAndroidAppScheme, url::SCHEME_WITHOUT_PORT});
#endif

  savable_schemes->push_back(extensions::kExtensionScheme);
  savable_schemes->push_back(extensions::kExtensionResourceScheme);
  savable_schemes->push_back(chrome::kChromeSearchScheme);
  savable_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

bool ChromeContentClient::CanSendWhileSwappedOut(const IPC::Message* message) {
  return message->type() ==
         DataReductionProxyViewHostMsg_IsDataReductionProxy::ID;
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
#if !defined(DISABLE_NACL)
  switch (type) {
    case PROCESS_TYPE_NACL_LOADER:
      return "Native Client module";
    case PROCESS_TYPE_NACL_BROKER:
      return "Native Client broker";
  }
#endif

  NOTREACHED() << "Unknown child process type!";
  return "Unknown";
}

#if defined(OS_MACOSX)
bool ChromeContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  DCHECK(sandbox_profile_resource_id);
#if !defined(DISABLE_NACL)
  if (sandbox_type == NACL_SANDBOX_TYPE_NACL_LOADER) {
    *sandbox_profile_resource_id = IDR_NACL_SANDBOX_PROFILE;
    return true;
  }
#endif
  return false;
}
#endif

void ChromeContentClient::AddSecureSchemesAndOrigins(
    std::set<std::string>* schemes,
    std::set<GURL>* origins) {
  schemes->insert(chrome::kChromeSearchScheme);
  schemes->insert(content::kChromeUIScheme);
  schemes->insert(extensions::kExtensionScheme);
  schemes->insert(extensions::kExtensionResourceScheme);
  GetSecureOriginWhitelist(origins);
}

void ChromeContentClient::AddServiceWorkerSchemes(
    std::set<std::string>* schemes) {
#if defined(ENABLE_EXTENSIONS)
  if (extensions::feature_util::ExtensionServiceWorkersEnabled())
    schemes->insert(extensions::kExtensionScheme);
#endif
}

bool ChromeContentClient::IsSupplementarySiteIsolationModeEnabled() {
#if defined(ENABLE_EXTENSIONS)
  return extensions::IsIsolateExtensionsEnabled();
#else
  return false;
#endif
}

base::StringPiece ChromeContentClient::GetOriginTrialPublicKey() {
  return origin_trial_key_manager_.GetPublicKey();
}
