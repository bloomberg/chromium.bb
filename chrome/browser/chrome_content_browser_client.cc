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
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/app/breakpad_mac.h"
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
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/geolocation/chrome_access_token_store.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/guestview/adview/adview_guest.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"
#include "chrome/browser/nacl_host/nacl_host_message_filter.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugins/plugin_info_message_filter.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
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
#include "chrome/browser/user_style_sheet_watcher.h"
#include "chrome/browser/user_style_sheet_watcher_factory.h"
#include "chrome/browser/validation_message_message_filter.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest_handlers/app_isolation_info.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "chrome/common/extensions/web_accessible_resources_handler.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_constants.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ppapi/host/ppapi_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center_util.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/common/webpreferences.h"

#if defined(OS_WIN)
#include "chrome/browser/chrome_browser_main_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_main_mac.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter_mac.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#include "chrome/browser/chromeos/drive/file_system_backend_delegate.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chromeos/chromeos_switches.h"
#elif defined(OS_LINUX)
#include "chrome/browser/chrome_browser_main_linux.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/android/crash_dump_manager.h"
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "chrome/browser/chrome_browser_main_android.h"
#include "chrome/common/descriptors_android.h"
#elif defined(OS_POSIX)
#include "chrome/browser/chrome_browser_main_posix.h"
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD) || defined(OS_ANDROID)
#include "base/linux_util.h"
#include "chrome/browser/crash_handler_host_linux.h"
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if defined(OS_ANDROID)
#include "ui/base/ui_base_paths.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
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

#if defined(FILE_MANAGER_EXTENSION)
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

using WebKit::WebWindowFeatures;
using base::FileDescriptor;
using content::AccessTokenStore;
using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::ChildProcessSecurityPolicy;
using content::FileDescriptorInfo;
using content::QuotaPermissionContext;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using extensions::APIPermission;
using extensions::Extension;
using extensions::Manifest;
using message_center::NotifierId;

namespace {

// Cached version of the locale so we can return the locale on the I/O
// thread.
base::LazyInstance<std::string> g_io_thread_application_locale;

#if defined(ENABLE_PLUGINS)
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
    if (url->SchemeIs(chrome::kChromeUIScheme) &&
        (url->DomainIs(chrome::kChromeUIBookmarksHost) ||
         url->DomainIs(chrome::kChromeUIHistoryHost))) {
      // Rewrite with new tab URL
      *url = GURL(chrome::kChromeUINewTabURL);
    }
  }
#endif

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url->SchemeIs(chrome::kChromeInternalScheme)) {
    // Rewrite it with the proper new tab URL.
    *url = GURL(chrome::kChromeUINewTabURL);
  }

  return true;
}

// Reverse URL handler for Web UI. Maps "chrome://chrome/foo/" to
// "chrome://foo/".
bool HandleWebUIReverse(GURL* url, content::BrowserContext* browser_context) {
  if (!url->is_valid() || !url->SchemeIs(chrome::kChromeUIScheme))
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

// Fills |map| with the per-script font prefs under path |map_name|.
void FillFontFamilyMap(const PrefService* prefs,
                       const char* map_name,
                       webkit_glue::ScriptFontFamilyMap* map) {
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::string pref_name = base::StringPrintf("%s.%s", map_name, script);
    std::string font_family = prefs->GetString(pref_name.c_str());
    if (!font_family.empty())
      (*map)[script] = UTF8ToUTF16(font_family);
  }
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
int GetCrashSignalFD(const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kExtensionProcess)) {
    ExtensionCrashHandlerHostLinux* crash_handler =
        ExtensionCrashHandlerHostLinux::GetInstance();
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess)
    return RendererCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  if (process_type == switches::kPluginProcess)
    return PluginCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  if (process_type == switches::kPpapiPluginProcess)
    return PpapiCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  if (process_type == switches::kGpuProcess)
    return GpuCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  return -1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

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
#endif  // defined(OS_ANDROID)

}  // namespace

namespace chrome {

ChromeContentBrowserClient::ChromeContentBrowserClient() {
#if defined(ENABLE_PLUGINS)
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
  registry->RegisterBooleanPref(
      prefs::kEnableMemoryInfo,
      false,
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
  if (site.SchemeIs(chrome::kGuestScheme))
    partition_id = site.spec();

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

  // For the webview tag, we create special guest processes, which host the
  // tag content separately from the main application that embeds the tag.
  // A webview tag can specify both the partition name and whether the storage
  // for that partition should be persisted. Each tag gets a SiteInstance with
  // a specially formatted URL, based on the application it is hosted by and
  // the partition requested by it. The format for that URL is:
  // chrome-guest://partition_domain/persist?partition_name
  if (site.SchemeIs(chrome::kGuestScheme)) {
    // Since guest URLs are only used for packaged apps, there must be an app
    // id in the URL.
    CHECK(site.has_host());
    *partition_domain = site.host();
    // Since persistence is optional, the path must either be empty or the
    // literal string.
    *in_memory = (site.path() != "/persist");
    // The partition name is user supplied value, which we have encoded when the
    // URL was created, so it needs to be decoded.
    *partition_name = net::UnescapeURLComponent(site.query(),
                                                net::UnescapeRule::NORMAL);
  } else if (site.SchemeIs(extensions::kExtensionScheme)) {
    // If |can_be_default| is false, the caller is stating that the |site|
    // should be parsed as if it had isolated storage. In particular it is
    // important to NOT check ExtensionService for the is_storage_isolated()
    // attribute because this code path is run during Extension uninstall
    // to do cleanup after the Extension has already been unloaded from the
    // ExtensionService.
    bool is_isolated = !can_be_default;
    if (can_be_default) {
      const Extension* extension = NULL;
      Profile* profile = Profile::FromBrowserContext(browser_context);
      ExtensionService* extension_service =
          extensions::ExtensionSystem::Get(profile)->extension_service();
      if (extension_service) {
        extension =
            extension_service->extensions()->GetExtensionOrAppByURL(site);
        if (extension &&
            extensions::AppIsolationInfo::HasIsolatedStorage(extension)) {
          is_isolated = true;
        }
      }
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
    WebContents* guest_web_contents,
    WebContents* opener_web_contents,
    content::BrowserPluginGuestDelegate** guest_delegate,
    scoped_ptr<base::DictionaryValue> extra_params) {
  if (opener_web_contents) {
    GuestView* guest = GuestView::FromWebContents(opener_web_contents);
    if (!guest) {
      NOTREACHED();
      return;
    }

    switch (guest->GetViewType()) {
      case GuestView::WEBVIEW: {
        *guest_delegate = new WebViewGuest(guest_web_contents);
        break;
      }
      case GuestView::ADVIEW: {
        *guest_delegate = new AdViewGuest(guest_web_contents);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
    return;
  }

  if (!extra_params) {
    NOTREACHED();
    return;
  }
  std::string api_type;
  extra_params->GetString(guestview::kParameterApi, &api_type);

  if (api_type == "adview") {
    *guest_delegate  = new AdViewGuest(guest_web_contents);
  } else if (api_type == "webview") {
    *guest_delegate = new WebViewGuest(guest_web_contents);
  } else {
    NOTREACHED();
  }
}

void ChromeContentBrowserClient::GuestWebContentsAttached(
    WebContents* guest_web_contents,
    WebContents* embedder_web_contents,
    const base::DictionaryValue& extra_params) {
  Profile* profile = Profile::FromBrowserContext(
      embedder_web_contents->GetBrowserContext());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service) {
    NOTREACHED();
    return;
  }
  const GURL& url = embedder_web_contents->GetSiteInstance()->GetSiteURL();
  const Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  if (!extension) {
    // It's ok to return here, since we could be running a browser plugin
    // outside an extension, and don't need to attach a
    // BrowserPluginGuestDelegate in that case;
    // e.g. running with flag --enable-browser-plugin-for-all-view-types.
    return;
  }

  GuestView* guest = GuestView::FromWebContents(guest_web_contents);
  if (!guest) {
    NOTREACHED();
    return;
  }
  guest->Attach(embedder_web_contents, extension->id(), extra_params);
}

void ChromeContentBrowserClient::RenderProcessHostCreated(
    content::RenderProcessHost* host) {
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  net::URLRequestContextGetter* context =
      profile->GetRequestContextForRenderProcess(id);

  host->GetChannel()->AddFilter(new ChromeRenderMessageFilter(
      id, profile, context));
#if defined(ENABLE_PLUGINS)
  host->GetChannel()->AddFilter(new PluginInfoMessageFilter(id, profile));
#endif
#if defined(ENABLE_PRINTING)
  host->GetChannel()->AddFilter(new PrintingMessageFilter(id, profile));
#endif
  host->GetChannel()->AddFilter(
      new SearchProviderInstallStateMessageFilter(id, profile));
#if defined(ENABLE_SPELLCHECK)
  host->GetChannel()->AddFilter(new SpellCheckMessageFilter(id));
#endif
#if defined(OS_MACOSX)
  host->GetChannel()->AddFilter(new SpellCheckMessageFilterMac(id));
#endif
  host->GetChannel()->AddFilter(new ChromeNetBenchmarkingMessageFilter(
      id, profile, context));
  host->GetChannel()->AddFilter(
      new prerender::PrerenderMessageFilter(id, profile));
  host->GetChannel()->AddFilter(new ValidationMessageMessageFilter(id));
  host->GetChannel()->AddFilter(new TtsMessageFilter(id, profile));
#if defined(ENABLE_WEBRTC)
  host->GetChannel()->AddFilter(new WebRtcLoggingHandlerHost());
#endif
#if !defined(DISABLE_NACL)
  ExtensionInfoMap* extension_info_map =
      extensions::ExtensionSystem::Get(profile)->info_map();
  host->GetChannel()->AddFilter(new NaClHostMessageFilter(
      id, profile->IsOffTheRecord(),
      profile->GetPath(), extension_info_map,
      context));
#endif

  host->Send(new ChromeViewMsg_SetIsIncognitoProcess(
      profile->IsOffTheRecord()));

  SendExtensionWebRequestStatusToHost(host);

  RendererContentSettingRules rules;
  GetRendererContentSettingRules(profile->GetHostContentSettingsMap(), &rules);
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

// These are treated as WebUI schemes but do not get WebUI bindings.
void ChromeContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(chrome::kChromeSearchScheme);
}

net::URLRequestContextGetter*
ChromeContentBrowserClient::CreateRequestContext(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->CreateRequestContext(protocol_handlers);
}

net::URLRequestContextGetter*
ChromeContentBrowserClient::CreateRequestContextForStoragePartition(
    content::BrowserContext* browser_context,
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->CreateRequestContextForStoragePartition(
      partition_path, in_memory, protocol_handlers);
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
      !service->process_map()->Contains(new_extension->id(),
                                        process_host->GetID())) {
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
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile);
  if (signin_manager && signin_manager->IsSigninProcess(process_host->GetID()))
    return SigninManager::IsWebBasedSigninFlowURL(site_url);
#endif

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extensions::ProcessMap* process_map = service->process_map();

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
    ExtensionProcessManager* epm =
        extensions::ExtensionSystem::Get(profiles[i])->process_manager();
    for (ExtensionProcessManager::const_iterator iter =
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
    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfile(profile);
    if (signin_manager)
      signin_manager->SetSigninProcess(site_instance->GetProcess()->GetID());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ExtensionInfoMap::SetSigninProcess,
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

  service->process_map()->Insert(extension->id(),
                                 site_instance->GetProcess()->GetID(),
                                 site_instance->GetId());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ExtensionInfoMap::RegisterExtensionProcess,
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

  service->process_map()->Remove(extension->id(),
                                 site_instance->GetProcess()->GetID(),
                                 site_instance->GetId());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ExtensionInfoMap::UnregisterExtensionProcess,
                 extensions::ExtensionSystem::Get(profile)->info_map(),
                 extension->id(),
                 site_instance->GetProcess()->GetID(),
                 site_instance->GetId()));
}

bool ChromeContentBrowserClient::ShouldSwapProcessesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_url,
    const GURL& new_url) {
  if (current_url.is_empty()) {
    // Always choose a new process when navigating to extension URLs. The
    // process grouping logic will combine all of a given extension's pages
    // into the same process.
    if (new_url.SchemeIs(extensions::kExtensionScheme))
      return true;

    return false;
  }

  // Also, we must switch if one is an extension and the other is not the exact
  // same extension.
  if (current_url.SchemeIs(extensions::kExtensionScheme) ||
      new_url.SchemeIs(extensions::kExtensionScheme)) {
    if (current_url.GetOrigin() != new_url.GetOrigin())
      return true;
  }

  // The checks below only matter if we can retrieve which extensions are
  // installed.
  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return false;

  // We must swap if the URL is for an extension and we are not using an
  // extension process.
  const Extension* new_extension =
      service->extensions()->GetExtensionOrAppByURL(new_url);
  // Ignore all hosted apps except the Chrome Web Store, since they do not
  // require their own BrowsingInstance (e.g., postMessage is ok).
  if (new_extension &&
      new_extension->is_hosted_app() &&
      new_extension->id() != extension_misc::kWebStoreAppId)
    new_extension = NULL;
  if (new_extension &&
      site_instance->HasProcess() &&
      !service->process_map()->Contains(new_extension->id(),
                                        site_instance->GetProcess()->GetID()))
    return true;

  return false;
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
#if defined(OS_MACOSX)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
                                    child_process_logging::GetClientId());
  }
#elif defined(OS_POSIX)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
        child_process_logging::GetClientId() + "," + base::GetLinuxDistro());
  }

#endif  // OS_MACOSX

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

  static const char* const kCommonSwitchNames[] = {
    switches::kChromeFrame,
    switches::kUserDataDir,  // Make logs go to the right file.
  };
  command_line->CopySwitchesFrom(browser_command_line, kCommonSwitchNames,
                                 arraysize(kCommonSwitchNames));

  if (process_type == switches::kRendererProcess) {
#if defined(OS_CHROMEOS)
    const std::string& login_profile =
        browser_command_line.GetSwitchValueASCII(
            chromeos::switches::kLoginProfile);
    if (!login_profile.empty())
      command_line->AppendSwitchASCII(
          chromeos::switches::kLoginProfile, login_profile);
#endif

    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(child_process_id);
    if (process) {
      Profile* profile = Profile::FromBrowserContext(
          process->GetBrowserContext());
      ExtensionService* extension_service =
          extensions::ExtensionSystem::Get(profile)->extension_service();
      if (extension_service) {
        extensions::ProcessMap* process_map = extension_service->process_map();
        if (process_map && process_map->Contains(process->GetID()))
          command_line->AppendSwitch(switches::kExtensionProcess);
      }

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
      SigninManager* signin_manager =
          SigninManagerFactory::GetForProfile(profile);
      if (signin_manager && signin_manager->IsSigninProcess(process->GetID()))
        command_line->AppendSwitch(switches::kSigninProcess);
#endif
    }

    if (message_center::IsRichNotificationEnabled())
      command_line->AppendSwitch(switches::kDisableHTMLNotifications);

    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
      autofill::switches::kDisableInteractiveAutocomplete,
      autofill::switches::kEnableExperimentalFormFilling,
      autofill::switches::kEnableInteractiveAutocomplete,
      extensions::switches::kAllowLegacyExtensionManifests,
      extensions::switches::kAllowScriptingGallery,
      extensions::switches::kEnableExperimentalExtensionApis,
      extensions::switches::kExtensionsOnChromeURLs,
      switches::kAllowHTTPBackgroundPage,
      // TODO(victorhsieh): remove the following flag once we move PPAPI FileIO
      // to browser.
      switches::kAllowNaClFileHandleAPI,
      switches::kAppsCheckoutURL,
      switches::kAppsGalleryURL,
      switches::kCloudPrintServiceURL,
      switches::kDebugPrint,
      switches::kDisableBundledPpapiFlash,
      switches::kDisableExtensionsResourceWhitelist,
      switches::kDisablePnacl,
      switches::kDisableScriptedPrintThrottling,
      switches::kEnableAdview,
      switches::kEnableAdviewSrcAttribute,
      switches::kEnableAppWindowControls,
      switches::kEnableBenchmarking,
      switches::kEnableIPCFuzzing,
      switches::kEnableNaCl,
      switches::kEnableNetBenchmarking,
      switches::kEnablePasswordGeneration,
      switches::kEnableWatchdog,
      switches::kMemoryProfiling,
      switches::kMessageLoopHistogrammer,
      switches::kNoJsRandomness,
      switches::kPlaybackMode,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashInProcess,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kProfilingAtStart,
      switches::kProfilingFile,
      switches::kProfilingFlush,
      switches::kRecordMode,
      switches::kSilentDumpOnDCHECK,
      switches::kSpdyProxyAuthOrigin,
      switches::kTranslateSecurityOrigin,
      switches::kWhitelistedExtensionID,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kUtilityProcess) {
    static const char* const kSwitchNames[] = {
      extensions::switches::kEnableExperimentalExtensionApis,
      extensions::switches::kExtensionsOnChromeURLs,
      switches::kAllowHTTPBackgroundPage,
      switches::kWhitelistedExtensionID,
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
      switches::kPpapiFlashInProcess,
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
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  bool allow = io_data->GetCookieSettings()->
      IsReadingCookieAllowed(url, first_party);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::CookiesRead, render_process_id,
                 render_view_id, url, first_party, cookie_list, !allow));
  return allow;
}

bool ChromeContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    content::ResourceContext* context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, first_party);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::CookieChanged, render_process_id,
                 render_view_id, url, first_party, cookie_line, *options,
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
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

  // Record access to database for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_views.begin(); i != render_views.end(); ++i) {
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
    const std::vector<std::pair<int, int> >& render_views) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

  // Record access to file system for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_views.begin(); i != render_views.end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::FileSystemAccessed,
                   i->first, i->second, url, !allow));
  }

  return allow;
}

bool ChromeContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  CookieSettings* cookie_settings = io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

  // Record access to IndexedDB for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_views.begin(); i != render_views.end(); ++i) {
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
    int render_view_id,
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
  WebContents* tab = tab_util::GetWebContentsByID(
      render_process_id, render_view_id);
  if (!tab) {
    NOTREACHED();
    return;
  }
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(tab->GetBrowserContext()));
  if (prerender_manager && prerender_manager->IsWebContentsPrerendering(tab,
                                                                        NULL)) {
    if (prerender_manager->prerender_tracker()->TryCancel(
            render_process_id, render_view_id,
            prerender::FINAL_STATUS_SSL_ERROR)) {
      *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
      return;
    }
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
    int render_view_id,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
  WebContents* tab = tab_util::GetWebContentsByID(
      render_process_id, render_view_id);
  if (!tab) {
    NOTREACHED();
    return;
  }

  GURL requesting_url("https://" + cert_request_info->host_and_port);
  DCHECK(requesting_url.is_valid()) << "Invalid URL string: https://"
                                    << cert_request_info->host_and_port;

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  scoped_ptr<Value> filter(
      profile->GetHostContentSettingsMap()->GetWebsiteSetting(
          requesting_url,
          requesting_url,
          CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
          std::string(), NULL));

  if (filter.get()) {
    // Try to automatically select a client certificate.
    if (filter->IsType(Value::TYPE_DICTIONARY)) {
      DictionaryValue* filter_dict =
          static_cast<DictionaryValue*>(filter.get());

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
  ExtensionInfoMap* extension_info_map =
      extensions::ExtensionSystem::Get(profile)->info_map();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  const Extension* extension = NULL;
  if (extension_info_map) {
    ExtensionSet extensions;
    extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
        source_origin, render_process_id,
        extensions::APIPermission::kNotification, &extensions);
    for (ExtensionSet::const_iterator iter = extensions.begin();
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

WebKit::WebNotificationPresenter::Permission
    ChromeContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_origin,
        content::ResourceContext* context,
        int render_process_id) {
#if defined(ENABLE_NOTIFICATIONS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Sometimes a notification may be invoked during the shutdown.
  // See http://crbug.com/256638
  if (browser_shutdown::IsTryingToQuit())
    return WebKit::WebNotificationPresenter::PermissionNotAllowed;

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);

  DesktopNotificationService* notification_service =
      io_data->GetNotificationService();
  if (notification_service) {
    ExtensionInfoMap* extension_info_map = io_data->GetExtensionInfoMap();
    ExtensionSet extensions;
    extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
        source_origin, render_process_id,
        extensions::APIPermission::kNotification, &extensions);
    for (ExtensionSet::const_iterator iter = extensions.begin();
         iter != extensions.end(); ++iter) {
      NotifierId notifier_id(NotifierId::APPLICATION, (*iter)->id());
      if (notification_service->IsNotifierEnabled(notifier_id))
        return WebKit::WebNotificationPresenter::PermissionAllowed;
    }

    return notification_service->HasPermission(source_origin);
  }

  return WebKit::WebNotificationPresenter::PermissionNotAllowed;
#else
  return WebKit::WebNotificationPresenter::PermissionAllowed;
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
  ExtensionInfoMap* map = io_data->GetExtensionInfoMap();

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

  if (g_browser_process->prerender_tracker() &&
      g_browser_process->prerender_tracker()->TryCancelOnIOThread(
          render_process_id,
          opener_id,
          prerender::FINAL_STATUS_CREATE_NEW_WINDOW)) {
    return false;
  }

  if (is_guest)
    return true;

  // Exempt extension processes from popup blocking.
  if (map->process_map().Contains(render_process_id))
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
  const DictionaryValue* inspector_settings =
      prefs->GetDictionary(prefs::kWebKitInspectorSettings);
  if (inspector_settings) {
    for (DictionaryValue::Iterator iter(*inspector_settings); !iter.IsAtEnd();
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

  web_prefs->memory_info_enabled =
      prefs->GetBoolean(prefs::kEnableMemoryInfo);
  web_prefs->allow_displaying_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowDisplayingInsecureContent);
  web_prefs->allow_running_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowRunningInsecureContent);
#if defined(OS_ANDROID)
  web_prefs->font_scale_factor =
      static_cast<float>(prefs->GetDouble(prefs::kWebKitFontScaleFactor));
  web_prefs->force_enable_zoom =
      prefs->GetBoolean(prefs::kWebKitForceEnableZoom);
#endif

#if defined(OS_ANDROID)
  web_prefs->password_echo_enabled =
      prefs->GetBoolean(prefs::kWebKitPasswordEchoEnabled);
#else
  web_prefs->password_echo_enabled = browser_defaults::kPasswordEchoEnabled;
#endif

#if defined(OS_CHROMEOS)
  // Enable password echo during OOBE when keyboard driven flag is set.
  if (chromeos::UserManager::IsInitialized() &&
      !chromeos::UserManager::Get()->IsUserLoggedIn() &&
      !chromeos::StartupUtils::IsOobeCompleted() &&
      chromeos::system::keyboard_settings::ForceKeyboardDrivenUINavigation()) {
    web_prefs->password_echo_enabled = true;
  }
#endif

#if defined(OS_ANDROID)
  web_prefs->user_style_sheet_enabled = false;
#else
  // The user stylesheet watcher may not exist in a testing profile.
  UserStyleSheetWatcher* user_style_sheet_watcher =
      UserStyleSheetWatcherFactory::GetForProfile(profile).get();
  if (user_style_sheet_watcher) {
    web_prefs->user_style_sheet_enabled = true;
    web_prefs->user_style_sheet_location =
        user_style_sheet_watcher->user_style_sheet();
  } else {
    web_prefs->user_style_sheet_enabled = false;
  }
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
    const GURL& url = rvh->GetSiteInstance()->GetSiteURL();
    const Extension* extension = service->extensions()->GetByID(url.host());
    // Ensure that we are only granting extension preferences to URLs with
    // the correct scheme. Without this check, chrome-guest:// schemes used by
    // webview tags as well as hosts that happen to match the id of an
    // installed extension would get the wrong preferences.
    if (url.SchemeIs(extensions::kExtensionScheme)) {
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

#if defined(FILE_MANAGER_EXTENSION)
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
  DictionaryValue* inspector_settings = update.Get();
  inspector_settings->SetWithoutPathExpansion(key,
                                              Value::CreateStringValue(value));
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

  // Handler to rewrite chrome://newtab for InstantExtended.
  handler->AddHandlerPair(&chrome::HandleNewTabURLRewrite,
                          &chrome::HandleNewTabURLReverseRewrite);

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
    NaClProcessHost* host = static_cast<NaClProcessHost*>(iter.GetDelegate());
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
    const content::SocketPermissionRequest& params) {
#if defined(ENABLE_PLUGINS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const ExtensionSet* extension_set = NULL;
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
        extensions::SocketPermission::CheckParam check_params(
            params.type, params.host, params.port);
        if (extensions::PermissionsData::CheckAPIPermissionWithParam(
                extension, extensions::APIPermission::kSocket, &check_params)) {
          return true;
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
  additional_allowed_schemes->push_back(kChromeUIScheme);
  additional_allowed_schemes->push_back(extensions::kExtensionScheme);
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
          new drive::FileSystemBackendDelegate(browser_context),
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

  int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  base::FilePath chrome_resources_pak =
      data_path.AppendASCII("chrome_100_percent.pak");
  base::PlatformFile f =
      base::CreatePlatformFile(chrome_resources_pak, flags, NULL, NULL);
  DCHECK(f != base::kInvalidPlatformFileValue);
  mappings->push_back(FileDescriptorInfo(kAndroidChrome100PercentPakDescriptor,
                                         FileDescriptor(f, true)));

  const std::string locale = GetApplicationLocale();
  base::FilePath locale_pak = ResourceBundle::GetSharedInstance().
      GetLocaleFilePath(locale, false);
  f = base::CreatePlatformFile(locale_pak, flags, NULL, NULL);
  DCHECK(f != base::kInvalidPlatformFileValue);
  mappings->push_back(FileDescriptorInfo(kAndroidLocalePakDescriptor,
                                         FileDescriptor(f, true)));

  base::FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  f = base::CreatePlatformFile(resources_pack_path, flags, NULL, NULL);
  DCHECK(f != base::kInvalidPlatformFileValue);
  mappings->push_back(FileDescriptorInfo(kAndroidUIResourcesPakDescriptor,
                                         FileDescriptor(f, true)));

  if (IsCrashReporterEnabled()) {
    f = CrashDumpManager::GetInstance()->CreateMinidumpFile(child_process_id);
    if (f == base::kInvalidPlatformFileValue) {
      LOG(ERROR) << "Failed to create file for minidump, crash reporting will "
                 "be disabled for this process.";
    } else {
      mappings->push_back(FileDescriptorInfo(kAndroidMinidumpDescriptor,
                                             FileDescriptor(f, true)));
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

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    ChromeContentBrowserClient::GetCryptoPasswordDelegate(
        const GURL& url) {
  return chrome::NewCryptoModuleBlockingDialogDelegate(
      chrome::kCryptoModulePasswordKeygen, url.host());
}
#endif

}  // namespace chrome
