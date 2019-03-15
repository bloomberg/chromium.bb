// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_browser_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dark_mode_handler.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/extensions_resources.h"
#include "chrome/grit/extensions_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/ui/webui/extensions/chromeos/kiosk_apps_handler.h"
#endif

namespace extensions {

namespace {

constexpr char kInDevModeKey[] = "inDevMode";
constexpr char kShowActivityLogKey[] = "showActivityLog";
constexpr char kLoadTimeClassesKey[] = "loadTimeClasses";

class ExtensionWebUiTimer : public content::WebContentsObserver {
 public:
  explicit ExtensionWebUiTimer(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~ExtensionWebUiTimer() override {}

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsInMainFrame() &&
        !navigation_handle->IsSameDocument()) {
      timer_.reset(new base::ElapsedTimer());
    }
  }

  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host != web_contents()->GetMainFrame() ||
        !timer_) {  // See comment in DocumentOnLoadCompletedInMainFrame()
      return;
    }
    UMA_HISTOGRAM_TIMES("Extensions.WebUi.DocumentLoadedInMainFrameTime.MD",
                        timer_->Elapsed());
  }

  void DocumentOnLoadCompletedInMainFrame() override {
    // TODO(devlin): The usefulness of these metrics remains to be seen.
    if (!timer_) {
      // This object could have been created for a child RenderFrameHost so it
      // would never get a DidStartNavigation with the main frame. However it
      // will receive this current callback.
      return;
    }
    UMA_HISTOGRAM_TIMES("Extensions.WebUi.LoadCompletedInMainFrame.MD",
                        timer_->Elapsed());
    timer_.reset();
  }

  void WebContentsDestroyed() override { delete this; }

 private:
  std::unique_ptr<base::ElapsedTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebUiTimer);
};

std::string GetLoadTimeClasses(bool in_dev_mode) {
  return in_dev_mode ? "in-dev-mode" : std::string();
}

content::WebUIDataSource* CreateMdExtensionsSource(Profile* profile,
                                                   bool in_dev_mode) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIExtensionsHost);
  source->SetJsonPath("strings.js");

  static constexpr LocalizedString kLocalizedStrings[] = {
    // Add common strings.
    {"add", IDS_ADD},
    {"back", IDS_ACCNAME_BACK},
    {"cancel", IDS_CANCEL},
    {"close", IDS_CLOSE},
    {"confirm", IDS_CONFIRM},
    {"controlledSettingPolicy", IDS_CONTROLLED_SETTING_POLICY},
    {"done", IDS_DONE},
    {"learnMore", IDS_LEARN_MORE},
    {"noSearchResults", IDS_SEARCH_NO_RESULTS},
    {"ok", IDS_OK},
    {"save", IDS_SAVE},
    {"searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL},
    {"searchResultsSingular", IDS_SEARCH_RESULTS_SINGULAR},

    // Multi-use strings defined in md_extensions_strings.grdp.
    {"remove", IDS_MD_EXTENSIONS_REMOVE},

    // Add extension-specific strings.
    {"title", IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE},
    {"toolbarTitle", IDS_MD_EXTENSIONS_TOOLBAR_TITLE},
    {"mainMenu", IDS_MD_EXTENSIONS_MENU_BUTTON_LABEL},
    {"search", IDS_MD_EXTENSIONS_SEARCH},
    // TODO(dpapad): Use a single merged string resource for "Clear search".
    {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
    {"sidebarExtensions", IDS_MD_EXTENSIONS_SIDEBAR_EXTENSIONS},
    {"appsTitle", IDS_MD_EXTENSIONS_APPS_TITLE},
    {"noExtensionsOrApps", IDS_MD_EXTENSIONS_NO_INSTALLED_ITEMS},
    {"noDescription", IDS_MD_EXTENSIONS_NO_DESCRIPTION},
    {"viewInStore", IDS_MD_EXTENSIONS_ITEM_CHROME_WEB_STORE},
    {"extensionWebsite", IDS_MD_EXTENSIONS_ITEM_EXTENSION_WEBSITE},
    {"dropToInstall", IDS_EXTENSIONS_INSTALL_DROP_TARGET},
    {"errorsPageHeading", IDS_MD_EXTENSIONS_ERROR_PAGE_HEADING},
    {"clearActivities", IDS_MD_EXTENSIONS_CLEAR_ACTIVITIES},
    {"clearAll", IDS_MD_EXTENSIONS_ERROR_CLEAR_ALL},
    {"clearEntry", IDS_MD_EXTENSIONS_A11Y_CLEAR_ENTRY},
    {"logLevel", IDS_EXTENSIONS_LOG_LEVEL_INFO},
    {"warnLevel", IDS_EXTENSIONS_LOG_LEVEL_WARN},
    {"errorLevel", IDS_EXTENSIONS_LOG_LEVEL_ERROR},
    {"anonymousFunction", IDS_MD_EXTENSIONS_ERROR_ANONYMOUS_FUNCTION},
    {"errorContext", IDS_MD_EXTENSIONS_ERROR_CONTEXT},
    {"errorContextUnknown", IDS_MD_EXTENSIONS_ERROR_CONTEXT_UNKNOWN},
    {"stackTrace", IDS_MD_EXTENSIONS_ERROR_STACK_TRACE},
    // TODO(dpapad): Unify with Settings' IDS_SETTINGS_WEB_STORE.
    {"openChromeWebStore", IDS_MD_EXTENSIONS_SIDEBAR_OPEN_CHROME_WEB_STORE},
    {"keyboardShortcuts", IDS_MD_EXTENSIONS_SIDEBAR_KEYBOARD_SHORTCUTS},
    {"incognitoInfoWarning", IDS_EXTENSIONS_INCOGNITO_WARNING},
    {"hostPermissionsDescription",
     IDS_MD_EXTENSIONS_HOST_PERMISSIONS_DESCRIPTION},
    {"hostPermissionsEdit", IDS_MD_EXTENSIONS_HOST_PERMISSIONS_EDIT},
    {"hostPermissionsHeading", IDS_MD_EXTENSIONS_ITEM_HOST_PERMISSIONS_HEADING},
    {"hostAccessOnClick", IDS_MD_EXTENSIONS_HOST_ACCESS_ON_CLICK},
    {"hostAccessOnSpecificSites",
     IDS_MD_EXTENSIONS_HOST_ACCESS_ON_SPECIFIC_SITES},
    {"hostAccessOnAllSites", IDS_MD_EXTENSIONS_HOST_ACCESS_ON_ALL_SITES},
    {"hostAllowedHosts", IDS_EXTENSIONS_ITEM_ALLOWED_HOSTS},
    {"itemId", IDS_MD_EXTENSIONS_ITEM_ID},
    {"itemInspectViews", IDS_MD_EXTENSIONS_ITEM_INSPECT_VIEWS},
    // NOTE: This text reads "<n> more". It's possible that it should be using
    // a plural string instead. Unfortunately, this is non-trivial since we
    // don't expose that capability to JS yet. Since we don't know it's a
    // problem, use a simple placeholder for now.
    {"itemInspectViewsExtra", IDS_MD_EXTENSIONS_ITEM_INSPECT_VIEWS_EXTRA},
    {"noActiveViews", IDS_MD_EXTENSIONS_ITEM_NO_ACTIVE_VIEWS},
    {"itemAllowIncognito", IDS_MD_EXTENSIONS_ITEM_ALLOW_INCOGNITO},
    {"itemDescriptionLabel", IDS_MD_EXTENSIONS_ITEM_DESCRIPTION},
    {"itemDependencies", IDS_MD_EXTENSIONS_ITEM_DEPENDENCIES},
    {"itemDependentEntry", IDS_MD_EXTENSIONS_DEPENDENT_ENTRY},
    {"itemDetails", IDS_MD_EXTENSIONS_ITEM_DETAILS},
    {"itemErrors", IDS_MD_EXTENSIONS_ITEM_ERRORS},
    {"accessibilityErrorLine", IDS_MD_EXTENSIONS_ACCESSIBILITY_ERROR_LINE},
    {"accessibilityErrorMultiLine",
     IDS_MD_EXTENSIONS_ACCESSIBILITY_ERROR_MULTI_LINE},
    {"activityLogPageHeading", IDS_MD_EXTENSIONS_ACTIVITY_LOG_PAGE_HEADING},
    {"activityLogTypeColumn", IDS_MD_EXTENSIONS_ACTIVITY_LOG_TYPE_COLUMN},
    {"activityLogNameColumn", IDS_MD_EXTENSIONS_ACTIVITY_LOG_NAME_COLUMN},
    {"activityLogCountColumn", IDS_MD_EXTENSIONS_ACTIVITY_LOG_COUNT_COLUMN},
    {"activityLogTimeColumn", IDS_MD_EXTENSIONS_ACTIVITY_LOG_TIME_COLUMN},
    {"activityLogSearchLabel", IDS_MD_EXTENSIONS_ACTIVITY_LOG_SEARCH_LABEL},
    {"activityLogHistoryTabHeading",
     IDS_MD_EXTENSIONS_ACTIVITY_LOG_HISTORY_TAB_HEADING},
    {"activityLogStreamTabHeading",
     IDS_MD_EXTENSIONS_ACTIVITY_LOG_STREAM_TAB_HEADING},
    {"startActivityStream", IDS_MD_EXTENSIONS_START_ACTIVITY_STREAM},
    {"stopActivityStream", IDS_MD_EXTENSIONS_STOP_ACTIVITY_STREAM},
    {"emptyStreamStarted", IDS_MD_EXTENSIONS_EMPTY_STREAM_STARTED},
    {"emptyStreamStopped", IDS_MD_EXTENSIONS_EMPTY_STREAM_STOPPED},
    {"activityArgumentsHeading", IDS_MD_EXTENSIONS_ACTIVITY_ARGUMENTS_HEADING},
    {"webRequestInfoHeading", IDS_MD_EXTENSIONS_WEB_REQUEST_INFO_HEADING},
    {"appIcon", IDS_MD_EXTENSIONS_APP_ICON},
    {"extensionIcon", IDS_MD_EXTENSIONS_EXTENSION_ICON},
    {"extensionA11yAssociation", IDS_MD_EXTENSIONS_EXTENSION_A11Y_ASSOCIATION},
    {"itemIdHeading", IDS_MD_EXTENSIONS_ITEM_ID_HEADING},
    {"extensionEnabled", IDS_MD_EXTENSIONS_EXTENSION_ENABLED},
    {"appEnabled", IDS_MD_EXTENSIONS_APP_ENABLED},
    {"installWarnings", IDS_EXTENSIONS_INSTALL_WARNINGS},
    {"itemExtensionPath", IDS_EXTENSIONS_PATH},
    {"itemOff", IDS_MD_EXTENSIONS_ITEM_OFF},
    {"itemOn", IDS_MD_EXTENSIONS_ITEM_ON},
    {"itemOptions", IDS_MD_EXTENSIONS_ITEM_OPTIONS},
    {"itemPermissions", IDS_MD_EXTENSIONS_ITEM_PERMISSIONS},
    {"itemPermissionsEmpty", IDS_MD_EXTENSIONS_ITEM_PERMISSIONS_EMPTY},
    {"itemRemoveExtension", IDS_MD_EXTENSIONS_ITEM_REMOVE_EXTENSION},
    {"itemSiteAccess", IDS_MD_EXTENSIONS_ITEM_SITE_ACCESS},
    {"itemSiteAccessAddHost", IDS_MD_EXTENSIONS_ITEM_SITE_ACCESS_ADD_HOST},
    {"itemSiteAccessEmpty", IDS_MD_EXTENSIONS_ITEM_SITE_ACCESS_EMPTY},
    {"itemSource", IDS_MD_EXTENSIONS_ITEM_SOURCE},
    {"itemSourcePolicy", IDS_MD_EXTENSIONS_ITEM_SOURCE_POLICY},
    {"itemSourceSideloaded", IDS_MD_EXTENSIONS_ITEM_SOURCE_SIDELOADED},
    {"itemSourceUnpacked", IDS_MD_EXTENSIONS_ITEM_SOURCE_UNPACKED},
    {"itemSourceWebstore", IDS_MD_EXTENSIONS_ITEM_SOURCE_WEBSTORE},
    {"itemVersion", IDS_MD_EXTENSIONS_ITEM_VERSION},
    // TODO(dpapad): Replace this with an Extensions specific string.
    {"itemSize", IDS_DIRECTORY_LISTING_SIZE},
    {"itemAllowOnFileUrls", IDS_EXTENSIONS_ALLOW_FILE_ACCESS},
    {"itemAllowOnAllSites", IDS_EXTENSIONS_ALLOW_ON_ALL_URLS},
    {"itemAllowOnFollowingSites", IDS_EXTENSIONS_ALLOW_ON_FOLLOWING_SITES},
    {"itemCollectErrors", IDS_EXTENSIONS_ENABLE_ERROR_COLLECTION},
    {"itemCorruptInstall", IDS_EXTENSIONS_CORRUPTED_EXTENSION},
    {"itemRepair", IDS_EXTENSIONS_REPAIR_CORRUPTED},
    {"itemReload", IDS_EXTENSIONS_RELOAD_TERMINATED},
    {"loadErrorCouldNotLoadManifest",
     IDS_MD_EXTENSIONS_LOAD_ERROR_COULD_NOT_LOAD_MANIFEST},
    {"loadErrorHeading", IDS_MD_EXTENSIONS_LOAD_ERROR_HEADING},
    {"loadErrorFileLabel", IDS_MD_EXTENSIONS_LOAD_ERROR_FILE_LABEL},
    {"loadErrorErrorLabel", IDS_MD_EXTENSIONS_LOAD_ERROR_ERROR_LABEL},
    {"loadErrorRetry", IDS_MD_EXTENSIONS_LOAD_ERROR_RETRY},
    {"loadingActivities", IDS_MD_EXTENSIONS_LOADING_ACTIVITIES},
    {"noActivities", IDS_MD_EXTENSIONS_NO_ACTIVITIES},
    {"noErrorsToShow", IDS_EXTENSIONS_ERROR_NO_ERRORS_CODE_MESSAGE},
    {"runtimeHostsDialogInputError",
     IDS_MD_EXTENSIONS_RUNTIME_HOSTS_DIALOG_INPUT_ERROR},
    {"runtimeHostsDialogInputLabel",
     IDS_MD_EXTENSIONS_RUNTIME_HOSTS_DIALOG_INPUT_LABEL},
    {"runtimeHostsDialogTitle", IDS_MD_EXTENSIONS_RUNTIME_HOSTS_DIALOG_TITLE},
    {"packDialogTitle", IDS_MD_EXTENSIONS_PACK_DIALOG_TITLE},
    {"packDialogWarningTitle", IDS_MD_EXTENSIONS_PACK_DIALOG_WARNING_TITLE},
    {"packDialogErrorTitle", IDS_MD_EXTENSIONS_PACK_DIALOG_ERROR_TITLE},
    {"packDialogProceedAnyway", IDS_MD_EXTENSIONS_PACK_DIALOG_PROCEED_ANYWAY},
    {"packDialogBrowse", IDS_MD_EXTENSIONS_PACK_DIALOG_BROWSE_BUTTON},
    {"packDialogExtensionRoot",
     IDS_MD_EXTENSIONS_PACK_DIALOG_EXTENSION_ROOT_LABEL},
    {"packDialogKeyFile", IDS_MD_EXTENSIONS_PACK_DIALOG_KEY_FILE_LABEL},
    {"packDialogContent", IDS_EXTENSION_PACK_DIALOG_HEADING},
    {"packDialogConfirm", IDS_MD_EXTENSIONS_PACK_DIALOG_CONFIRM_BUTTON},
    {"shortcutNotSet", IDS_MD_EXTENSIONS_SHORTCUT_NOT_SET},
    {"shortcutScopeGlobal", IDS_MD_EXTENSIONS_SHORTCUT_SCOPE_GLOBAL},
    {"shortcutScopeLabel", IDS_MD_EXTENSIONS_SHORTCUT_SCOPE_LABEL},
    {"shortcutScopeInChrome", IDS_MD_EXTENSIONS_SHORTCUT_SCOPE_IN_CHROME},
    {"shortcutTypeAShortcut", IDS_MD_EXTENSIONS_TYPE_A_SHORTCUT},
    {"shortcutIncludeStartModifier", IDS_MD_EXTENSIONS_INCLUDE_START_MODIFIER},
    {"shortcutTooManyModifiers", IDS_MD_EXTENSIONS_TOO_MANY_MODIFIERS},
    {"shortcutNeedCharacter", IDS_MD_EXTENSIONS_NEED_CHARACTER},
    {"toolbarDevMode", IDS_MD_EXTENSIONS_DEVELOPER_MODE},
    {"toolbarLoadUnpacked", IDS_MD_EXTENSIONS_TOOLBAR_LOAD_UNPACKED},
    {"toolbarPack", IDS_MD_EXTENSIONS_TOOLBAR_PACK},
    {"toolbarUpdateNow", IDS_MD_EXTENSIONS_TOOLBAR_UPDATE_NOW},
    {"toolbarUpdateNowTooltip", IDS_MD_EXTENSIONS_TOOLBAR_UPDATE_NOW_TOOLTIP},
    {"toolbarUpdateDone", IDS_MD_EXTENSIONS_TOOLBAR_UPDATE_DONE},
    {"toolbarUpdatingToast", IDS_MD_EXTENSIONS_TOOLBAR_UPDATING_TOAST},
    {"updateRequiredByPolicy",
     IDS_MD_EXTENSIONS_DISABLED_UPDATE_REQUIRED_BY_POLICY},
    {"viewActivityLog", IDS_EXTENSIONS_VIEW_ACTIVITY_LOG},
    {"viewBackgroundPage", IDS_EXTENSIONS_BACKGROUND_PAGE},
    {"viewIncognito", IDS_EXTENSIONS_VIEW_INCOGNITO},
    {"viewInactive", IDS_EXTENSIONS_VIEW_INACTIVE},
    {"viewIframe", IDS_EXTENSIONS_VIEW_IFRAME},

#if defined(OS_CHROMEOS)
    {"manageKioskApp", IDS_MD_EXTENSIONS_MANAGE_KIOSK_APP},
    {"kioskAddApp", IDS_MD_EXTENSIONS_KIOSK_ADD_APP},
    {"kioskAddAppHint", IDS_MD_EXTENSIONS_KIOSK_ADD_APP_HINT},
    {"kioskEnableAutoLaunch", IDS_MD_EXTENSIONS_KIOSK_ENABLE_AUTO_LAUNCH},
    {"kioskDisableAutoLaunch", IDS_MD_EXTENSIONS_KIOSK_DISABLE_AUTO_LAUNCH},
    {"kioskAutoLaunch", IDS_MD_EXTENSIONS_KIOSK_AUTO_LAUNCH},
    {"kioskInvalidApp", IDS_MD_EXTENSIONS_KIOSK_INVALID_APP},
    {"kioskDisableBailout",
     IDS_MD_EXTENSIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_LABEL},
    {"kioskDisableBailoutWarningTitle",
     IDS_MD_EXTENSIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_TITLE},
#endif
  };
  AddLocalizedStringsBulk(source, kLocalizedStrings,
                          base::size(kLocalizedStrings));

  source->AddString("errorLinesNotShownSingular",
                    l10n_util::GetPluralStringFUTF16(
                        IDS_MD_EXTENSIONS_ERROR_LINES_NOT_SHOWN, 1));
  source->AddString("errorLinesNotShownPlural",
                    l10n_util::GetPluralStringFUTF16(
                        IDS_MD_EXTENSIONS_ERROR_LINES_NOT_SHOWN, 2));
  source->AddString(
      "itemSuspiciousInstall",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
  source->AddString(
      "suspiciousInstallHelpUrl",
      base::ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kRemoveNonCWSExtensionURL),
                             g_browser_process->GetApplicationLocale())
                             .spec()));
#if defined(OS_CHROMEOS)
  source->AddString(
      "kioskDisableBailoutWarningBody",
      l10n_util::GetStringFUTF16(
          IDS_MD_EXTENSIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_BODY,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME)));
#endif
  source->AddString(
      "getMoreExtensionsUrl",
      base::ASCIIToUTF16(
          google_util::AppendGoogleLocaleParam(
              GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
              g_browser_process->GetApplicationLocale())
              .spec()));
  source->AddString("hostPermissionsLearnMoreLink",
                    chrome_extension_constants::kRuntimeHostPermissionsHelpURL);
  source->AddBoolean(kInDevModeKey, in_dev_mode);
  source->AddBoolean(kShowActivityLogKey,
                     base::CommandLine::ForCurrentProcess()->HasSwitch(
                         ::switches::kEnableExtensionActivityLogging));
  source->AddString(kLoadTimeClassesKey, GetLoadTimeClasses(in_dev_mode));

#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("crisper.js", IDR_MD_EXTENSIONS_CRISPER_JS);
  source->SetDefaultResource(
      base::FeatureList::IsEnabled(features::kWebUIPolymer2) ?
          IDR_MD_EXTENSIONS_VULCANIZED_P2_HTML :
          IDR_MD_EXTENSIONS_VULCANIZED_HTML);
  source->UseGzip();
#else
  // Add all MD Extensions resources.
  for (size_t i = 0; i < kExtensionsResourcesSize; ++i) {
    source->AddResourcePath(kExtensionsResources[i].name,
                            kExtensionsResources[i].value);
  }
  source->SetDefaultResource(IDR_MD_EXTENSIONS_EXTENSIONS_HTML);
#endif

  return source;
}

}  // namespace

ExtensionsUI::ExtensionsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = nullptr;

  in_dev_mode_.Init(
      prefs::kExtensionsUIDeveloperMode, profile->GetPrefs(),
      base::Bind(&ExtensionsUI::OnDevModeChanged, base::Unretained(this)));

  source = CreateMdExtensionsSource(profile, *in_dev_mode_);
  DarkModeHandler::Initialize(web_ui, source);
  ManagedUIHandler::Initialize(web_ui, source);

#if defined(OS_CHROMEOS)
  auto kiosk_app_handler = std::make_unique<chromeos::KioskAppsHandler>(
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile));
  web_ui->AddMessageHandler(std::move(kiosk_app_handler));
#endif

  // Need to allow <object> elements so that the <extensionoptions> browser
  // plugin can be loaded within chrome://extensions.
  source->OverrideContentSecurityPolicyObjectSrc("object-src 'self';");

  content::WebUIDataSource::Add(profile, source);

  // Handles its own lifetime.
  new ExtensionWebUiTimer(web_ui->GetWebContents());
}

ExtensionsUI::~ExtensionsUI() {}

// static
base::RefCountedMemory* ExtensionsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.LoadDataResourceBytesForScale(IDR_EXTENSIONS_FAVICON, scale_factor);
}

void ExtensionsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kExtensionsUIDeveloperMode, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// Normally volatile data does not belong in loadTimeData, but in this case
// prevents flickering on a very prominent surface (top of the landing page).
void ExtensionsUI::OnDevModeChanged() {
  auto update = std::make_unique<base::DictionaryValue>();
  update->SetBoolean(kInDevModeKey, *in_dev_mode_);
  update->SetString(kLoadTimeClassesKey, GetLoadTimeClasses(*in_dev_mode_));
  content::WebUIDataSource::Update(Profile::FromWebUI(web_ui()),
                                   chrome::kChromeUIExtensionsHost,
                                   std::move(update));
}

}  // namespace extensions
