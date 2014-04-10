// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_net_benchmarking_message_filter.h"
#include "chrome/browser/chrome_quota_permission_context.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/browser_permissions_policy_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/geolocation/chrome_access_token_store.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/guestview/adview/adview_guest.h"
#include "chrome/browser/guestview/guestview.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/browser/local_discovery/storage/privet_filesystem_backend.h"
#include "chrome/browser/media/cast_transport_host_filter.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugins/plugin_info_message_filter.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_message_filter.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/printing/printing_message_filter.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_provider_install_state_message_filter.h"
#include "chrome/browser/signin/principals_message_filter.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/speech/tts_message_filter.h"
#include "chrome/browser/ssl/ssl_add_certificate.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_tab_helper.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profile_management_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/chromeos_constants.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/switches.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center_util.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/common/webpreferences.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_main_mac.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter_mac.h"
#include "components/breakpad/app/breakpad_mac.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#include "chrome/browser/chromeos/drive/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chromeos/chromeos_switches.h"
#elif defined(OS_LINUX)
#include "chrome/browser/chrome_browser_main_linux.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/android/new_tab_page_url_handler.h"
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "chrome/browser/chrome_browser_main_android.h"
#include "chrome/browser/media/encrypted_media_message_filter_android.h"
#include "chrome/common/descriptors_android.h"
#include "components/breakpad/browser/crash_dump_manager_android.h"
#elif defined(OS_POSIX)
#include "chrome/browser/chrome_browser_main_posix.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/debug/leak_annotations.h"
#include "base/linux_util.h"
#include "components/breakpad/app/breakpad_linux.h"
#include "components/breakpad/browser/crash_handler_host_linux.h"
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if defined(OS_ANDROID)
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/android/device_display_info.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "chrome/browser/media/webrtc_logging_handler_host.h"
#endif

#if defined(ENABLE_INPUT_SPEECH)
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate_bubble_ui.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/app_id.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/chrome_browser_main_extra_parts_gtk.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/aura/chrome_browser_main_extra_parts_aura.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/chrome_browser_main_extra_parts_x11.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/local_discovery/storage/privet_filesystem_backend.h"
#endif

using blink::WebWindowFeatures;
using base::FileDescriptor;
using content::AccessTokenStore;
using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::ChildProcessSecurityPolicy;
using content::QuotaPermissionContext;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using extensions::APIPermission;
using extensions::Extension;
using extensions::InfoMap;
using extensions::Manifest;
using message_center::NotifierId;

#if defined(OS_POSIX)
using content::FileDescriptorInfo;
#endif

namespace {

// Cached version of the locale so we can return the locale on the I/O
// thread.
base::LazyInstance<std::string> g_io_thread_application_locale;

#if defined(ENABLE_PLUGINS)
const char* kPredefinedAllowedFileHandleOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/234789
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see crbug.com/234789
};

const char* kPredefinedAllowedSocketOrigins[] = {
  "okddffdblfhhnmhodogpojmfkjmhinfp",  // Test SSH Client
  "pnhechapfaindjhompbnflcldabbghjo",  // HTerm App (SSH Client)
  "bglhmjfplikpjnfoegeomebmfnkjomhe",  // see crbug.com/122126
  "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
  "kgngmbheleoaphbjbaiobfdepmghbfah",  // Pre-release Chrome Remote Desktop
  "odkaodonbgfohohmklejpjiejmcipmib",  // Dogfood Chrome Remote Desktop
  "ojoimpklfciegopdfgeenehpalipignm",  // Chromoting canary
  "cbkkbcmdlboombapidmoeolnmdacpkch",  // see crbug.com/129089
  "hhnbmknkdabfoieppbbljkhkfjcmcbjh",  // see crbug.com/134099
  "mablfbjkhmhkmefkjjacnbaikjkipphg",  // see crbug.com/134099
  "pdeelgamlgannhelgoegilelnnojegoh",  // see crbug.com/134099
  "cabapfdbkniadpollkckdnedaanlciaj",  // see crbug.com/134099
  "mapljbgnjledlpdmlchihnmeclmefbba",  // see crbug.com/134099
  "ghbfeebgmiidnnmeobbbaiamklmpbpii",  // see crbug.com/134099
  "jdfhpkjeckflbbleddjlpimecpbjdeep",  // see crbug.com/142514
  "iabmpiboiopbgfabjmgeedhcmjenhbla",  // see crbug.com/165080
  "B7CF8A292249681AF81771650BA4CEEAF19A4560",  // see crbug.com/165080
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/234789
  "4EB74897CB187C7633357C2FE832E0AD6A44883A",  // see crbug.com/234789
  "7525AF4F66763A70A883C4700529F647B470E4D2",  // see crbug.com/238084
  "0B549507088E1564D672F7942EB87CA4DAD73972",  // see crbug.com/238084
  "864288364E239573E777D3E0E36864E590E95C74"   // see crbug.com/238084
};
#endif

// Returns a copy of the given url with its host set to given host and path set
// to given path. Other parts of the url will be the same.
GURL ReplaceURLHostAndPath(const GURL& url,
                           const std::string& host,
                           const std::string& path) {
  url_canon::Replacements<char> replacements;
  replacements.SetHost(host.c_str(),
                       url_parse::Component(0, host.length()));
  replacements.SetPath(path.c_str(),
                       url_parse::Component(0, path.length()));
  return url.ReplaceComponents(replacements);
}

// Maps "foo://bar/baz/" to "foo://chrome/bar/baz/".
GURL AddUberHost(const GURL& url) {
  const std::string uber_host = chrome::kChromeUIUberHost;
  const std::string new_path = url.host() + url.path();

  return ReplaceURLHostAndPath(url, uber_host, new_path);
}

// If url->host() is "chrome" and url->path() has characters other than the
// first slash, changes the url from "foo://chrome/bar/" to "foo://bar/" and
// returns true. Otherwise returns false.
bool RemoveUberHost(GURL* url) {
  if (url->host() != chrome::kChromeUIUberHost)
    return false;

  if (url->path().empty() || url->path() == "/")
    return false;

  const std::string old_path = url->path();

  const std::string::size_type separator = old_path.find('/', 1);
  std::string new_host;
  std::string new_path;
  if (separator == std::string::npos) {
    new_host = old_path.substr(1);
  } else {
    new_host = old_path.substr(1, separator - 1);
    new_path = old_path.substr(separator);
  }

  // Do not allow URLs with paths empty before the first slash since we can't
  // have an empty host. (e.g "foo://chrome//")
  if (new_host.empty())
    return false;

  *url = ReplaceURLHostAndPath(*url, new_host, new_path);

  DCHECK(url->is_valid());

  return true;
}

// Handles rewriting Web UI URLs.
bool HandleWebUI(GURL* url, content::BrowserContext* browser_context) {
  // Do not handle special URLs such as "about:foo"
  if (!url->host().empty()) {
    const GURL chrome_url = AddUberHost(*url);

    // Handle valid "chrome://chrome/foo" URLs so the reverse handler will
    // be called.
    if (ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
            browser_context, chrome_url))
      return true;
  }

  if (!ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
          browser_context, *url))
    return false;

#if defined(OS_CHROMEOS)
  // Special case : in ChromeOS in Guest mode bookmarks and history are
  // disabled for security reasons. New tab page explains the reasons, so
  // we redirect user to new tab page.
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest()) {
    if (url->SchemeIs(content::kChromeUIScheme) &&
        (url->DomainIs(chrome::kChromeUIBookmarksHost) ||
         url->DomainIs(chrome::kChromeUIHistoryHost))) {
      // Rewrite with new tab URL
      *url = GURL(chrome::kChromeUINewTabURL);
    }
  }
#endif

  return true;
}

// Reverse URL handler for Web UI. Maps "chrome://chrome/foo/" to
// "chrome://foo/".
bool HandleWebUIReverse(GURL* url, content::BrowserContext* browser_context) {
  if (!url->is_valid() || !url->SchemeIs(content::kChromeUIScheme))
    return false;

  return RemoveUberHost(url);
}

// Used by the GetPrivilegeRequiredByUrl() and GetProcessPrivilege() functions
// below.  Extension, and isolated apps require different privileges to be
// granted to their RenderProcessHosts.  This classification allows us to make
// sure URLs are served by hosts with the right set of privileges.
enum RenderProcessHostPrivilege {
  PRIV_NORMAL,
  PRIV_HOSTED,
  PRIV_ISOLATED,
  PRIV_EXTENSION,
};

RenderProcessHostPrivilege GetPrivilegeRequiredByUrl(
    const GURL& url,
    ExtensionService* service) {
  // Default to a normal renderer cause it is lower privileged. This should only
  // occur if the URL on a site instance is either malformed, or uninitialized.
  // If it is malformed, then there is no need for better privileges anyways.
  // If it is uninitialized, but eventually settles on being an a scheme other
  // than normal webrenderer, the navigation logic will correct us out of band
  // anyways.
  if (!url.is_valid())
    return PRIV_NORMAL;

  if (url.SchemeIs(extensions::kExtensionScheme)) {
    const Extension* extension =
        service->extensions()->GetByID(url.host());
    if (extension &&
        extensions::AppIsolationInfo::HasIsolatedStorage(extension))
      return PRIV_ISOLATED;
    if (extension && extension->is_hosted_app())
      return PRIV_HOSTED;

    return PRIV_EXTENSION;
  }

  return PRIV_NORMAL;
}

RenderProcessHostPrivilege GetProcessPrivilege(
    content::RenderProcessHost* process_host,
    extensions::ProcessMap* process_map,
    ExtensionService* service) {
  std::set<std::string> extension_ids =
      process_map->GetExtensionsInProcess(process_host->GetID());
  if (extension_ids.empty())
    return PRIV_NORMAL;

  for (std::set<std::string>::iterator iter = extension_ids.begin();
       iter != extension_ids.end(); ++iter) {
    const Extension* extension = service->GetExtensionById(*iter, false);
    if (extension &&
        extensions::AppIsolationInfo::HasIsolatedStorage(extension))
      return PRIV_ISOLATED;
    if (extension && extension->is_hosted_app())
      return PRIV_HOSTED;
  }

  return PRIV_EXTENSION;
}

bool CertMatchesFilter(const net::X509Certificate& cert,
                       const base::DictionaryValue& filter) {
  // TODO(markusheintz): This is the minimal required filter implementation.
  // Implement a better matcher.

  // An empty filter matches any client certificate since no requirements are
  // specified at all.
  if (filter.empty())
    return true;

  std::string common_name;
  if (filter.GetString("ISSUER.CN", &common_name) &&
      (cert.issuer().common_name == common_name)) {
    return true;
  }
  return false;
}

#if !defined(OS_ANDROID)
// Fills |map| with the per-script font prefs under path |map_name|.
void FillFontFamilyMap(const PrefService* prefs,
                       const char* map_name,
                       webkit_glue::ScriptFontFamilyMap* map) {
  // TODO: Get rid of the brute-force scan over possible (font family / script)
  // combinations - see http://crbug.com/308095.
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::string pref_name = base::StringPrintf("%s.%s", map_name, script);
    std::string font_family = prefs->GetString(pref_name.c_str());
    if (!font_family.empty())
      (*map)[script] = base::UTF8ToUTF16(font_family);
  }
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(
            process_type, dumps_path, getenv(env_vars::kHeadless) == NULL);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const CommandLine& command_line) {
  if (command_line.HasSwitch(extensions::switches::kExtensionProcess)) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost("extension");
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)
#endif  // !defined(OS_ANDROID)

#if !defined(OS_CHROMEOS)
GURL GetEffectiveURLForSignin(const GURL& url) {
  CHECK(SigninManager::IsWebBasedSigninFlowURL(url));

  GURL effective_url(SigninManager::kChromeSigninEffectiveSite);
  // Copy the path because the argument to SetPathStr must outlive
  // the Replacements object.
  const std::string path_copy(url.path());
  GURL::Replacements replacements;
  replacements.SetPathStr(path_copy);
  effective_url = effective_url.ReplaceComponents(replacements);
  return effective_url;
}
#endif

void SetApplicationLocaleOnIOThread(const std::string& locale) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  g_io_thread_application_locale.Get() = locale;
}

void HandleBlockedPopupOnUIThread(const BlockedWindowParams& params) {
  WebContents* tab = tab_util::GetWebContentsByID(params.render_process_id(),
                                                  params.opener_id());
  if (!tab)
    return;

  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(tab);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_CREATE_NEW_WINDOW);
    return;
  }

  PopupBlockerTabHelper* popup_helper =
      PopupBlockerTabHelper::FromWebContents(tab);
  if (!popup_helper)
    return;
  popup_helper->AddBlockedPopup(params);
}

#if defined(OS_ANDROID)

void HandleSingleTabModeBlockOnUIThread(const BlockedWindowParams& params) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(params.render_process_id(),
                                   params.opener_id());
  if (!web_contents)
    return;

  SingleTabModeTabHelper::FromWebContents(web_contents)->HandleOpenUrl(params);
}

float GetDeviceScaleAdjustment() {
  static const float kMinFSM = 1.05f;
  static const int kWidthForMinFSM = 320;
  static const float kMaxFSM = 1.3f;
  static const int kWidthForMaxFSM = 800;

  gfx::DeviceDisplayInfo info;
  int minWidth = info.GetSmallestDIPWidth();

  if (minWidth <= kWidthForMinFSM)
    return kMinFSM;
  if (minWidth >= kWidthForMaxFSM)
    return kMaxFSM;

  // The font scale multiplier varies linearly between kMinFSM and kMaxFSM.
  float ratio = static_cast<float>(minWidth - kWidthForMinFSM) /
      (kWidthForMaxFSM - kWidthForMinFSM);
  return ratio * (kMaxFSM - kMinFSM) + kMinFSM;
}

#endif  // defined(OS_ANDROID)

}  // namespace

namespace chrome {

ChromeContentBrowserClient::ChromeContentBrowserClient() {
#if defined(ENABLE_PLUGINS)
  for (size_t i = 0; i < arraysize(kPredefinedAllowedFileHandleOrigins); ++i)
    allowed_file_handle_origins_.insert(kPredefinedAllowedFileHandleOrigins[i]);
  for (size_t i = 0; i < arraysize(kPredefinedAllowedSocketOrigins); ++i)
    allowed_socket_origins_.insert(kPredefinedAllowedSocketOrigins[i]);
#endif

  permissions_policy_delegate_.reset(
      new extensions::BrowserPermissionsPolicyDelegate());
}

ChromeContentBrowserClient::~ChromeContentBrowserClient() {
}

// static
void ChromeContentBrowserClient::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kDisable3DAPIs,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableHyperlinkAuditing,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
void ChromeContentBrowserClient::SetApplicationLocale(
    const std::string& locale) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This object is guaranteed to outlive all threads so we don't have to
  // worry about the lack of refcounting and can just post as Unretained.
  //
  // The common case is that this function is called early in Chrome startup
  // before any threads are created (it will also be called later if the user
  // changes the pref). In this case, there will be no threads created and
  // posting will fail. When there are no threads, we can just set the string
  // without worrying about threadsafety.
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
          base::Bind(&SetApplicationLocaleOnIOThread, locale))) {
    g_io_thread_application_locale.Get() = locale;
  }
}

content::BrowserMainParts* ChromeContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  ChromeBrowserMainParts* main_parts;
  // Construct the Main browser parts based on the OS type.
#if defined(OS_WIN)
  main_parts = new ChromeBrowserMainPartsWin(parameters);
#elif defined(OS_MACOSX)
  main_parts = new ChromeBrowserMainPartsMac(parameters);
#elif defined(OS_CHROMEOS)
  main_parts = new chromeos::ChromeBrowserMainPartsChromeos(parameters);
#elif defined(OS_LINUX)
  main_parts = new ChromeBrowserMainPartsLinux(parameters);
#elif defined(OS_ANDROID)
  main_parts = new ChromeBrowserMainPartsAndroid(parameters);
#elif defined(OS_POSIX)
  main_parts = new ChromeBrowserMainPartsPosix(parameters);
#else
  NOTREACHED();
  main_parts = new ChromeBrowserMainParts(parameters);
#endif

  chrome::AddProfilesExtraParts(main_parts);

  // Construct additional browser parts. Stages are called in the order in
  // which they are added.
#if defined(TOOLKIT_GTK)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsGtk());
#endif

#if defined(TOOLKIT_VIEWS)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsViews());
#endif

#if defined(USE_ASH)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAsh());
#endif

#if defined(USE_AURA)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAura());
#endif

#if defined(USE_X11)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsX11());
#endif

  chrome::AddMetricsExtraParts(main_parts);

  return main_parts;
}

std::string ChromeContentBrowserClient::GetStoragePartitionIdForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  std::string partition_id;

  // The partition ID for webview guest processes is the string value of its
  // SiteInstance URL - "chrome-guest://app_id/persist?partition".
  if (site.SchemeIs(content::kGuestScheme)) {
    partition_id = site.spec();
  } else if (site.GetOrigin().spec() == kChromeUIChromeSigninURL) {
    // Chrome signin page has an embedded iframe of extension and web content,
    // thus it must be isolated from other webUI pages.
    partition_id = site.GetOrigin().spec();
  }

  DCHECK(IsValidStoragePartitionId(browser_context, partition_id));
  return partition_id;
}

bool ChromeContentBrowserClient::IsValidStoragePartitionId(
    content::BrowserContext* browser_context,
    const std::string& partition_id) {
  // The default ID is empty and is always valid.
  if (partition_id.empty())
    return true;

  return GURL(partition_id).is_valid();
}

void ChromeContentBrowserClient::GetStoragePartitionConfigForSite(
    content::BrowserContext* browser_context,
    const GURL& site,
    bool can_be_default,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {
  // Default to the browser-wide storage partition and override based on |site|
  // below.
  partition_domain->clear();
  partition_name->clear();
  *in_memory = false;

  bool success = GuestView::GetGuestPartitionConfigForSite(
      site, partition_domain, partition_name, in_memory);

  if (!success && site.SchemeIs(extensions::kExtensionScheme)) {
    // If |can_be_default| is false, the caller is stating that the |site|
    // should be parsed as if it had isolated storage. In particular it is
    // important to NOT check ExtensionService for the is_storage_isolated()
    // attribute because this code path is run during Extension uninstall
    // to do cleanup after the Extension has already been unloaded from the
    // ExtensionService.
    bool is_isolated = !can_be_default;
    if (can_be_default) {
      if (extensions::util::SiteHasIsolatedStorage(site, browser_context))
        is_isolated = true;
    }

    if (is_isolated) {
      CHECK(site.has_host());
      // For extensions with isolated storage, the the host of the |site| is
      // the |partition_domain|. The |in_memory| and |partition_name| are only
      // used in guest schemes so they are cleared here.
      *partition_domain = site.host();
      *in_memory = false;
      partition_name->clear();
    }
  } else if (site.GetOrigin().spec() == kChromeUIChromeSigninURL) {
    // Chrome signin page has an embedded iframe of extension and web content,
    // thus it must be isolated from other webUI pages.
    *partition_domain = chrome::kChromeUIChromeSigninHost;
  }

  // Assert that if |can_be_default| is false, the code above must have found a
  // non-default partition.  If this fails, the caller has a serious logic
  // error about which StoragePartition they expect to be in and it is not
  // safe to continue.
  CHECK(can_be_default || !partition_domain->empty());
}

content::WebContentsViewDelegate*
    ChromeContentBrowserClient::GetWebContentsViewDelegate(
        content::WebContents* web_contents) {
  return chrome::CreateWebContentsViewDelegate(web_contents);
}

void ChromeContentBrowserClient::GuestWebContentsCreated(
    SiteInstance* guest_site_instance,
    WebContents* guest_web_contents,
    WebContents* opener_web_contents,
    content::BrowserPluginGuestDelegate** guest_delegate,
    scoped_ptr<base::DictionaryValue> extra_params) {
  if (!guest_site_instance) {
    NOTREACHED();
    return;
  }
  GURL guest_site_url = guest_site_instance->GetSiteURL();
  const std::string& extension_id = guest_site_url.host();

  Profile* profile = Profile::FromBrowserContext(
      guest_web_contents->GetBrowserContext());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service) {
    NOTREACHED();
    return;
  }

  /// TODO(fsamuel): In the future, certain types of GuestViews won't require
  // extension bindings. At that point, we should clear |extension_id| instead
  // of exiting early.
  if (!service->GetExtensionById(extension_id, false) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserPluginForAllViewTypes)) {
    NOTREACHED();
    return;
  }

  if (opener_web_contents) {
    GuestView* guest = GuestView::FromWebContents(opener_web_contents);
    if (!guest) {
      NOTREACHED();
      return;
    }

    // Create a new GuestView of the same type as the opener.
    *guest_delegate =
        GuestView::Create(guest_web_contents,
                          extension_id,
                          guest->GetViewType());
    return;
  }

  if (!extra_params) {
    NOTREACHED();
    return;
  }
  std::string api_type;
  extra_params->GetString(guestview::kParameterApi, &api_type);

  if (api_type.empty())
    return;

  *guest_delegate =
      GuestView::Create(guest_web_contents,
                        extension_id,
                        GuestView::GetViewTypeFromString(api_type));
}

void ChromeContentBrowserClient::GuestWebContentsAttached(
    WebContents* guest_web_contents,
    WebContents* embedder_web_contents,
    const base::DictionaryValue& extra_params) {

  GuestView* guest = GuestView::FromWebContents(guest_web_contents);
  if (!guest) {
    // It's ok to return here, since we could be running a browser plugin
    // outside an extension, and don't need to attach a
    // BrowserPluginGuestDelegate in that case;
    // e.g. running with flag --enable-browser-plugin-for-all-view-types.
    return;
  }
  guest->Attach(embedder_web_contents, extra_params);
}

void ChromeContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  net::URLRequestContextGetter* context =
      profile->GetRequestContextForRenderProcess(id);

  host->AddFilter(new ChromeRenderMessageFilter(id, profile, context));
  host->AddFilter(new extensions::ExtensionMessageFilter(id, profile));
#if defined(ENABLE_PLUGINS)
  host->AddFilter(new PluginInfoMessageFilter(id, profile));
#endif
  host->AddFilter(new cast::CastTransportHostFilter);
#if defined(ENABLE_PRINTING)
  host->AddFilter(new PrintingMessageFilter(id, profile));
#endif
  host->AddFilter(new SearchProviderInstallStateMessageFilter(id, profile));
#if defined(ENABLE_SPELLCHECK)
  host->AddFilter(new SpellCheckMessageFilter(id));
#endif
#if defined(OS_MACOSX)
  host->AddFilter(new SpellCheckMessageFilterMac(id));
#endif
  host->AddFilter(new ChromeNetBenchmarkingMessageFilter(profile, context));
  host->AddFilter(new prerender::PrerenderMessageFilter(id, profile));
  host->AddFilter(new TtsMessageFilter(id, profile));
#if defined(ENABLE_WEBRTC)
  WebRtcLoggingHandlerHost* webrtc_logging_handler_host =
      new WebRtcLoggingHandlerHost(profile);
  host->SetWebRtcLogMessageCallback(base::Bind(
      &WebRtcLoggingHandlerHost::LogMessage, webrtc_logging_handler_host));
  host->AddFilter(webrtc_logging_handler_host);
  host->SetUserData(host, new base::UserDataAdapter<WebRtcLoggingHandlerHost>(
      webrtc_logging_handler_host));
#endif
#if !defined(DISABLE_NACL)
  host->AddFilter(new nacl::NaClHostMessageFilter(
      id, profile->IsOffTheRecord(),
      profile->GetPath(),
      context));
#endif
#if defined(OS_ANDROID)
  host->AddFilter(new EncryptedMediaMessageFilterAndroid());
#endif
  if (switches::IsNewProfileManagement())
    host->AddFilter(new PrincipalsMessageFilter(id));

  host->Send(new ChromeViewMsg_SetIsIncognitoProcess(
      profile->IsOffTheRecord()));

  SendExtensionWebRequestStatusToHost(host);

  RendererContentSettingRules rules;
  if (host->IsGuest()) {
    GuestView::GetDefaultContentSettingRules(&rules, profile->IsOffTheRecord());
  } else {
    GetRendererContentSettingRules(
        profile->GetHostContentSettingsMap(), &rules);
  }
  host->Send(new ChromeViewMsg_SetContentSettingRules(rules));
}

GURL ChromeContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context, const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return url;

  // If the input |url| should be assigned to the Instant renderer, make its
  // effective URL distinct from other URLs on the search provider's domain.
  if (chrome::ShouldAssignURLToInstantRenderer(url, profile))
    return chrome::GetEffectiveURLForInstant(url, profile);

#if !defined(OS_CHROMEOS)
  // If the input |url| should be assigned to the Signin renderer, make its
  // effective URL distinct from other URLs on the signin service's domain.
  // Note that the signin renderer will be allowed to sign the user in to
  // Chrome.
  if (SigninManager::IsWebBasedSigninFlowURL(url))
    return GetEffectiveURLForSignin(url);
#endif

  // If the input |url| is part of an installed app, the effective URL is an
  // extension URL with the ID of that extension as the host. This has the
  // effect of grouping apps together in a common SiteInstance.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service)
    return url;

  const Extension* extension = extension_service->extensions()->
      GetHostedAppByURL(url);
  if (!extension)
    return url;

  // Bookmark apps do not use the hosted app process model, and should be
  // treated as normal URLs.
  if (extension->from_bookmark())
    return url;

  // If the URL is part of an extension's web extent, convert it to an
  // extension URL.
  return extension->GetResourceURL(url.path());
}

bool ChromeContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context, const GURL& effective_url) {
  // Non-extension, non-Instant URLs should generally use
  // process-per-site-instance.  Because we expect to use the effective URL,
  // URLs for hosted apps (apart from bookmark apps) should have an extension
  // scheme by now.

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  if (chrome::ShouldUseProcessPerSiteForInstantURL(effective_url, profile))
    return true;

#if !defined(OS_CHROMEOS)
  if (SigninManager::IsWebBasedSigninFlowURL(effective_url))
    return true;
#endif

  if (!effective_url.SchemeIs(extensions::kExtensionScheme))
    return false;

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service)
    return false;

  const Extension* extension =
      extension_service->extensions()->GetExtensionOrAppByURL(effective_url);
  if (!extension)
    return false;

  // If the URL is part of a hosted app that does not have the background
  // permission, or that does not allow JavaScript access to the background
  // page, we want to give each instance its own process to improve
  // responsiveness.
  if (extension->GetType() == Manifest::TYPE_HOSTED_APP) {
    if (!extension->HasAPIPermission(APIPermission::kBackground) ||
        !extensions::BackgroundInfo::AllowJSAccess(extension)) {
      return false;
    }
  }

  // Hosted apps that have script access to their background page must use
  // process per site, since all instances can make synchronous calls to the
  // background window.  Other extensions should use process per site as well.
  return true;
}

// These are treated as WebUI schemes but do not get WebUI bindings. Also,
// view-source is allowed for these schemes.
void ChromeContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(chrome::kChromeSearchScheme);
  additional_schemes->push_back(chrome::kDomDistillerScheme);
}

void ChromeContentBrowserClient::GetAdditionalWebUIHostsToIgnoreParititionCheck(
    std::vector<std::string>* hosts) {
  hosts->push_back(chrome::kChromeUIExtensionIconHost);
  hosts->push_back(chrome::kChromeUIFaviconHost);
  hosts->push_back(chrome::kChromeUIThemeHost);
  hosts->push_back(chrome::kChromeUIThumbnailHost);
  hosts->push_back(chrome::kChromeUIThumbnailHost2);
  hosts->push_back(chrome::kChromeUIThumbnailListHost);
}

net::URLRequestContextGetter*
ChromeContentBrowserClient::CreateRequestContext(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::ProtocolHandlerScopedVector protocol_interceptors) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->CreateRequestContext(protocol_handlers,
                                       protocol_interceptors.Pass());
}

net::URLRequestContextGetter*
ChromeContentBrowserClient::CreateRequestContextForStoragePartition(
    content::BrowserContext* browser_context,
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::ProtocolHandlerScopedVector protocol_interceptors) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->CreateRequestContextForStoragePartition(
      partition_path,
      in_memory,
      protocol_handlers,
      protocol_interceptors.Pass());
}

bool ChromeContentBrowserClient::IsHandledURL(const GURL& url) {
  return ProfileIOData::IsHandledURL(url);
}

bool ChromeContentBrowserClient::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
  // We need to let most extension URLs commit in any process, since this can
  // be allowed due to web_accessible_resources.  Most hosted app URLs may also
  // load in any process (e.g., in an iframe).  However, the Chrome Web Store
  // cannot be loaded in iframes and should never be requested outside its
  // process.
  Profile* profile =
      Profile::FromBrowserContext(process_host->GetBrowserContext());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return true;
  const Extension* new_extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  if (new_extension &&
      new_extension->is_hosted_app() &&
      new_extension->id() == extension_misc::kWebStoreAppId &&
      !extensions::ProcessMap::Get(profile)->
          Contains(new_extension->id(), process_host->GetID())) {
    return false;
  }

  return true;
}

bool ChromeContentBrowserClient::ShouldAllowOpenURL(
    content::SiteInstance* site_instance, const GURL& url) {
  GURL from_url = site_instance->GetSiteURL();
  // Do not allow pages from the web or other extensions navigate to
  // non-web-accessible extension resources.
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      (from_url.SchemeIsHTTPOrHTTPS() ||
          from_url.SchemeIs(extensions::kExtensionScheme))) {
    Profile* profile = Profile::FromBrowserContext(
        site_instance->GetProcess()->GetBrowserContext());
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    if (!service)
      return true;
    const Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url);
    if (!extension)
      return true;
    const Extension* from_extension =
        service->extensions()->GetExtensionOrAppByURL(
            site_instance->GetSiteURL());
    if (from_extension && from_extension->id() == extension->id())
      return true;

    if (!extensions::WebAccessibleResourcesInfo::IsResourceWebAccessible(
            extension, url.path()))
      return false;
  }
  return true;
}

bool ChromeContentBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  Profile* profile =
      Profile::FromBrowserContext(process_host->GetBrowserContext());
  // This may be NULL during tests. In that case, just assume any site can
  // share any host.
  if (!profile)
    return true;

  // Instant URLs should only be in the instant process and instant process
  // should only have Instant URLs.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (instant_service) {
    bool is_instant_process = instant_service->IsInstantProcess(
        process_host->GetID());
    bool should_be_in_instant_process =
        chrome::ShouldAssignURLToInstantRenderer(site_url, profile);
    if (is_instant_process || should_be_in_instant_process)
      return is_instant_process && should_be_in_instant_process;
  }

#if !defined(OS_CHROMEOS)
  ChromeSigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile);
  if (signin_client && signin_client->IsSigninProcess(process_host->GetID()))
    return SigninManager::IsWebBasedSigninFlowURL(site_url);
#endif

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);

  // Don't allow the Task Manager to share a process with anything else.
  // Otherwise it can affect the renderers it is observing.
  // Note: we could create another RenderProcessHostPrivilege bucket for
  // this to allow multiple chrome://tasks instances to share, but that's
  // a very unlikely case without serious consequences.
  if (site_url.GetOrigin() == GURL(chrome::kChromeUITaskManagerURL).GetOrigin())
    return false;

  // These may be NULL during tests. In that case, just assume any site can
  // share any host.
  if (!service || !process_map)
    return true;

  // Otherwise, just make sure the process privilege matches the privilege
  // required by the site.
  RenderProcessHostPrivilege privilege_required =
      GetPrivilegeRequiredByUrl(site_url, service);
  return GetProcessPrivilege(process_host, process_map, service) ==
      privilege_required;
}

// This function is trying to limit the amount of processes used by extensions
// with background pages. It uses a globally set percentage of processes to
// run such extensions and if the limit is exceeded, it returns true, to
// indicate to the content module to group extensions together.
bool ChromeContentBrowserClient::ShouldTryToUseExistingProcessHost(
    content::BrowserContext* browser_context, const GURL& url) {
  // It has to be a valid URL for us to check for an extension.
  if (!url.is_valid())
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  ExtensionService* service = !profile ? NULL :
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return false;

  // We have to have a valid extension with background page to proceed.
  const Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  if (!extension)
    return false;
  if (!extensions::BackgroundInfo::HasBackgroundPage(extension))
    return false;

  std::set<int> process_ids;
  size_t max_process_count =
      content::RenderProcessHost::GetMaxRendererProcessCount();

  // Go through all profiles to ensure we have total count of extension
  // processes containing background pages, otherwise one profile can
  // starve the other.
  std::vector<Profile*> profiles = g_browser_process->profile_manager()->
      GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    extensions::ProcessManager* epm =
        extensions::ExtensionSystem::Get(profiles[i])->process_manager();
    for (extensions::ProcessManager::const_iterator iter =
             epm->background_hosts().begin();
         iter != epm->background_hosts().end(); ++iter) {
      const extensions::ExtensionHost* host = *iter;
      process_ids.insert(host->render_process_host()->GetID());
    }
  }

  if (process_ids.size() >
      (max_process_count * chrome::kMaxShareOfExtensionProcesses)) {
    return true;
  }

  return false;
}

void ChromeContentBrowserClient::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
  CHECK(site_instance->HasProcess());

  Profile* profile = Profile::FromBrowserContext(
      site_instance->GetBrowserContext());
  if (!profile)
    return;

  // Remember the ID of the Instant process to signal the renderer process
  // on startup in |AppendExtraCommandLineSwitches| below.
  if (chrome::ShouldAssignURLToInstantRenderer(
          site_instance->GetSiteURL(), profile)) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile);
    if (instant_service)
      instant_service->AddInstantProcess(site_instance->GetProcess()->GetID());
  }

#if !defined(OS_CHROMEOS)
  // We only expect there to be one signin process as we use process-per-site
  // for signin URLs. The signin process will be cleared from SigninManager
  // when the renderer is destroyed.
  if (SigninManager::IsWebBasedSigninFlowURL(site_instance->GetSiteURL())) {
    ChromeSigninClient* signin_client =
        ChromeSigninClientFactory::GetForProfile(profile);
    if (signin_client)
      signin_client->SetSigninProcess(site_instance->GetProcess()->GetID());
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&InfoMap::SetSigninProcess,
                   extensions::ExtensionSystem::Get(profile)->info_map(),
                   site_instance->GetProcess()->GetID()));
  }
#endif

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const Extension* extension = service->extensions()->GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
  if (!extension)
    return;

  extensions::ProcessMap::Get(profile)
      ->Insert(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::RegisterExtensionProcess,
                 extensions::ExtensionSystem::Get(profile)->info_map(),
                 extension->id(),
                 site_instance->GetProcess()->GetID(),
                 site_instance->GetId()));
}

void ChromeContentBrowserClient::SiteInstanceDeleting(
    SiteInstance* site_instance) {
  if (!site_instance->HasProcess())
    return;

  Profile* profile = Profile::FromBrowserContext(
      site_instance->GetBrowserContext());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const Extension* extension = service->extensions()->GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
  if (!extension)
    return;

  extensions::ProcessMap::Get(profile)
      ->Remove(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::UnregisterExtensionProcess,
                 extensions::ExtensionSystem::Get(profile)->info_map(),
                 extension->id(),
                 site_instance->GetProcess()->GetID(),
                 site_instance->GetId()));
}

void ChromeContentBrowserClient::WorkerProcessCreated(
    SiteInstance* site_instance,
    int worker_process_id) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!extension_registry)
    return;
  const Extension* extension =
      extension_registry->enabled_extensions().GetExtensionOrAppByURL(
        site_instance->GetSiteURL());
  if (!extension)
    return;
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(site_instance->GetBrowserContext());
  extension_system->info_map()->RegisterExtensionWorkerProcess(
      extension->id(),
      worker_process_id,
      site_instance->GetId());
}

void ChromeContentBrowserClient::WorkerProcessTerminated(
    SiteInstance* site_instance,
    int worker_process_id) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(site_instance->GetBrowserContext());
  extension_system->info_map()->UnregisterExtensionWorkerProcess(
      worker_process_id);
}

bool ChromeContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_url,
    const GURL& new_url) {
  // If we don't have an ExtensionService, then rely on the SiteInstance logic
  // in RenderFrameHostManager to decide when to swap.
  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return false;

  // We must use a new BrowsingInstance (forcing a process swap and disabling
  // scripting by existing tabs) if one of the URLs is an extension and the
  // other is not the exact same extension.
  //
  // We ignore hosted apps here so that other tabs in their BrowsingInstance can
  // use postMessage with them.  (The exception is the Chrome Web Store, which
  // is a hosted app that requires its own BrowsingInstance.)  Navigations
  // to/from a hosted app will still trigger a SiteInstance swap in
  // RenderFrameHostManager.
  const Extension* current_extension =
      service->extensions()->GetExtensionOrAppByURL(current_url);
  if (current_extension &&
      current_extension->is_hosted_app() &&
      current_extension->id() != extension_misc::kWebStoreAppId)
    current_extension = NULL;

  const Extension* new_extension =
      service->extensions()->GetExtensionOrAppByURL(new_url);
  if (new_extension &&
      new_extension->is_hosted_app() &&
      new_extension->id() != extension_misc::kWebStoreAppId)
    new_extension = NULL;

  // First do a process check.  We should force a BrowsingInstance swap if the
  // current process doesn't know about new_extension, even if current_extension
  // is somehow the same as new_extension.
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);
  if (new_extension &&
      site_instance->HasProcess() &&
      !process_map->Contains(
          new_extension->id(), site_instance->GetProcess()->GetID()))
    return true;

  // Otherwise, swap BrowsingInstances if current_extension and new_extension
  // differ.
  return current_extension != new_extension;
}

bool ChromeContentBrowserClient::ShouldSwapProcessesForRedirect(
    content::ResourceContext* resource_context, const GURL& current_url,
    const GURL& new_url) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  return extensions::CrossesExtensionProcessBoundary(
      io_data->GetExtensionInfoMap()->extensions(),
      current_url, new_url, false);
}

bool ChromeContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return !url.SchemeIs(chrome::kChromeNativeScheme);
}

std::string ChromeContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return CharacterEncoding::GetCanonicalEncodingNameByAliasName(alias_name);
}

void ChromeContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
#if defined(OS_POSIX)
  if (breakpad::IsCrashReporterEnabled()) {
    std::string enable_crash_reporter;
    GoogleUpdateSettings::GetMetricsId(&enable_crash_reporter);
#if !defined(OS_MACOSX)
    enable_crash_reporter += "," + base::GetLinuxDistro();
#endif
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
        enable_crash_reporter);
  }
#endif  // OS_POSIX

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

  static const char* const kCommonSwitchNames[] = {
    switches::kUserAgent,
    switches::kUserDataDir,  // Make logs go to the right file.
  };
  command_line->CopySwitchesFrom(browser_command_line, kCommonSwitchNames,
                                 arraysize(kCommonSwitchNames));

#if defined(ENABLE_IPC_FUZZER)
  static const char* const kIpcFuzzerSwitches[] = {
    switches::kIpcFuzzerTestcase,
  };
  command_line->CopySwitchesFrom(browser_command_line, kIpcFuzzerSwitches,
                                 arraysize(kIpcFuzzerSwitches));
#endif

  if (process_type == switches::kRendererProcess) {
#if defined(OS_CHROMEOS)
    const std::string& login_profile =
        browser_command_line.GetSwitchValueASCII(
            chromeos::switches::kLoginProfile);
    if (!login_profile.empty())
      command_line->AppendSwitchASCII(
          chromeos::switches::kLoginProfile, login_profile);
#endif

#if defined(ENABLE_WEBRTC)
    MaybeCopyDisableWebRtcEncryptionSwitch(command_line,
                                           browser_command_line,
                                           VersionInfo::GetChannel());
#endif

    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(child_process_id);
    if (process) {
      Profile* profile = Profile::FromBrowserContext(
          process->GetBrowserContext());

      if (extensions::ProcessMap::Get(profile)->Contains(process->GetID()))
        command_line->AppendSwitch(extensions::switches::kExtensionProcess);

      PrefService* prefs = profile->GetPrefs();
      // Currently this pref is only registered if applied via a policy.
      if (prefs->HasPrefPath(prefs::kDisable3DAPIs) &&
          prefs->GetBoolean(prefs::kDisable3DAPIs)) {
        // Turn this policy into a command line switch.
        command_line->AppendSwitch(switches::kDisable3DAPIs);
      }

      // Disable client-side phishing detection in the renderer if it is
      // disabled in the Profile preferences or the browser process.
      if (!prefs->GetBoolean(prefs::kSafeBrowsingEnabled) ||
          !g_browser_process->safe_browsing_detection_service()) {
        command_line->AppendSwitch(
            switches::kDisableClientSidePhishingDetection);
      }

      if (!prefs->GetBoolean(prefs::kPrintPreviewDisabled))
        command_line->AppendSwitch(switches::kRendererPrintPreview);

      InstantService* instant_service =
          InstantServiceFactory::GetForProfile(profile);
      if (instant_service &&
          instant_service->IsInstantProcess(process->GetID()))
        command_line->AppendSwitch(switches::kInstantProcess);

#if !defined(OS_CHROMEOS)
      ChromeSigninClient* signin_client =
          ChromeSigninClientFactory::GetForProfile(profile);
      if (signin_client && signin_client->IsSigninProcess(process->GetID()))
        command_line->AppendSwitch(switches::kSigninProcess);
#endif
    }

    {
      // Enable auto-reload if this session is in the field trial or the user
      // explicitly enabled it.
      std::string group =
          base::FieldTrialList::FindFullName("AutoReloadExperiment");
      if (group == "Enabled" ||
          browser_command_line.HasSwitch(switches::kEnableOfflineAutoReload)) {
        command_line->AppendSwitch(switches::kEnableOfflineAutoReload);
      }
    }

    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
      autofill::switches::kDisableIgnoreAutocompleteOff,
      autofill::switches::kDisablePasswordGeneration,
      autofill::switches::kEnablePasswordGeneration,
      autofill::switches::kLocalHeuristicsOnlyForPasswordGeneration,
      extensions::switches::kAllowHTTPBackgroundPage,
      extensions::switches::kAllowLegacyExtensionManifests,
      extensions::switches::kEnableExperimentalExtensionApis,
      extensions::switches::kExtensionsOnChromeURLs,
      extensions::switches::kWhitelistedExtensionID,
      // TODO(victorhsieh): remove the following flag once we move PPAPI FileIO
      // to browser.
      switches::kAllowNaClFileHandleAPI,
      switches::kAppsCheckoutURL,
      switches::kAppsGalleryURL,
      switches::kCloudPrintServiceURL,
      switches::kDisableBundledPpapiFlash,
      switches::kDisableExtensionsResourceWhitelist,
      switches::kDisablePnacl,
      switches::kDisableScriptedPrintThrottling,
      switches::kEnableAdview,
      switches::kEnableAppWindowControls,
      switches::kEnableBenchmarking,
      switches::kEnableNaCl,
      switches::kEnableNaClDebug,
      switches::kEnableNaClNonSfiMode,
      switches::kEnableNetBenchmarking,
      switches::kEnableStreamlinedHostedApps,
      switches::kEnableWatchdog,
      switches::kMemoryProfiling,
      switches::kMessageLoopHistogrammer,
      switches::kNoJsRandomness,
      switches::kOutOfProcessPdf,
      switches::kPlaybackMode,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kProfilingAtStart,
      switches::kProfilingFile,
      switches::kProfilingFlush,
      switches::kRecordMode,
      switches::kSilentDumpOnDCHECK,
      translate::switches::kTranslateSecurityOrigin,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kUtilityProcess) {
    static const char* const kSwitchNames[] = {
      extensions::switches::kAllowHTTPBackgroundPage,
      extensions::switches::kEnableExperimentalExtensionApis,
      extensions::switches::kExtensionsOnChromeURLs,
      extensions::switches::kWhitelistedExtensionID,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kPluginProcess) {
    static const char* const kSwitchNames[] = {
#if defined(OS_CHROMEOS)
      chromeos::switches::kLoginProfile,
#endif
      switches::kMemoryProfiling,
      switches::kSilentDumpOnDCHECK,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kZygoteProcess) {
    static const char* const kSwitchNames[] = {
      // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
      switches::kDisableBundledPpapiFlash,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kGpuProcess) {
    // If --ignore-gpu-blacklist is passed in, don't send in crash reports
    // because GPU is expected to be unreliable.
    if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) &&
        !command_line->HasSwitch(switches::kDisableBreakpad))
      command_line->AppendSwitch(switches::kDisableBreakpad);
  }

  // The command line switch kEnableBenchmarking needs to be specified along
  // with the kEnableStatsTable switch to ensure that the stats table global
  // is initialized correctly.
  if (command_line->HasSwitch(switches::kEnableBenchmarking))
    DCHECK(command_line->HasSwitch(switches::kEnableStatsTable));
}

std::string ChromeContentBrowserClient::GetApplicationLocale() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return g_io_thread_application_locale.Get();
  return g_browser_process->GetApplicationLocale();
}

std::string ChromeContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

gfx::ImageSkia* ChromeContentBrowserClient::GetDefaultFavicon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
}

bool ChromeContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& first_party,
    content::ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  return io_data->GetCookieSettings()->
      IsSettingCookieAllowed(manifest_url, first_party);
}

bool ChromeContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  bool allow = io_data->GetCookieSettings()->
      IsReadingCookieAllowed(url, first_party);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::CookiesRead, render_process_id,
                 render_frame_id, url, first_party, cookie_list, !allow, true));
  return allow;
}

bool ChromeContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id,
    net::CookieOptions* options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, first_party);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::CookieChanged, render_process_id,
                 render_frame_id, url, first_party, cookie_line, *options,
                 !allow));
  return allow;
}

bool ChromeContentBrowserClient::AllowSaveLocalState(
    content::ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  ContentSetting setting = cookie_settings->GetDefaultCookieSetting(NULL);

  // TODO(bauerb): Should we also disallow local state if the default is BLOCK?
  // Could we even support per-origin settings?
  return setting != CONTENT_SETTING_SESSION_ONLY;
}

bool ChromeContentBrowserClient::AllowWorkerDatabase(
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    unsigned long estimated_size,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

  // Record access to database for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_frames.begin(); i != render_frames.end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::WebDatabaseAccessed,
                   i->first, i->second, url, name, display_name, !allow));
  }

  return allow;
}

bool ChromeContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

  // Record access to file system for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_frames.begin(); i != render_frames.end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::FileSystemAccessed,
                   i->first, i->second, url, !allow));
  }

  return allow;
}

bool ChromeContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const base::string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

  // Record access to IndexedDB for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_frames.begin(); i != render_frames.end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::IndexedDBAccessed,
                   i->first, i->second, url, name, !allow));
  }

  return allow;
}

net::URLRequestContext*
ChromeContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, content::ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
    return io_data->extensions_request_context();
  }

  return NULL;
}

QuotaPermissionContext*
ChromeContentBrowserClient::CreateQuotaPermissionContext() {
  return new ChromeQuotaPermissionContext();
}

void ChromeContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_frame_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    ResourceType::Type resource_type,
    bool overridable,
    bool strict_enforcement,
    const base::Callback<void(bool)>& callback,
    content::CertificateRequestResultType* result) {
  if (resource_type != ResourceType::MAIN_FRAME) {
    // A sub-resource has a certificate error.  The user doesn't really
    // have a context for making the right decision, so block the
    // request hard, without an info bar to allow showing the insecure
    // content.
    *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY;
    return;
  }

  // If the tab is being prerendered, cancel the prerender and the request.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  WebContents* tab = WebContents::FromRenderFrameHost(render_frame_host);
  if (!tab) {
    NOTREACHED();
    return;
  }

  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(tab);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_SSL_ERROR);
    *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
    return;
  }

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper =
      captive_portal::CaptivePortalTabHelper::FromWebContents(tab);
  if (captive_portal_tab_helper)
    captive_portal_tab_helper->OnSSLCertError(ssl_info);
#endif

  // Otherwise, display an SSL blocking page.
  new SSLBlockingPage(tab, cert_error, ssl_info, request_url, overridable,
                      strict_enforcement, callback);
}

void ChromeContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_frame_id,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      render_process_id, render_frame_id);
  WebContents* tab = WebContents::FromRenderFrameHost(rfh);
  if (!tab) {
    NOTREACHED();
    return;
  }

  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(tab);
  if (prerender_contents) {
    prerender_contents->Destroy(
        prerender::FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED);
    return;
  }

  GURL requesting_url("https://" + cert_request_info->host_and_port.ToString());
  DCHECK(requesting_url.is_valid())
      << "Invalid URL string: https://"
      << cert_request_info->host_and_port.ToString();

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  scoped_ptr<base::Value> filter(
      profile->GetHostContentSettingsMap()->GetWebsiteSetting(
          requesting_url,
          requesting_url,
          CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
          std::string(), NULL));

  if (filter.get()) {
    // Try to automatically select a client certificate.
    if (filter->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* filter_dict =
          static_cast<base::DictionaryValue*>(filter.get());

      const std::vector<scoped_refptr<net::X509Certificate> >&
          all_client_certs = cert_request_info->client_certs;
      for (size_t i = 0; i < all_client_certs.size(); ++i) {
        if (CertMatchesFilter(*all_client_certs[i].get(), *filter_dict)) {
          // Use the first certificate that is matched by the filter.
          callback.Run(all_client_certs[i].get());
          return;
        }
      }
    } else {
      NOTREACHED();
    }
  }

  SSLTabHelper* ssl_tab_helper = SSLTabHelper::FromWebContents(tab);
  if (!ssl_tab_helper) {
    // If there is no SSLTabHelper for the given WebContents then we can't
    // show the user a dialog to select a client certificate. So we simply
    // proceed with no client certificate.
    callback.Run(NULL);
    return;
  }
  ssl_tab_helper->ShowClientCertificateRequestDialog(
      network_session, cert_request_info, callback);
}

void ChromeContentBrowserClient::AddCertificate(
    net::URLRequest* request,
    net::CertificateMimeType cert_type,
    const void* cert_data,
    size_t cert_size,
    int render_process_id,
    int render_view_id) {
  chrome::SSLAddCertificate(request, cert_type, cert_data, cert_size,
      render_process_id, render_view_id);
}

content::MediaObserver* ChromeContentBrowserClient::GetMediaObserver() {
  return MediaCaptureDevicesDispatcher::GetInstance();
}

void ChromeContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
#if defined(ENABLE_NOTIFICATIONS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!contents) {
    NOTREACHED();
    return;
  }

  // Skip showing the infobar if the request comes from an extension, and that
  // extension has the 'notify' permission. (If the extension does not have the
  // permission, the user will still be prompted.)
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  InfoMap* extension_info_map =
      extensions::ExtensionSystem::Get(profile)->info_map();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  const Extension* extension = NULL;
  if (extension_info_map) {
    extensions::ExtensionSet extensions;
    extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
        source_origin, render_process_id,
        extensions::APIPermission::kNotification, &extensions);
    for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
         iter != extensions.end(); ++iter) {
      if (notification_service->IsNotifierEnabled(NotifierId(
              NotifierId::APPLICATION, (*iter)->id()))) {
        extension = iter->get();
        break;
      }
    }
  }
  RenderViewHost* rvh =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (IsExtensionWithPermissionOrSuggestInConsole(
      APIPermission::kNotification, extension, rvh)) {
    if (rvh)
      rvh->DesktopNotificationPermissionRequestDone(callback_context);
    return;
  }

  notification_service->RequestPermission(source_origin, render_process_id,
      render_view_id, callback_context, contents);
#else
  NOTIMPLEMENTED();
#endif
}

blink::WebNotificationPresenter::Permission
    ChromeContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_origin,
        content::ResourceContext* context,
        int render_process_id) {
#if defined(ENABLE_NOTIFICATIONS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  InfoMap* extension_info_map = io_data->GetExtensionInfoMap();

  // We want to see if there is an extension that hasn't been manually disabled
  // that has the notifications permission and applies to this security origin.
  // First, get the list of extensions with permission for the origin.
  extensions::ExtensionSet extensions;
  extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
      source_origin, render_process_id,
      extensions::APIPermission::kNotification, &extensions);
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    // Then, check to see if it's been disabled by the user.
    if (!extension_info_map->AreNotificationsDisabled((*iter)->id()))
      return blink::WebNotificationPresenter::PermissionAllowed;
  }

  // No enabled extensions exist, so check the normal host content settings.
  HostContentSettingsMap* host_content_settings_map =
      io_data->GetHostContentSettingsMap();
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      source_origin,
      source_origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER);

  if (setting == CONTENT_SETTING_ALLOW)
    return blink::WebNotificationPresenter::PermissionAllowed;
  if (setting == CONTENT_SETTING_BLOCK)
    return blink::WebNotificationPresenter::PermissionDenied;
  return blink::WebNotificationPresenter::PermissionNotAllowed;
#else
  return blink::WebNotificationPresenter::PermissionAllowed;
#endif
}

void ChromeContentBrowserClient::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
#if defined(ENABLE_NOTIFICATIONS)
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  content::RenderProcessHost* process = rvh->GetProcess();
  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  service->ShowDesktopNotification(
    params, render_process_id, render_view_id,
    worker ? DesktopNotificationService::WorkerNotification :
        DesktopNotificationService::PageNotification);
#else
  NOTIMPLEMENTED();
#endif
}

void ChromeContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
#if defined(ENABLE_NOTIFICATIONS)
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  content::RenderProcessHost* process = rvh->GetProcess();
  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  service->CancelDesktopNotification(
      render_process_id, render_view_id, notification_id);
#else
  NOTIMPLEMENTED();
#endif
}

bool ChromeContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    const WebWindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    content::ResourceContext* context,
    int render_process_id,
    bool is_guest,
    int opener_id,
    bool* no_javascript_access) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  *no_javascript_access = false;

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  InfoMap* map = io_data->GetExtensionInfoMap();

  // If the opener is trying to create a background window but doesn't have
  // the appropriate permission, fail the attempt.
  if (container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    if (!map->SecurityOriginHasAPIPermission(
            source_origin,
            render_process_id,
            APIPermission::kBackground)) {
      return false;
    }

    // Note: this use of GetExtensionOrAppByURL is safe but imperfect.  It may
    // return a recently installed Extension even if this CanCreateWindow call
    // was made by an old copy of the page in a normal web process.  That's ok,
    // because the permission check above would have caused an early return
    // already. We must use the full URL to find hosted apps, though, and not
    // just the origin.
    const Extension* extension =
        map->extensions().GetExtensionOrAppByURL(opener_url);
    if (extension && !extensions::BackgroundInfo::AllowJSAccess(extension))
      *no_javascript_access = true;

    return true;
  }

  // No new browser window (popup or tab) in app mode.
  if (container_type == WINDOW_CONTAINER_TYPE_NORMAL &&
      chrome::IsRunningInForcedAppMode()) {
    return false;
  }

  if (is_guest)
    return true;

  HostContentSettingsMap* content_settings =
      ProfileIOData::FromResourceContext(context)->GetHostContentSettingsMap();
  BlockedWindowParams blocked_params(target_url,
                                    referrer,
                                    disposition,
                                    features,
                                    user_gesture,
                                    opener_suppressed,
                                    render_process_id,
                                    opener_id);

  if (!user_gesture && !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisablePopupBlocking)) {
    if (content_settings->GetContentSetting(opener_top_level_frame_url,
                                            opener_top_level_frame_url,
                                            CONTENT_SETTINGS_TYPE_POPUPS,
                                            std::string()) !=
        CONTENT_SETTING_ALLOW) {
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&HandleBlockedPopupOnUIThread,
                                         blocked_params));
      return false;
    }
  }

#if defined(OS_ANDROID)
  if (SingleTabModeTabHelper::IsRegistered(render_process_id, opener_id)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&HandleSingleTabModeBlockOnUIThread,
                                       blocked_params));
    return false;
  }
#endif

  return true;
}

std::string ChromeContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, content::ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Check if it's an extension-created worker, in which case we want to use
  // the name of the extension.
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  const Extension* extension =
      io_data->GetExtensionInfoMap()->extensions().GetByID(url.host());
  return extension ? extension->name() : std::string();
}

void ChromeContentBrowserClient::ResourceDispatcherHostCreated() {
  return g_browser_process->ResourceDispatcherHostCreated();
}

// TODO(tommi): Rename from Get to Create.
content::SpeechRecognitionManagerDelegate*
    ChromeContentBrowserClient::GetSpeechRecognitionManagerDelegate() {
#if defined(ENABLE_INPUT_SPEECH)
  return new speech::ChromeSpeechRecognitionManagerDelegateBubbleUI();
#else
  // Platforms who don't implement x-webkit-speech (a.k.a INPUT_SPEECH) just
  // need the base delegate without the bubble UI.
  return new speech::ChromeSpeechRecognitionManagerDelegate();
#endif
}

net::NetLog* ChromeContentBrowserClient::GetNetLog() {
  return g_browser_process->net_log();
}

AccessTokenStore* ChromeContentBrowserClient::CreateAccessTokenStore() {
  return new ChromeAccessTokenStore();
}

bool ChromeContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

void ChromeContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* rvh, const GURL& url, WebPreferences* web_prefs) {
  Profile* profile = Profile::FromBrowserContext(
      rvh->GetProcess()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  // Fill per-script font preferences. These are not registered on Android
  // - http://crbug.com/308033.
#if !defined(OS_ANDROID)
  FillFontFamilyMap(prefs, prefs::kWebKitStandardFontFamilyMap,
                    &web_prefs->standard_font_family_map);
  FillFontFamilyMap(prefs, prefs::kWebKitFixedFontFamilyMap,
                    &web_prefs->fixed_font_family_map);
  FillFontFamilyMap(prefs, prefs::kWebKitSerifFontFamilyMap,
                    &web_prefs->serif_font_family_map);
  FillFontFamilyMap(prefs, prefs::kWebKitSansSerifFontFamilyMap,
                    &web_prefs->sans_serif_font_family_map);
  FillFontFamilyMap(prefs, prefs::kWebKitCursiveFontFamilyMap,
                    &web_prefs->cursive_font_family_map);
  FillFontFamilyMap(prefs, prefs::kWebKitFantasyFontFamilyMap,
                    &web_prefs->fantasy_font_family_map);
  FillFontFamilyMap(prefs, prefs::kWebKitPictographFontFamilyMap,
                    &web_prefs->pictograph_font_family_map);
#endif

  web_prefs->default_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web_prefs->default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web_prefs->minimum_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web_prefs->minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);

  web_prefs->default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web_prefs->javascript_can_open_windows_automatically =
      prefs->GetBoolean(prefs::kWebKitJavascriptCanOpenWindowsAutomatically);
  web_prefs->dom_paste_enabled =
      prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web_prefs->shrinks_standalone_images_to_fit =
      prefs->GetBoolean(prefs::kWebKitShrinksStandaloneImagesToFit);
  const base::DictionaryValue* inspector_settings =
      prefs->GetDictionary(prefs::kWebKitInspectorSettings);
  if (inspector_settings) {
    for (base::DictionaryValue::Iterator iter(*inspector_settings);
         !iter.IsAtEnd();
         iter.Advance()) {
      std::string value;
      if (iter.value().GetAsString(&value)) {
          web_prefs->inspector_settings.push_back(
              std::make_pair(iter.key(), value));
      }
    }
  }
  web_prefs->tabs_to_links = prefs->GetBoolean(prefs::kWebkitTabsToLinks);

  if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled))
    web_prefs->javascript_enabled = false;
  if (!prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled))
    web_prefs->web_security_enabled = false;
  if (!prefs->GetBoolean(prefs::kWebKitPluginsEnabled))
    web_prefs->plugins_enabled = false;
  if (!prefs->GetBoolean(prefs::kWebKitJavaEnabled))
    web_prefs->java_enabled = false;
  web_prefs->loads_images_automatically =
      prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);

  if (prefs->GetBoolean(prefs::kDisable3DAPIs))
    web_prefs->experimental_webgl_enabled = false;

  web_prefs->allow_displaying_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowDisplayingInsecureContent);
  web_prefs->allow_running_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowRunningInsecureContent);
#if defined(OS_ANDROID)
  web_prefs->font_scale_factor =
      static_cast<float>(prefs->GetDouble(prefs::kWebKitFontScaleFactor));
  web_prefs->device_scale_adjustment = GetDeviceScaleAdjustment();
  web_prefs->force_enable_zoom =
      prefs->GetBoolean(prefs::kWebKitForceEnableZoom);
#endif

#if defined(OS_ANDROID)
  web_prefs->password_echo_enabled =
      prefs->GetBoolean(prefs::kWebKitPasswordEchoEnabled);
#else
  web_prefs->password_echo_enabled = browser_defaults::kPasswordEchoEnabled;
#endif

  web_prefs->asynchronous_spell_checking_enabled = true;
  web_prefs->unified_textchecker_enabled = true;

  web_prefs->uses_universal_detector =
      prefs->GetBoolean(prefs::kWebKitUsesUniversalDetector);
  web_prefs->text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);
  web_prefs->hyperlink_auditing_enabled =
      prefs->GetBoolean(prefs::kEnableHyperlinkAuditing);

  // Make sure we will set the default_encoding with canonical encoding name.
  web_prefs->default_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByAliasName(
          web_prefs->default_encoding);
  if (web_prefs->default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web_prefs->default_encoding = prefs->GetString(prefs::kDefaultCharset);
  }
  DCHECK(!web_prefs->default_encoding.empty());

  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  extensions::ViewType view_type = extensions::GetViewType(web_contents);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (service) {
    const GURL& site_url = rvh->GetSiteInstance()->GetSiteURL();
    const Extension* extension =
        service->extensions()->GetByID(site_url.host());
    // Ensure that we are only granting extension preferences to URLs with
    // the correct scheme. Without this check, chrome-guest:// schemes used by
    // webview tags as well as hosts that happen to match the id of an
    // installed extension would get the wrong preferences.
    if (site_url.SchemeIs(extensions::kExtensionScheme)) {
      extension_webkit_preferences::SetPreferences(
          extension, view_type, web_prefs);
    }
  }

  if (view_type == extensions::VIEW_TYPE_NOTIFICATION) {
    web_prefs->allow_scripts_to_close_windows = true;
  } else if (view_type == extensions::VIEW_TYPE_BACKGROUND_CONTENTS) {
    // Disable all kinds of acceleration for background pages.
    // See http://crbug.com/96005 and http://crbug.com/96006
    web_prefs->force_compositing_mode = false;
    web_prefs->accelerated_compositing_enabled = false;
  }

#if defined(OS_CHROMEOS)
  // Override the default of suppressing HW compositing for WebUI pages for the
  // file manager, which is implemented using WebUI but wants HW acceleration
  // for video decode & render.
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      url.host() == file_manager::kFileManagerAppId) {
    web_prefs->accelerated_compositing_enabled = true;
    web_prefs->accelerated_2d_canvas_enabled = true;
  }
#endif
}

void ChromeContentBrowserClient::UpdateInspectorSetting(
    RenderViewHost* rvh, const std::string& key, const std::string& value) {
  content::BrowserContext* browser_context =
      rvh->GetProcess()->GetBrowserContext();
  DictionaryPrefUpdate update(
      Profile::FromBrowserContext(browser_context)->GetPrefs(),
      prefs::kWebKitInspectorSettings);
  base::DictionaryValue* inspector_settings = update.Get();
  inspector_settings->SetWithoutPathExpansion(
      key, base::Value::CreateStringValue(value));
}

void ChromeContentBrowserClient::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
  // Add the default URL handlers.
  handler->AddHandlerPair(&ExtensionWebUI::HandleChromeURLOverride,
                          BrowserURLHandler::null_handler());
  handler->AddHandlerPair(BrowserURLHandler::null_handler(),
                          &ExtensionWebUI::HandleChromeURLOverrideReverse);

  // about: handler. Must come before chrome: handler, since it will
  // rewrite about: urls to chrome: URLs and then expect chrome: to
  // actually handle them.
  handler->AddHandlerPair(&WillHandleBrowserAboutURL,
                          BrowserURLHandler::null_handler());

#if defined(OS_ANDROID)
  // Handler to rewrite chrome://newtab on Android.
  handler->AddHandlerPair(&chrome::android::HandleAndroidNewTabURL,
                          BrowserURLHandler::null_handler());
#else
  // Handler to rewrite chrome://newtab for InstantExtended.
  handler->AddHandlerPair(&chrome::HandleNewTabURLRewrite,
                          &chrome::HandleNewTabURLReverseRewrite);
#endif

  // chrome: & friends.
  handler->AddHandlerPair(&HandleWebUI, &HandleWebUIReverse);
}

void ChromeContentBrowserClient::ClearCache(RenderViewHost* rvh) {
  Profile* profile = Profile::FromBrowserContext(
      rvh->GetSiteInstance()->GetProcess()->GetBrowserContext());
  BrowsingDataRemover* remover =
      BrowsingDataRemover::CreateForUnboundedRange(profile);
  remover->Remove(BrowsingDataRemover::REMOVE_CACHE,
                  BrowsingDataHelper::UNPROTECTED_WEB);
  // BrowsingDataRemover takes care of deleting itself when done.
}

void ChromeContentBrowserClient::ClearCookies(RenderViewHost* rvh) {
  Profile* profile = Profile::FromBrowserContext(
      rvh->GetSiteInstance()->GetProcess()->GetBrowserContext());
  BrowsingDataRemover* remover =
      BrowsingDataRemover::CreateForUnboundedRange(profile);
  int remove_mask = BrowsingDataRemover::REMOVE_SITE_DATA;
  remover->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
  // BrowsingDataRemover takes care of deleting itself when done.
}

base::FilePath ChromeContentBrowserClient::GetDefaultDownloadDirectory() {
  return DownloadPrefs::GetDefaultDownloadDirectory();
}

std::string ChromeContentBrowserClient::GetDefaultDownloadName() {
  return l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME);
}

void ChromeContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
#if defined(ENABLE_PLUGINS)
  browser_host->GetPpapiHost()->AddHostFactoryFilter(
      scoped_ptr<ppapi::host::HostFactory>(
          new ChromeBrowserPepperHostFactory(browser_host)));
#endif
}

content::BrowserPpapiHost*
    ChromeContentBrowserClient::GetExternalBrowserPpapiHost(
        int plugin_process_id) {
  BrowserChildProcessHostIterator iter(PROCESS_TYPE_NACL_LOADER);
  while (!iter.Done()) {
    nacl::NaClProcessHost* host = static_cast<nacl::NaClProcessHost*>(
        iter.GetDelegate());
    if (host->process() &&
        host->process()->GetData().id == plugin_process_id) {
      // Found the plugin.
      return host->browser_ppapi_host();
    }
    ++iter;
  }
  return NULL;
}

bool ChromeContentBrowserClient::SupportsBrowserPlugin(
    content::BrowserContext* browser_context, const GURL& site_url) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserPluginForAllViewTypes))
    return true;

  if (content::HasWebUIScheme(site_url))
    return true;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return false;

  const Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(site_url);
  if (!extension)
    return false;

  return extension->HasAPIPermission(APIPermission::kWebView) ||
         extension->HasAPIPermission(APIPermission::kAdView);
}

bool ChromeContentBrowserClient::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params) {
#if defined(ENABLE_PLUGINS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = NULL;
  if (profile) {
    extension_set = extensions::ExtensionSystem::Get(profile)->
        extension_service()->extensions();
  }

  if (private_api) {
    // Access to private socket APIs is controlled by the whitelist.
    if (IsExtensionOrSharedModuleWhitelisted(url, extension_set,
                                             allowed_socket_origins_)) {
      return true;
    }
  } else {
    // Access to public socket APIs is controlled by extension permissions.
    if (url.is_valid() && url.SchemeIs(extensions::kExtensionScheme) &&
        extension_set) {
      const Extension* extension = extension_set->GetByID(url.host());
      if (extension) {
        if (params) {
          extensions::SocketPermission::CheckParam check_params(
              params->type, params->host, params->port);
          if (extensions::PermissionsData::CheckAPIPermissionWithParam(
                  extension, extensions::APIPermission::kSocket,
                  &check_params)) {
            return true;
          }
        } else {
          if (extensions::PermissionsData::HasAPIPermission(
                  extension, extensions::APIPermission::kSocket)) {
            return true;
          }
        }
      }
    }
  }

  // Allow both public and private APIs if the command line says so.
  return IsHostAllowedByCommandLine(url, extension_set,
                                    switches::kAllowNaClSocketAPI);
#else
  return false;
#endif
}

ui::SelectFilePolicy* ChromeContentBrowserClient::CreateSelectFilePolicy(
    WebContents* web_contents) {
  return new ChromeSelectFilePolicy(web_contents);
}

void ChromeContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_allowed_schemes) {
  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
      additional_allowed_schemes);
  additional_allowed_schemes->push_back(content::kChromeDevToolsScheme);
  additional_allowed_schemes->push_back(content::kChromeUIScheme);
  additional_allowed_schemes->push_back(extensions::kExtensionScheme);
}

void ChromeContentBrowserClient::GetURLRequestAutoMountHandlers(
    std::vector<fileapi::URLRequestAutoMountHandler>* handlers) {
#if !defined(OS_ANDROID)
  handlers->push_back(
      base::Bind(MediaFileSystemBackend::AttemptAutoMountForURLRequest));
#endif  // OS_ANDROID
}

void ChromeContentBrowserClient::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    ScopedVector<fileapi::FileSystemBackend>* additional_backends) {
#if !defined(OS_ANDROID)
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  additional_backends->push_back(new MediaFileSystemBackend(
      storage_partition_path,
      pool->GetSequencedTaskRunner(pool->GetNamedSequenceToken(
          MediaFileSystemBackend::kMediaTaskRunnerName)).get()));
#endif
#if defined(OS_CHROMEOS)
  fileapi::ExternalMountPoints* external_mount_points =
      content::BrowserContext::GetMountPoints(browser_context);
  DCHECK(external_mount_points);
  chromeos::FileSystemBackend* backend =
      new chromeos::FileSystemBackend(
          new drive::FileSystemBackendDelegate,
          browser_context->GetSpecialStoragePolicy(),
          external_mount_points,
          fileapi::ExternalMountPoints::GetSystemInstance());
  backend->AddSystemMountPoints();
  DCHECK(backend->CanHandleType(fileapi::kFileSystemTypeExternal));
  additional_backends->push_back(backend);
#endif

  additional_backends->push_back(
      new sync_file_system::SyncFileSystemBackend(
          Profile::FromBrowserContext(browser_context)));

#if defined(ENABLE_SERVICE_DISCOVERY)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePrivetStorage)) {
    additional_backends->push_back(
        new local_discovery::PrivetFileSystemBackend(
            fileapi::ExternalMountPoints::GetSystemInstance(),
            browser_context));
  }
#endif
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void ChromeContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const CommandLine& command_line,
    int child_process_id,
    std::vector<FileDescriptorInfo>* mappings) {
#if defined(OS_ANDROID)
  base::FilePath data_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &data_path);
  DCHECK(!data_path.empty());

  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::FilePath chrome_resources_pak =
      data_path.AppendASCII("chrome_100_percent.pak");
  base::File file(chrome_resources_pak, flags);
  DCHECK(file.IsValid());
  mappings->push_back(FileDescriptorInfo(kAndroidChrome100PercentPakDescriptor,
                                         FileDescriptor(file.Pass())));

  const std::string locale = GetApplicationLocale();
  base::FilePath locale_pak = ResourceBundle::GetSharedInstance().
      GetLocaleFilePath(locale, false);
  file.Initialize(locale_pak, flags);
  DCHECK(file.IsValid());
  mappings->push_back(FileDescriptorInfo(kAndroidLocalePakDescriptor,
                                         FileDescriptor(file.Pass())));

  base::FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  file.Initialize(resources_pack_path, flags);
  DCHECK(file.IsValid());
  mappings->push_back(FileDescriptorInfo(kAndroidUIResourcesPakDescriptor,
                                         FileDescriptor(file.Pass())));

  if (breakpad::IsCrashReporterEnabled()) {
    file = breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFile(
               child_process_id);
    if (file.IsValid()) {
      mappings->push_back(FileDescriptorInfo(kAndroidMinidumpDescriptor,
                                             FileDescriptor(file.Pass())));
    } else {
      LOG(ERROR) << "Failed to create file for minidump, crash reporting will "
                 "be disabled for this process.";
    }
  }

#else
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->push_back(FileDescriptorInfo(kCrashDumpSignal,
                                           FileDescriptor(crash_signal_fd,
                                                          false)));
  }
#endif  // defined(OS_ANDROID)
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

#if defined(OS_WIN)
const wchar_t* ChromeContentBrowserClient::GetResourceDllName() {
  return chrome::kBrowserResourcesDll;
}

void ChromeContentBrowserClient::PreSpawnRenderer(
    sandbox::TargetPolicy* policy,
    bool* success) {
  // This code is duplicated in nacl_exe_win_64.cc.
  // Allow the server side of a pipe restricted to the "chrome.nacl."
  // namespace so that it cannot impersonate other system or other chrome
  // service pipes.
  sandbox::ResultCode result = policy->AddRule(
      sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
      sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
      L"\\\\.\\pipe\\chrome.nacl.*");
  if (result != sandbox::SBOX_ALL_OK) {
    *success = false;
    return;
  }

  // Renderers need to send named pipe handles and shared memory
  // segment handles to NaCl loader processes.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                           sandbox::TargetPolicy::HANDLES_DUP_ANY,
                           L"File");
  if (result != sandbox::SBOX_ALL_OK) {
    *success = false;
    return;
  }
}
#endif

bool ChromeContentBrowserClient::IsPluginAllowedToCallRequestOSFileHandle(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if defined(ENABLE_PLUGINS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = NULL;
  if (profile) {
    extension_set = extensions::ExtensionSystem::Get(profile)->
        extension_service()->extensions();
  }
  // TODO(teravest): Populate allowed_file_handle_origins_ when FileIO is moved
  // from the renderer to the browser.
  return IsExtensionOrSharedModuleWhitelisted(url, extension_set,
                                              allowed_file_handle_origins_) ||
         IsHostAllowedByCommandLine(url, extension_set,
                                    switches::kAllowNaClFileHandleAPI);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsPluginAllowedToUseDevChannelAPIs() {
#if defined(ENABLE_PLUGINS)
  // Allow access for tests.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    return true;
  }

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  // Allow dev channel APIs to be used on "Canary", "Dev", and "Unknown"
  // releases of Chrome. Permitting "Unknown" allows these APIs to be used on
  // Chromium builds as well.
  return channel <= chrome::VersionInfo::CHANNEL_DEV;
#else
  return false;
#endif
}

#if defined(ENABLE_WEBRTC)
void ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
    CommandLine* to_command_line,
    const CommandLine& from_command_line,
    VersionInfo::Channel channel) {
#if defined(OS_ANDROID)
  const VersionInfo::Channel kMaxDisableEncryptionChannel =
      VersionInfo::CHANNEL_BETA;
#else
  const VersionInfo::Channel kMaxDisableEncryptionChannel =
      VersionInfo::CHANNEL_DEV;
#endif
  if (channel <= kMaxDisableEncryptionChannel) {
    static const char* const kWebRtcDevSwitchNames[] = {
      switches::kDisableWebRtcEncryption,
    };
    to_command_line->CopySwitchesFrom(from_command_line,
                                      kWebRtcDevSwitchNames,
                                      arraysize(kWebRtcDevSwitchNames));
  }
}
#endif  // defined(ENABLE_WEBRTC)

}  // namespace chrome
