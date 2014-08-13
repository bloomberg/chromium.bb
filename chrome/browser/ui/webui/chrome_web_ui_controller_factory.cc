// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/app_launcher_page_ui.h"
#include "chrome/browser/ui/webui/bookmarks_ui.h"
#include "chrome/browser/ui/webui/components_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/webui/domain_reliability_internals_ui.h"
#include "chrome/browser/ui/webui/downloads_ui.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/flash_ui.h"
#include "chrome/browser/ui/webui/gcm_internals_ui.h"
#include "chrome/browser/ui/webui/help/help_ui.h"
#include "chrome/browser/ui/webui/history_ui.h"
#include "chrome/browser/ui/webui/identity_internals_ui.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"
#include "chrome/browser/ui/webui/invalidations_ui.h"
#include "chrome/browser/ui/webui/memory_internals/memory_internals_ui.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui.h"
#include "chrome/browser/ui/webui/plugins_ui.h"
#include "chrome/browser/ui/webui/predictors/predictors_ui.h"
#include "chrome/browser/ui/webui/profiler_ui.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/user_manager_ui.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "chrome/browser/ui/webui/sync_internals_ui.h"
#include "chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"
#include "chrome/browser/ui/webui/version_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/dom_distiller_constants.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/webui/dom_distiller_ui.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "url/gurl.h"

#if !defined(DISABLE_NACL)
#include "chrome/browser/ui/webui/nacl_ui.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/ui/webui/policy_ui.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"
#endif

#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/welcome_ui_android.h"
#else
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_ui.h"
#include "chrome/browser/ui/webui/suggestions_internals/suggestions_internals_ui.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_ui.h"
#include "chrome/browser/ui/webui/system_info_ui.h"
#include "chrome/browser/ui/webui/uber/uber_ui.h"
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "chrome/browser/ui/webui/net_export_ui.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_ui.h"
#include "chrome/browser/ui/webui/chromeos/certificate_manager_dialog_ui.h"
#include "chrome/browser/ui/webui/chromeos/charger_replacement_ui.h"
#include "chrome/browser/ui/webui/chromeos/choose_mobile_network_ui.h"
#include "chrome/browser/ui/webui/chromeos/cryptohome_ui.h"
#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_ui.h"
#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_ui.h"
#include "chrome/browser/ui/webui/chromeos/keyboard_overlay_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_ui.h"
#include "chrome/browser/ui/webui/chromeos/network_ui.h"
#include "chrome/browser/ui/webui/chromeos/nfc_debug_ui.h"
#include "chrome/browser/ui/webui/chromeos/power_ui.h"
#include "chrome/browser/ui/webui/chromeos/provided_file_systems_ui.h"
#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"
#include "chrome/browser/ui/webui/chromeos/salsa_ui.h"
#include "chrome/browser/ui/webui/chromeos/set_time_ui.h"
#include "chrome/browser/ui/webui/chromeos/sim_unlock_ui.h"
#include "chrome/browser/ui/webui/chromeos/slow_trace_ui.h"
#include "chrome/browser/ui/webui/chromeos/slow_ui.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/webui/gesture_config_ui.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/conflicts_ui.h"
#include "chrome/browser/ui/webui/set_as_default_browser_ui.h"
#endif

#if (defined(USE_NSS) || defined(USE_OPENSSL)) && defined(USE_AURA)
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"
#endif

#if defined(ENABLE_APP_LIST)
#include "chrome/browser/ui/webui/app_list/start_page_ui.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/webui/extensions/extension_info_ui.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/browser/ui/webui/voicesearch_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#endif

using content::WebUI;
using content::WebUIController;
using ui::ExternalWebDialogUI;
using ui::WebDialogUI;

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be NULL (for example, if the URL refers to an non-existent extension).
typedef WebUIController* (*WebUIFactoryFunction)(WebUI* web_ui,
                                                 const GURL& url);

// Template for defining WebUIFactoryFunction.
template<class T>
WebUIController* NewWebUI(WebUI* web_ui, const GURL& url) {
  return new T(web_ui);
}

#if defined(ENABLE_EXTENSIONS)
// Special cases for extensions.
template<>
WebUIController* NewWebUI<ExtensionWebUI>(WebUI* web_ui,
                                          const GURL& url) {
  return new ExtensionWebUI(web_ui, url);
}
#endif  // defined(ENABLE_EXTENSIONS)

// Special case for older about: handlers.
template<>
WebUIController* NewWebUI<AboutUI>(WebUI* web_ui, const GURL& url) {
  return new AboutUI(web_ui, url.host());
}

#if defined(OS_CHROMEOS)
template<>
WebUIController* NewWebUI<chromeos::OobeUI>(WebUI* web_ui, const GURL& url) {
  return new chromeos::OobeUI(web_ui, url);
}
#endif

// Special cases for DOM distiller.
template<>
WebUIController* NewWebUI<dom_distiller::DomDistillerUi>(WebUI* web_ui,
                                                         const GURL& url) {
  // The DomDistillerUi can not depend on components/dom_distiller/content,
  // so inject the correct DomDistillerService from chrome/.
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  dom_distiller::DomDistillerService* service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          browser_context);
  return new dom_distiller::DomDistillerUi(
      web_ui, service, dom_distiller::kDomDistillerScheme);
}

#if defined(ENABLE_EXTENSIONS)
// Only create ExtensionWebUI for URLs that are allowed extension bindings,
// hosted by actual tabs.
bool NeedsExtensionWebUI(Profile* profile, const GURL& url) {
  if (!profile)
    return false;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().
          GetExtensionOrAppByURL(url);
  // Allow bindings for all packaged extensions and component hosted apps.
  return extension &&
      (!extension->is_hosted_app() ||
       extension->location() == extensions::Manifest::COMPONENT);
}
#endif

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have WebUI associated
// with it.
WebUIFactoryFunction GetWebUIFactoryFunction(WebUI* web_ui,
                                             Profile* profile,
                                             const GURL& url) {
#if defined(ENABLE_EXTENSIONS)
  if (NeedsExtensionWebUI(profile, url))
    return &NewWebUI<ExtensionWebUI>;
#endif

  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!url.SchemeIs(content::kChromeDevToolsScheme) &&
      !url.SchemeIs(content::kChromeUIScheme)) {
    return NULL;
  }

  /****************************************************************************
   * Please keep this in alphabetical order. If #ifs or special logics are
   * required, add it below in the appropriate section.
   ***************************************************************************/
  // We must compare hosts only since some of the Web UIs append extra stuff
  // after the host name.
  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host() == chrome::kChromeUICloudPrintResourcesHost)
    return &NewWebUI<ExternalWebDialogUI>;
  if (url.host() == chrome::kChromeUICloudPrintSetupHost)
    return &NewWebUI<WebDialogUI>;
  if (url.host() == chrome::kChromeUIComponentsHost)
    return &NewWebUI<ComponentsUI>;
  if (url.spec() == chrome::kChromeUIConstrainedHTMLTestURL)
    return &NewWebUI<ConstrainedWebDialogUI>;
  if (url.host() == chrome::kChromeUICrashesHost)
    return &NewWebUI<CrashesUI>;
#if defined(ENABLE_SERVICE_DISCOVERY)
  if (url.host() == chrome::kChromeUIDevicesHost) {
    return &NewWebUI<LocalDiscoveryUI>;
  }
#endif
  if (url.host() == chrome::kChromeUIDomainReliabilityInternalsHost)
    return &NewWebUI<DomainReliabilityInternalsUI>;
  if (url.host() == chrome::kChromeUIFlagsHost)
    return &NewWebUI<FlagsUI>;
  if (url.host() == chrome::kChromeUIHistoryFrameHost)
    return &NewWebUI<HistoryUI>;
  if (url.host() == chrome::kChromeUIInstantHost)
    return &NewWebUI<InstantUI>;
  if (url.host() == chrome::kChromeUIInterstitialHost)
    return &NewWebUI<InterstitialUI>;
  if (url.host() == chrome::kChromeUIInvalidationsHost)
    return &NewWebUI<InvalidationsUI>;
  if (url.host() == chrome::kChromeUISupervisedUserPassphrasePageHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
  if (url.host() == chrome::kChromeUIMemoryInternalsHost)
    return &NewWebUI<MemoryInternalsUI>;
#if !defined(DISABLE_NACL)
  if (url.host() == chrome::kChromeUINaClHost)
    return &NewWebUI<NaClUI>;
#endif
#if defined(OS_ANDROID) || defined(OS_IOS)
  if (url.host() == chrome::kChromeUINetExportHost)
    return &NewWebUI<NetExportUI>;
#endif
  if (url.host() == chrome::kChromeUINetInternalsHost)
    return &NewWebUI<NetInternalsUI>;
#if !defined(OS_ANDROID)
  if (url.host() == chrome::kChromeUINewTabHost)
    return &NewWebUI<NewTabUI>;
#endif
  if (url.host() == chrome::kChromeUIOmniboxHost)
    return &NewWebUI<OmniboxUI>;
  if (url.host() == chrome::kChromeUIPasswordManagerInternalsHost)
    return &NewWebUI<PasswordManagerInternalsUI>;
  if (url.host() == chrome::kChromeUIPredictorsHost)
    return &NewWebUI<PredictorsUI>;
  if (url.host() == chrome::kChromeUIProfilerHost)
    return &NewWebUI<ProfilerUI>;
  if (url.host() == chrome::kChromeUISignInInternalsHost)
    return &NewWebUI<SignInInternalsUI>;
  if (url.host() == chrome::kChromeUISyncInternalsHost)
    return &NewWebUI<SyncInternalsUI>;
  if (url.host() == chrome::kChromeUISyncResourcesHost)
    return &NewWebUI<WebDialogUI>;
  if (url.host() == chrome::kChromeUITranslateInternalsHost)
    return &NewWebUI<TranslateInternalsUI>;
  if (url.host() == chrome::kChromeUIUserActionsHost)
    return &NewWebUI<UserActionsUI>;
  if (url.host() == chrome::kChromeUIVersionHost)
    return &NewWebUI<VersionUI>;
#if defined(ENABLE_EXTENSIONS)
  if (url.host() == chrome::kChromeUIVoiceSearchHost)
    return &NewWebUI<VoiceSearchUI>;
#endif
#if defined(ENABLE_WEBRTC)
  if (url.host() == chrome::kChromeUIWebRtcLogsHost)
    return &NewWebUI<WebRtcLogsUI>;
#endif
#if defined(ENABLE_APP_LIST)
  if (url.host() == chrome::kChromeUIAppListStartPageHost)
    return &NewWebUI<app_list::StartPageUI>;
#endif

  /****************************************************************************
   * OS Specific #defines
   ***************************************************************************/
#if defined(OS_ANDROID)
  if (url.host() == chrome::kChromeUIWelcomeHost)
    return &NewWebUI<WelcomeUI>;
#else
  // AppLauncherPage is not needed on Android.
  if (url.host() == chrome::kChromeUIAppLauncherPageHost &&
      profile && extensions::ExtensionSystem::Get(profile)->
          extension_service()) {
    return &NewWebUI<AppLauncherPageUI>;
  }
  // Bookmarks are part of NTP on Android.
  if (url.host() == chrome::kChromeUIBookmarksHost)
    return &NewWebUI<BookmarksUI>;
  if (url.SchemeIs(content::kChromeDevToolsScheme))
    return &NewWebUI<DevToolsUI>;
  // Downloads list on Android uses the built-in download manager.
  if (url.host() == chrome::kChromeUIDownloadsHost)
    return &NewWebUI<DownloadsUI>;
  // Flash is not available on android.
  if (url.host() == chrome::kChromeUIFlashHost)
    return &NewWebUI<FlashUI>;
  if (url.host() == chrome::kChromeUIGCMInternalsHost)
    return &NewWebUI<GCMInternalsUI>;
  // Help is implemented with native UI elements on Android.
  if (url.host() == chrome::kChromeUIHelpFrameHost)
    return &NewWebUI<HelpUI>;
  // Identity API is not available on Android.
  if (url.host() == chrome::kChromeUIIdentityInternalsHost)
    return &NewWebUI<IdentityInternalsUI>;
  // chrome://inspect isn't supported on Android. Page debugging is handled by a
  // remote devtools on the host machine, and other elements (Shared Workers,
  // extensions, etc) aren't supported.
  if (url.host() == chrome::kChromeUIInspectHost)
    return &NewWebUI<InspectUI>;
  // Performance monitoring page is not on Android for now.
  if (url.host() == chrome::kChromeUIPerformanceMonitorHost)
    return &NewWebUI<performance_monitor::PerformanceMonitorUI>;
  // Android does not support plugins for now.
  if (url.host() == chrome::kChromeUIPluginsHost)
    return &NewWebUI<PluginsUI>;
  if (url.host() == chrome::kChromeUIQuotaInternalsHost)
    return &NewWebUI<QuotaInternalsUI>;
  // Settings are implemented with native UI elements on Android.
  if (url.host() == chrome::kChromeUISettingsFrameHost)
    return &NewWebUI<options::OptionsUI>;
  if (url.host() == chrome::kChromeUISuggestionsInternalsHost)
    return &NewWebUI<SuggestionsInternalsUI>;
  if (url.host() == chrome::kChromeUISyncFileSystemInternalsHost)
    return &NewWebUI<SyncFileSystemInternalsUI>;
  if (url.host() == chrome::kChromeUISystemInfoHost)
    return &NewWebUI<SystemInfoUI>;
  // Uber frame is not used on Android.
  if (url.host() == chrome::kChromeUIUberFrameHost)
    return &NewWebUI<UberFrameUI>;
  // Uber page is not used on Android.
  if (url.host() == chrome::kChromeUIUberHost)
    return &NewWebUI<UberUI>;
#endif
#if defined(OS_WIN)
  if (url.host() == chrome::kChromeUIConflictsHost)
    return &NewWebUI<ConflictsUI>;
  if (url.host() == chrome::kChromeUIMetroFlowHost)
    return &NewWebUI<SetAsDefaultBrowserUI>;
#endif
#if (defined(USE_NSS) || defined(USE_OPENSSL)) && defined(USE_AURA)
  if (url.host() == chrome::kChromeUICertificateViewerHost)
    return &NewWebUI<CertificateViewerUI>;
#if defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUICertificateViewerDialogHost)
    return &NewWebUI<CertificateViewerModalDialogUI>;
#endif
#endif
#if defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUIBluetoothPairingHost)
    return &NewWebUI<chromeos::BluetoothPairingUI>;
  if (url.host() == chrome::kChromeUICertificateManagerHost)
    return &NewWebUI<chromeos::CertificateManagerDialogUI>;
  if (url.host() == chrome::kChromeUIChargerReplacementHost)
    return &NewWebUI<chromeos::ChargerReplacementUI>;
  if (url.host() == chrome::kChromeUIChooseMobileNetworkHost)
    return &NewWebUI<chromeos::ChooseMobileNetworkUI>;
  if (url.host() == chrome::kChromeUICryptohomeHost)
    return &NewWebUI<chromeos::CryptohomeUI>;
  if (url.host() == chrome::kChromeUIDriveInternalsHost)
    return &NewWebUI<chromeos::DriveInternalsUI>;
  if (url.host() == chrome::kChromeUIFirstRunHost)
    return &NewWebUI<chromeos::FirstRunUI>;
  if (url.host() == chrome::kChromeUIImageBurnerHost)
    return &NewWebUI<ImageBurnUI>;
  if (url.host() == chrome::kChromeUIKeyboardOverlayHost)
    return &NewWebUI<KeyboardOverlayUI>;
  if (url.host() == chrome::kChromeUIMobileSetupHost)
    return &NewWebUI<MobileSetupUI>;
  if (url.host() == chrome::kChromeUINfcDebugHost)
    return &NewWebUI<chromeos::NfcDebugUI>;
  if (url.host() == chrome::kChromeUIOobeHost)
    return &NewWebUI<chromeos::OobeUI>;
  if (url.host() == chrome::kChromeUIProvidedFileSystemsHost)
    return &NewWebUI<chromeos::ProvidedFileSystemsUI>;
  if (url.host() == chrome::kChromeUIProxySettingsHost)
    return &NewWebUI<chromeos::ProxySettingsUI>;
  if (url.host() == chrome::kChromeUISalsaHost)
    return &NewWebUI<SalsaUI>;
  if (url.host() == chrome::kChromeUISetTimeHost)
    return &NewWebUI<chromeos::SetTimeUI>;
  if (url.host() == chrome::kChromeUISimUnlockHost)
    return &NewWebUI<chromeos::SimUnlockUI>;
  if (url.host() == chrome::kChromeUISlowHost)
    return &NewWebUI<chromeos::SlowUI>;
  if (url.host() == chrome::kChromeUISlowTraceHost)
    return &NewWebUI<chromeos::SlowTraceController>;
  if (url.host() == chrome::kChromeUINetworkHost)
    return &NewWebUI<chromeos::NetworkUI>;
  if (url.host() == chrome::kChromeUIPowerHost)
    return &NewWebUI<chromeos::PowerUI>;
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (url.host() == chrome::kChromeUIChromeSigninHost)
    return &NewWebUI<InlineLoginUI>;
#endif

  /****************************************************************************
   * Other #defines and special logics.
   ***************************************************************************/
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (url.host() == chrome::kChromeUIPolicyHost)
    return &NewWebUI<PolicyUI>;

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  if (url.host() == chrome::kChromeUIProfileSigninConfirmationHost)
    return &NewWebUI<ProfileSigninConfirmationUI>;
#endif

#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
  if (url.host() == chrome::kChromeUITabModalConfirmDialogHost) {
    return &NewWebUI<ConstrainedWebDialogUI>;
  }
#endif

#if defined(USE_AURA)
  if (url.host() == chrome::kChromeUIGestureConfigHost)
    return &NewWebUI<GestureConfigUI>;
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  if (url.host() == chrome::kChromeUIUserManagerHost &&
      switches::IsNewAvatarMenu()) {
    return &NewWebUI<UserManagerUI>;
  }
#endif

  if (url.host() == chrome::kChromeUIChromeURLsHost ||
      url.host() == chrome::kChromeUICreditsHost ||
      url.host() == chrome::kChromeUIDNSHost ||
      url.host() == chrome::kChromeUIMemoryHost ||
      url.host() == chrome::kChromeUIMemoryRedirectHost ||
      url.host() == chrome::kChromeUIStatsHost ||
      url.host() == chrome::kChromeUITermsHost
#if defined(OS_LINUX) || defined(OS_OPENBSD)
      || url.host() == chrome::kChromeUILinuxProxyConfigHost
      || url.host() == chrome::kChromeUISandboxHost
#endif
#if defined(OS_CHROMEOS)
      || url.host() == chrome::kChromeUIDiscardsHost
      || url.host() == chrome::kChromeUIOSCreditsHost
#endif
#if defined(WEBUI_TASK_MANAGER)
      || url.host() == chrome::kChromeUITaskManagerHost
#endif
      ) {
    return &NewWebUI<AboutUI>;
  }

#if defined(ENABLE_EXTENSIONS)
  if (url.host() == chrome::kChromeUIExtensionsFrameHost)
    return &NewWebUI<extensions::ExtensionsUI>;
#endif
#if defined(ENABLE_FULL_PRINTING)
  if (url.host() == chrome::kChromeUIPrintHost &&
      !profile->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled))
    return &NewWebUI<PrintPreviewUI>;
#endif

  if (IsEnableDomDistillerSet() &&
      url.host() == dom_distiller::kChromeUIDomDistillerHost) {
    return &NewWebUI<dom_distiller::DomDistillerUi>;
  }

  return NULL;
}

void RunFaviconCallbackAsync(
    const favicon_base::FaviconResultsCallback& callback,
    const std::vector<favicon_base::FaviconRawBitmapResult>* results) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&FaviconService::FaviconResultsCallbackRunner,
                 callback, base::Owned(results)));
}

}  // namespace

WebUI::TypeID ChromeWebUIControllerFactory::GetWebUIType(
      content::BrowserContext* browser_context, const GURL& url) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(NULL, profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  bool needs_extensions_web_ui = false;
#if defined(ENABLE_EXTENSIONS)
  // Extensions are rendered via WebUI in tabs, but don't actually need WebUI
  // bindings (see the ExtensionWebUI constructor).
  needs_extensions_web_ui =
      NeedsExtensionWebUI(Profile::FromBrowserContext(browser_context), url);
#endif
  return !needs_extensions_web_ui && UseWebUIForURL(browser_context, url);
}

WebUIController* ChromeWebUIControllerFactory::CreateWebUIControllerForURL(
    WebUI* web_ui,
    const GURL& url) const {
  Profile* profile = Profile::FromWebUI(web_ui);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(web_ui, profile, url);
  if (!function)
    return NULL;

  return (*function)(web_ui, url);
}

void ChromeWebUIControllerFactory::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    const std::vector<int>& desired_sizes_in_pixel,
    const favicon_base::FaviconResultsCallback& callback) const {
  // Before determining whether page_url is an extension url, we must handle
  // overrides. This changes urls in |kChromeUIScheme| to extension urls, and
  // allows to use ExtensionWebUI::GetFaviconForURL.
  GURL url(page_url);
#if defined(ENABLE_EXTENSIONS)
  ExtensionWebUI::HandleChromeURLOverride(&url, profile);

  // All extensions but the bookmark manager get their favicon from the icons
  // part of the manifest.
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      url.host() != extension_misc::kBookmarkManagerId) {
    ExtensionWebUI::GetFaviconForURL(profile, url, callback);
    return;
  }
#endif

  std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();

  // Use ui::GetSupportedScaleFactors instead of
  // favicon_base::GetFaviconScales() because chrome favicons comes from
  // resources.
  std::vector<ui::ScaleFactor> resource_scale_factors =
      ui::GetSupportedScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (size_t i = 0; i < resource_scale_factors.size(); ++i) {
    float scale = ui::GetScaleForScaleFactor(resource_scale_factors[i]);
    // Assume that GetFaviconResourceBytes() returns favicons which are
    // |gfx::kFaviconSize| x |gfx::kFaviconSize| DIP.
    int candidate_edge_size =
        static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
    candidate_sizes.push_back(
        gfx::Size(candidate_edge_size, candidate_edge_size));
  }
  std::vector<size_t> selected_indices;
  SelectFaviconFrameIndices(
      candidate_sizes, desired_sizes_in_pixel, &selected_indices, NULL);
  for (size_t i = 0; i < selected_indices.size(); ++i) {
    size_t selected_index = selected_indices[i];
    ui::ScaleFactor selected_resource_scale =
        resource_scale_factors[selected_index];

    scoped_refptr<base::RefCountedMemory> bitmap(
        GetFaviconResourceBytes(url, selected_resource_scale));
    if (bitmap.get() && bitmap->size()) {
      favicon_base::FaviconRawBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap;
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = favicon_base::FAVICON;
      favicon_bitmap_results->push_back(bitmap_result);

      bitmap_result.pixel_size = candidate_sizes[selected_index];
    }
  }

  RunFaviconCallbackAsync(callback, favicon_bitmap_results);
}

// static
ChromeWebUIControllerFactory* ChromeWebUIControllerFactory::GetInstance() {
  return Singleton<ChromeWebUIControllerFactory>::get();
}

ChromeWebUIControllerFactory::ChromeWebUIControllerFactory() {
}

ChromeWebUIControllerFactory::~ChromeWebUIControllerFactory() {
}

base::RefCountedMemory* ChromeWebUIControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url, ui::ScaleFactor scale_factor) const {
#if !defined(OS_ANDROID)  // Bookmarks are part of NTP on Android.
  // The bookmark manager is a chrome extension, so we have to check for it
  // before we check for extension scheme.
  if (page_url.host() == extension_misc::kBookmarkManagerId)
    return BookmarksUI::GetFaviconResourceBytes(scale_factor);

  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(extensions::kExtensionScheme)) {
    NOTREACHED();
    return NULL;
  }
#endif

  if (!content::HasWebUIScheme(page_url))
    return NULL;

  if (page_url.host() == chrome::kChromeUIComponentsHost)
    return ComponentsUI::GetFaviconResourceBytes(scale_factor);

#if defined(OS_WIN)
  if (page_url.host() == chrome::kChromeUIConflictsHost)
    return ConflictsUI::GetFaviconResourceBytes(scale_factor);
#endif

  if (page_url.host() == chrome::kChromeUICrashesHost)
    return CrashesUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host() == chrome::kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes(scale_factor);

#if !defined(OS_ANDROID)
  // The Apps launcher page is not available on android.
  if (page_url.host() == chrome::kChromeUIAppLauncherPageHost)
    return AppLauncherPageUI::GetFaviconResourceBytes(scale_factor);

  // Flash is not available on android.
  if (page_url.host() == chrome::kChromeUIFlashHost)
    return FlashUI::GetFaviconResourceBytes(scale_factor);

  // Android uses the native download manager.
  if (page_url.host() == chrome::kChromeUIDownloadsHost)
    return DownloadsUI::GetFaviconResourceBytes(scale_factor);

  // Android doesn't use the Options pages.
  if (page_url.host() == chrome::kChromeUISettingsHost ||
      page_url.host() == chrome::kChromeUISettingsFrameHost)
    return options::OptionsUI::GetFaviconResourceBytes(scale_factor);

#if defined(ENABLE_EXTENSIONS)
  if (page_url.host() == chrome::kChromeUIExtensionsHost ||
      page_url.host() == chrome::kChromeUIExtensionsFrameHost)
    return extensions::ExtensionsUI::GetFaviconResourceBytes(scale_factor);
#endif

  // Android doesn't use the plugins pages.
  if (page_url.host() == chrome::kChromeUIPluginsHost)
    return PluginsUI::GetFaviconResourceBytes(scale_factor);

#endif

  return NULL;
}
