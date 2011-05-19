// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include "base/command_line.h"
#include "chrome/app/breakpad_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_plugin_message_filter.h"
#include "chrome/browser/chrome_worker_message_filter.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/debugger/devtools_handler.h"
#include "chrome/browser/desktop_notification_handler.h"
#include "chrome/browser/extensions/extension_message_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/printing_message_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/renderer_host/chrome_render_view_host_observer.h"
#include "chrome/browser/renderer_host/text_input_client_message_filter.h"
#include "chrome/browser/search_engines/search_provider_install_state_message_filter.h"
#include "chrome/browser/spellcheck_message_filter.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/resource_context.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/bindings_policy.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_options.h"
#include "net/base/static_cookie_policy.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#include "chrome/browser/crash_handler_host_linux.h"
#endif  // OS_LINUX

namespace {

void InitRenderViewHostForExtensions(RenderViewHost* render_view_host) {
  // Note that due to GetEffectiveURL(), even hosted apps will have a
  // chrome-extension:// URL for their site, so we can ignore that wrinkle here.
  SiteInstance* site_instance = render_view_host->site_instance();
  const GURL& site = site_instance->site();
  RenderProcessHost* process = render_view_host->process();

  if (!site.SchemeIs(chrome::kExtensionScheme))
    return;

  Profile* profile = site_instance->browsing_instance()->profile();
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return;

  ExtensionProcessManager* process_manager =
      profile->GetExtensionProcessManager();
  CHECK(process_manager);

  // This can happen if somebody typos a chrome-extension:// URL.
  const Extension* extension = service->GetExtensionByURL(site);
  if (!extension)
    return;

  site_instance->GetProcess()->mark_is_extension_process();

  // Register the association between extension and process with
  // ExtensionProcessManager.
  process_manager->RegisterExtensionProcess(extension->id(), process->id());

  // Record which, if any, installed app is associated with this process.
  // TODO(aa): Totally lame to store this state in a global map in extension
  // service. Can we get it from EPM instead?
  if (extension->is_app())
    service->SetInstalledAppForRenderer(process->id(), extension);

  // Some extensions use chrome:// URLs.
  Extension::Type type = extension->GetType();
  if (type == Extension::TYPE_EXTENSION ||
      type == Extension::TYPE_PACKAGED_APP) {
    ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
        process->id(), chrome::kChromeUIScheme);
  }

  // Enable extension bindings for the renderer. Currently only extensions,
  // packaged apps, and hosted component apps use extension bindings.
  if (type == Extension::TYPE_EXTENSION ||
      type == Extension::TYPE_USER_SCRIPT ||
      type == Extension::TYPE_PACKAGED_APP ||
      (type == Extension::TYPE_HOSTED_APP &&
       extension->location() == Extension::COMPONENT)) {
    render_view_host->Send(new ExtensionMsg_ActivateExtension(extension->id()));
    render_view_host->AllowBindings(BindingsPolicy::EXTENSION);
  }
}

}

namespace chrome {

void ChromeContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {
  new ChromeRenderViewHostObserver(render_view_host);
  new DesktopNotificationHandler(render_view_host);
  new DevToolsHandler(render_view_host);
  new ExtensionMessageHandler(render_view_host);

  InitRenderViewHostForExtensions(render_view_host);
}

void ChromeContentBrowserClient::BrowserRenderProcessHostCreated(
    BrowserRenderProcessHost* host) {
  int id = host->id();
  Profile* profile = host->profile();
  host->channel()->AddFilter(new ChromeRenderMessageFilter(
      id, profile, profile->GetRequestContextForRenderProcess(id)));
  host->channel()->AddFilter(new PrintingMessageFilter());
  host->channel()->AddFilter(
      new SearchProviderInstallStateMessageFilter(id, profile));
  host->channel()->AddFilter(new SpellCheckMessageFilter(id));
#if defined(OS_MACOSX)
  host->channel()->AddFilter(new TextInputClientMessageFilter(host->id()));
#endif
}

void ChromeContentBrowserClient::PluginProcessHostCreated(
    PluginProcessHost* host) {
  host->AddFilter(new ChromePluginMessageFilter(host));
}

void ChromeContentBrowserClient::WorkerProcessHostCreated(
    WorkerProcessHost* host) {
  host->AddFilter(new ChromeWorkerMessageFilter(host));
}

content::WebUIFactory* ChromeContentBrowserClient::GetWebUIFactory() {
  return ChromeWebUIFactory::GetInstance();
}

GURL ChromeContentBrowserClient::GetEffectiveURL(Profile* profile,
                                                 const GURL& url) {
  // Get the effective URL for the given actual URL. If the URL is part of an
  // installed app, the effective URL is an extension URL with the ID of that
  // extension as the host. This has the effect of grouping apps together in
  // a common SiteInstance.
  if (!profile || !profile->GetExtensionService())
    return url;

  const Extension* extension =
      profile->GetExtensionService()->GetExtensionByWebExtent(url);
  if (!extension)
    return url;

  // If the URL is part of an extension's web extent, convert it to an
  // extension URL.
  return extension->GetResourceURL(url.path());
}

bool ChromeContentBrowserClient::IsURLSameAsAnySiteInstance(const GURL& url) {
  return url.spec() == chrome::kAboutKillURL ||
         url.spec() == chrome::kAboutHangURL ||
         url.spec() == chrome::kAboutShorthangURL;
}

GURL ChromeContentBrowserClient::GetAlternateErrorPageURL(
    const TabContents* tab) {
  GURL url;
  // Disable alternate error pages when in OffTheRecord/Incognito mode.
  if (tab->profile()->IsOffTheRecord())
    return url;

  PrefService* prefs = tab->profile()->GetPrefs();
  DCHECK(prefs);
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(
        GURL(google_util::kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

std::string ChromeContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return CharacterEncoding::GetCanonicalEncodingNameByAliasName(alias_name);
}

void ChromeContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
#if defined(USE_LINUX_BREAKPAD)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
        child_process_logging::GetClientId() + "," + base::GetLinuxDistro());
  }
#elif defined(OS_MACOSX)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
                                    child_process_logging::GetClientId());
  }
#endif  // OS_MACOSX

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kExtensionProcess ||
      process_type == switches::kRendererProcess) {
    const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
    FilePath user_data_dir =
        browser_command_line.GetSwitchValuePath(switches::kUserDataDir);
    if (!user_data_dir.empty())
      command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
#if defined(OS_CHROMEOS)
    const std::string& login_profile =
        browser_command_line.GetSwitchValueASCII(switches::kLoginProfile);
    if (!login_profile.empty())
      command_line->AppendSwitchASCII(switches::kLoginProfile, login_profile);
#endif

    RenderProcessHost* process = RenderProcessHost::FromID(child_process_id);

    PrefService* prefs = process->profile()->GetPrefs();
    // Currently this pref is only registered if applied via a policy.
    if (prefs->HasPrefPath(prefs::kDisable3DAPIs) &&
        prefs->GetBoolean(prefs::kDisable3DAPIs)) {
      // Turn this policy into a command line switch.
      command_line->AppendSwitch(switches::kDisable3DAPIs);
    }

    // Disable client-side phishing detection in the renderer if it is disabled
    // in the browser process.
    if (!g_browser_process->safe_browsing_detection_service())
      command_line->AppendSwitch(switches::kDisableClientSidePhishingDetection);
  }
}

std::string ChromeContentBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeContentBrowserClient::AllowAppCache(
    const GURL& manifest_url, const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  ContentSetting setting = io_data->GetHostContentSettingsMap()->
      GetContentSetting(manifest_url, CONTENT_SETTINGS_TYPE_COOKIES, "");
  DCHECK(setting != CONTENT_SETTING_DEFAULT);
  return setting != CONTENT_SETTING_BLOCK;
}

bool ChromeContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool allow = true;
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  if (io_data->GetHostContentSettingsMap()->BlockThirdPartyCookies()) {
    bool strict = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kBlockReadingThirdPartyCookies);
    net::StaticCookiePolicy policy(strict ?
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    int rv = policy.CanGetCookies(url, first_party);
    DCHECK_NE(net::ERR_IO_PENDING, rv);
    if (rv != net::OK)
      allow = false;
  }

  if (allow) {
    ContentSetting setting = io_data->GetHostContentSettingsMap()->
        GetContentSetting(url, CONTENT_SETTINGS_TYPE_COOKIES, "");
    allow = setting == CONTENT_SETTING_ALLOW ||
        setting == CONTENT_SETTING_SESSION_ONLY;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::CookiesRead,
          render_process_id, render_view_id, url, cookie_list, !allow));
  return allow;
}

bool ChromeContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool allow = true;
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  if (io_data->GetHostContentSettingsMap()->BlockThirdPartyCookies()) {
    bool strict = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kBlockReadingThirdPartyCookies);
    net::StaticCookiePolicy policy(strict ?
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    int rv = policy.CanSetCookie(url, first_party, cookie_line);
    if (rv != net::OK)
      allow = false;
  }

  if (allow) {
    ContentSetting setting = io_data->GetHostContentSettingsMap()->
        GetContentSetting(url, CONTENT_SETTINGS_TYPE_COOKIES, "");

    if (setting == CONTENT_SETTING_SESSION_ONLY)
      options->set_force_session();

    allow = setting == CONTENT_SETTING_ALLOW ||
        setting == CONTENT_SETTING_SESSION_ONLY;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::CookieChanged,
          render_process_id, render_view_id, url, cookie_line, *options,
          !allow));
  return allow;
}

#if defined(OS_LINUX)
int ChromeContentBrowserClient::GetCrashSignalFD(
    const std::string& process_type) {
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
#endif

}  // namespace chrome
