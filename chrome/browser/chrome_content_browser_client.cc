// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include "base/command_line.h"
#include "chrome/app/breakpad_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_plugin_message_filter.h"
#include "chrome/browser/chrome_quota_permission_context.h"
#include "chrome/browser/chrome_worker_message_filter.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/debugger/devtools_handler.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/printing_message_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/renderer_host/chrome_render_view_host_observer.h"
#include "chrome/browser/renderer_host/text_input_client_message_filter.h"
#include "chrome/browser/search_engines/search_provider_install_state_message_filter.h"
#include "chrome/browser/spellcheck_message_filter.h"
#include "chrome/browser/ssl/ssl_add_cert_handler.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/resource_context.h"
#include "content/browser/site_instance.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/bindings_policy.h"
#include "content/common/desktop_notification_messages.h"
#include "grit/app_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_options.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#include "chrome/browser/crash_handler_host_linux.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif

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

  if (extension->is_app()) {
    render_view_host->Send(
        new ExtensionMsg_ActivateApplication(extension->id()));
    // Record which, if any, installed app is associated with this process.
    // TODO(aa): Totally lame to store this state in a global map in extension
    // service. Can we get it from EPM instead?
    service->SetInstalledAppForRenderer(process->id(), extension);
  }

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

  host->Send(new ViewMsg_SetIsIncognitoProcess(profile->IsOffTheRecord()));
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
  return url == GURL(chrome::kChromeUICrashURL) ||
         url == GURL(chrome::kChromeUIKillURL) ||
         url == GURL(chrome::kChromeUIHangURL) ||
         url == GURL(chrome::kChromeUIShorthangURL);
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

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (process_type == switches::kExtensionProcess ||
      process_type == switches::kRendererProcess) {
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

    static const char* const kSwitchNames[] = {
      switches::kAllowHTTPBackgroundPage,
      switches::kAllowScriptingGallery,
      switches::kAppsCheckoutURL,
      switches::kAppsGalleryURL,
      switches::kDebugPrint,
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
      // Enabled by default in Google Chrome builds, except on CrOS.
      switches::kDisablePrintPreview,
#else
      // Disabled by default in Chromium builds and on CrOS.
      switches::kEnablePrintPreview,
#endif
      switches::kDomAutomationController,
      switches::kDumpHistogramsOnExit,
      switches::kEnableClickToPlay,
      switches::kEnableCrxlessWebApps,
      switches::kEnableExperimentalExtensionApis,
      switches::kEnableInBrowserThumbnailing,
      switches::kEnableIPCFuzzing,
      switches::kEnableNaCl,
      switches::kEnableRemoting,
      switches::kEnableResourceContentSettings,
      switches::kEnableSearchProviderApiV2,
      switches::kEnableWatchdog,
      switches::kExperimentalSpellcheckerFeatures,
      switches::kMemoryProfiling,
      switches::kMessageLoopHistogrammer,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashInProcess,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kProfilingAtStart,
      switches::kProfilingFile,
      switches::kProfilingFlush,
      switches::kRemoteShellPort,
      switches::kSilentDumpOnDCHECK,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kUtilityProcess) {
    if (browser_command_line.HasSwitch(
            switches::kEnableExperimentalExtensionApis)) {
      command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
    }
  } else if (process_type == switches::kPluginProcess) {
    static const char* const kSwitchNames[] = {
  #if defined(OS_CHROMEOS)
      switches::kLoginProfile,
  #endif
      switches::kMemoryProfiling,
      switches::kSilentDumpOnDCHECK,
      switches::kUserDataDir,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kZygoteProcess) {
    static const char* const kSwitchNames[] = {
      switches::kEnableRemoting,
      switches::kUserDataDir,  // Make logs go to the right file.
      // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
      switches::kPpapiFlashInProcess,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  }
}

std::string ChromeContentBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

std::string ChromeContentBrowserClient::GetAcceptLangs(const TabContents* tab) {
  return tab->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

SkBitmap* ChromeContentBrowserClient::GetDefaultFavicon() {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  return rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
}

bool ChromeContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  // FIXME(jochen): get the correct top-level origin.
  ContentSetting setting = io_data->GetHostContentSettingsMap()->
      GetCookieContentSetting(manifest_url, manifest_url, true);
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
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  ContentSetting setting = io_data->GetHostContentSettingsMap()->
      GetCookieContentSetting(url, first_party, false);
  bool allow = setting == CONTENT_SETTING_ALLOW ||
      setting == CONTENT_SETTING_SESSION_ONLY;

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
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  ContentSetting setting = io_data->GetHostContentSettingsMap()->
      GetCookieContentSetting(url, first_party, true);

  if (setting == CONTENT_SETTING_SESSION_ONLY)
    options->set_force_session();

  bool allow = setting == CONTENT_SETTING_ALLOW ||
      setting == CONTENT_SETTING_SESSION_ONLY;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::CookieChanged,
          render_process_id, render_view_id, url, cookie_line, *options,
          !allow));
  return allow;
}

QuotaPermissionContext*
ChromeContentBrowserClient::CreateQuotaPermissionContext() {
  return new ChromeQuotaPermissionContext();
}

void ChromeContentBrowserClient::RevealFolderInOS(const FilePath& path) {
  // On Mac, this call needs to be done on the UI thread.  On other platforms,
  // do it on the FILE thread so we don't slow down UI.
#if defined(OS_MACOSX)
  platform_util::OpenItem(path);
#else
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&platform_util::OpenItem, path));
#endif
}

void ChromeContentBrowserClient::AllowCertificateError(
    SSLCertErrorHandler* handler,
    bool overridable,
    Callback2<SSLCertErrorHandler*, bool>::Type* callback) {
  SSLBlockingPage* blocking_page = new SSLBlockingPage(
      handler, overridable, callback);
  blocking_page->Show();
}

void ChromeContentBrowserClient::ShowClientCertificateRequestDialog(
    int render_process_id,
    int render_view_id,
    SSLClientAuthHandler* handler) {
  TabContents* tab = tab_util::GetTabContentsByID(
      render_process_id, render_view_id);
  if (!tab) {
    NOTREACHED();
    return;
  }

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab);
  wrapper->ssl_helper()->ShowClientCertificateRequestDialog(handler);
}

void ChromeContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
  // The handler will run the UI and delete itself when it's finished.
  new SSLAddCertHandler(request, cert, render_process_id, render_view_id);
}

void ChromeContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  RenderProcessHost* process = rvh->process();
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(process->profile());
  service->RequestPermission(
      source_origin, render_process_id, render_view_id, callback_context,
      tab_util::GetTabContentsByID(render_process_id, render_view_id));
}

WebKit::WebNotificationPresenter::Permission
    ChromeContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_url,
        const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));

  const Extension* extension =
      io_data->GetExtensionInfoMap()->extensions().GetByURL(source_url);
  if (extension &&
      extension->HasAPIPermission(ExtensionAPIPermission::kNotification)) {
    return WebKit::WebNotificationPresenter::PermissionAllowed;
  }

  // Fall back to the regular notification preferences, which works on an
  // origin basis.
  return io_data->GetNotificationService() ?
      io_data->GetNotificationService()->HasPermission(source_url.GetOrigin()) :
      WebKit::WebNotificationPresenter::PermissionNotAllowed;
}

void ChromeContentBrowserClient::ShowDesktopNotification(
    const DesktopNotificationHostMsg_Show_Params& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  RenderProcessHost* process = rvh->process();
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(process->profile());
  service->ShowDesktopNotification(
    params, render_process_id, render_view_id,
    worker ? DesktopNotificationService::WorkerNotification :
        DesktopNotificationService::PageNotification);
}

void ChromeContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  RenderProcessHost* process = rvh->process();
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(process->profile());
  service->CancelDesktopNotification(
      render_process_id, render_view_id, notification_id);
}

bool ChromeContentBrowserClient::CanCreateWindow(
    const GURL& source_url,
    WindowContainerType container_type,
    const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // If the opener is trying to create a background window but doesn't have
  // the appropriate permission, fail the attempt.
  if (container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    ProfileIOData* io_data =
        reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
    const Extension* extension =
        io_data->GetExtensionInfoMap()->extensions().GetByURL(source_url);
    return (extension &&
            extension->HasAPIPermission(ExtensionAPIPermission::kBackground));
  }
  return true;
}

std::string ChromeContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Check if it's an extension-created worker, in which case we want to use
  // the name of the extension.
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  const Extension* extension =
      io_data->GetExtensionInfoMap()->extensions().GetByID(url.host());
  return extension ? extension->name() : std::string();
}

ResourceDispatcherHost*
    ChromeContentBrowserClient::GetResourceDispatcherHost() {
  return g_browser_process->resource_dispatcher_host();
}

ui::Clipboard* ChromeContentBrowserClient::GetClipboard() {
  return g_browser_process->clipboard();
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

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    ChromeContentBrowserClient::GetCryptoPasswordDelegate(
        const GURL& url) {
  return browser::NewCryptoModuleBlockingDialogDelegate(
      browser::kCryptoModulePasswordKeygen, url.host());
}
#endif

}  // namespace chrome
