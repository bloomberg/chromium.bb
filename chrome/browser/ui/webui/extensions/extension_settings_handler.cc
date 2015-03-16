// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/webstore_reinstaller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/blacklist_state.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const char kAppsDeveloperToolsExtensionId[] =
    "ohmmkhmmmpcnpikjeljgnaoabkaalbgc";

}  // namespace

namespace extensions {

///////////////////////////////////////////////////////////////////////////////
//
// ExtensionSettingsHandler
//
///////////////////////////////////////////////////////////////////////////////

ExtensionSettingsHandler::ExtensionSettingsHandler()
    : extension_service_(NULL),
      ignore_notifications_(false),
      deleting_rvh_(NULL),
      deleting_rwh_id_(-1),
      deleting_rph_id_(-1),
      registered_for_notifications_(false),
      warning_service_observer_(this),
      error_console_observer_(this),
      extension_prefs_observer_(this),
      extension_registry_observer_(this),
      extension_management_observer_(this),
      should_do_verification_check_(false) {
}

ExtensionSettingsHandler::~ExtensionSettingsHandler() {
}

// static
void ExtensionSettingsHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kExtensionsUIDeveloperMode,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kExtensionsUIDismissedADTPromo,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void ExtensionSettingsHandler::GetLocalizedValues(
    content::WebUIDataSource* source) {
  source->AddString("extensionSettings",
      l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE));

  source->AddString("extensionSettingsDeveloperMode",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DEVELOPER_MODE_LINK));
  source->AddString("extensionSettingsNoExtensions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_NONE_INSTALLED));
  source->AddString(
      "extensionSettingsSuggestGallery",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_NONE_INSTALLED_SUGGEST_GALLERY,
          base::ASCIIToUTF16(
              google_util::AppendGoogleLocaleParam(
                  GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
                  g_browser_process->GetApplicationLocale()).spec())));
  source->AddString("extensionSettingsGetMoreExtensions",
      l10n_util::GetStringUTF16(IDS_GET_MORE_EXTENSIONS));
  source->AddString(
      "extensionSettingsGetMoreExtensionsUrl",
      base::ASCIIToUTF16(
          google_util::AppendGoogleLocaleParam(
              GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
              g_browser_process->GetApplicationLocale()).spec()));
  source->AddString("extensionSettingsExtensionId",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ID));
  source->AddString("extensionSettingsExtensionPath",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PATH));
  source->AddString("extensionSettingsInspectViews",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSPECT_VIEWS));
  source->AddString("extensionSettingsInstallWarnings",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALL_WARNINGS));
  source->AddString("viewIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VIEW_INCOGNITO));
  source->AddString("viewInactive",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VIEW_INACTIVE));
  source->AddString("backgroundPage",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_BACKGROUND_PAGE));
  source->AddString("extensionSettingsEnable",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE));
  source->AddString("extensionSettingsEnabled",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLED));
  source->AddString("extensionSettingsRemove",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_REMOVE));
  source->AddString("extensionSettingsEnableIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE_INCOGNITO));
  source->AddString("extensionSettingsEnableErrorCollection",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE_ERROR_COLLECTION));
  source->AddString("extensionSettingsAllowFileAccess",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ALLOW_FILE_ACCESS));
  source->AddString("extensionSettingsAllowOnAllUrls",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ALLOW_ON_ALL_URLS));
  source->AddString("extensionSettingsIncognitoWarning",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INCOGNITO_WARNING));
  source->AddString("extensionSettingsReloadTerminated",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_RELOAD_TERMINATED));
  source->AddString("extensionSettingsRepairCorrupted",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_REPAIR_CORRUPTED));
  source->AddString("extensionSettingsLaunch",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LAUNCH));
  source->AddString("extensionSettingsReloadUnpacked",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_RELOAD_UNPACKED));
  source->AddString("extensionSettingsOptions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_OPTIONS_LINK));
  if (CanShowAppInfoDialog()) {
    source->AddString("extensionSettingsPermissions",
                      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INFO_LINK));
  } else {
    source->AddString(
        "extensionSettingsPermissions",
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_PERMISSIONS_LINK));
  }
  source->AddString("extensionSettingsVisitWebsite",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VISIT_WEBSITE));
  source->AddString("extensionSettingsVisitWebStore",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VISIT_WEBSTORE));
  source->AddString("extensionSettingsPolicyControlled",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_POLICY_CONTROLLED));
  source->AddString("extensionSettingsPolicyRecommeneded",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_POLICY_RECOMMENDED));
  source->AddString("extensionSettingsDependentExtensions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DEPENDENT_EXTENSIONS));
  source->AddString("extensionSettingsSupervisedUser",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER));
  source->AddString("extensionSettingsCorruptInstall",
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_CORRUPTED_EXTENSION));
  source->AddString("extensionSettingsSuspiciousInstall",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
  source->AddString("extensionSettingsLearnMore",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  source->AddString("extensionSettingsSuspiciousInstallHelpUrl",
                    base::ASCIIToUTF16(
                        google_util::AppendGoogleLocaleParam(
                            GURL(chrome::kRemoveNonCWSExtensionURL),
                            g_browser_process->GetApplicationLocale()).spec()));
  source->AddString("extensionSettingsShowButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON));
  source->AddString("extensionSettingsLoadUnpackedButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_UNPACKED_BUTTON));
  source->AddString("extensionSettingsPackButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PACK_BUTTON));
  source->AddString("extensionSettingsCommandsLink",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_COMMANDS_CONFIGURE));
  source->AddString("extensionSettingsUpdateButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UPDATE_BUTTON));
  source->AddString(
      "extensionSettingsAppsDevToolsPromoHTML",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_APPS_DEV_TOOLS_PROMO_HTML,
          base::ASCIIToUTF16(
              google_util::AppendGoogleLocaleParam(
                  GURL(extension_urls::GetWebstoreItemDetailURLPrefix() +
                       kAppsDeveloperToolsExtensionId),
                  g_browser_process->GetApplicationLocale()).spec())));
  source->AddString(
      "extensionSettingsAppDevToolsPromoClose",
      l10n_util::GetStringUTF16(IDS_CLOSE));
  source->AddString("extensionSettingsCrashMessage",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_CRASHED_EXTENSION));
  source->AddString("extensionSettingsInDevelopment",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_IN_DEVELOPMENT));
  source->AddString("extensionSettingsWarningsTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WARNINGS_TITLE));
  source->AddString("extensionSettingsShowDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  source->AddString("extensionSettingsHideDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS));
  source->AddString("extensionSettingsUpdateRequiredBePolicy",
                    l10n_util::GetStringUTF16(
                        IDS_EXTENSIONS_DISABLED_UPDATE_REQUIRED_BY_POLICY));

  // TODO(estade): comb through the above strings to find ones no longer used in
  // uber extensions.
  source->AddString("extensionUninstall",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));

  // Pack Extension Overlay:
  source->AddString("packExtensionOverlay",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_TITLE));
  source->AddString("packExtensionHeading",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_HEADING));
  source->AddString("packExtensionCommit",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_BUTTON));
  source->AddString("ok", l10n_util::GetStringUTF16(IDS_OK));
  source->AddString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));
  source->AddString("packExtensionRootDir",
      l10n_util::GetStringUTF16(
          IDS_EXTENSION_PACK_DIALOG_ROOT_DIRECTORY_LABEL));
  source->AddString("packExtensionPrivateKey",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_PRIVATE_KEY_LABEL));
  source->AddString("packExtensionBrowseButton",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_BROWSE));
  source->AddString("packExtensionProceedAnyway",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROCEED_ANYWAY));
  source->AddString("packExtensionWarningTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_WARNING_TITLE));
  source->AddString("packExtensionErrorTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_ERROR_TITLE));

  // Extension Error and Extension Error Overlay:
  source->AddString(
      "extensionErrorsShowMore",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERRORS_SHOW_MORE));
  source->AddString(
      "extensionErrorsShowFewer",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERRORS_SHOW_FEWER));
  source->AddString(
      "extensionErrorViewDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_VIEW_DETAILS));
  source->AddString(
      "extensionErrorViewManifest",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_VIEW_MANIFEST));
  source->AddString("extensionErrorOverlayDone",
                    l10n_util::GetStringUTF16(IDS_DONE));
  source->AddString(
      "extensionErrorOverlayContextUrl",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_CONTEXT));
  source->AddString(
      "extensionErrorOverlayStackTrace",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_STACK_TRACE));
  source->AddString(
      "extensionErrorOverlayAnonymousFunction",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_ANONYMOUS_FUNCTION));
  source->AddString(
      "extensionErrorOverlayLaunchDevtools",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_LAUNCH_DEVTOOLS));
  source->AddString(
      "extensionErrorOverlayContextUnknown",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_CONTEXT_UNKNOWN));
  source->AddString(
      "extensionErrorOverlayNoCodeToDisplay",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_NO_CODE_TO_DISPLAY));
}

void ExtensionSettingsHandler::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  deleting_rvh_ = render_view_host;
  Profile* source_profile = Profile::FromBrowserContext(
      render_view_host->GetSiteInstance()->GetBrowserContext());
  if (!Profile::FromWebUI(web_ui())->IsSameProfile(source_profile))
    return;
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  if (reload_type != content::NavigationController::NO_RELOAD)
    ReloadUnpackedExtensions();
}

void ExtensionSettingsHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui())->GetOriginalProfile();
  extension_service_ =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  web_ui()->RegisterMessageCallback("extensionSettingsRequestExtensionsData",
      base::Bind(&ExtensionSettingsHandler::HandleRequestExtensionsData,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsToggleDeveloperMode",
      base::Bind(&ExtensionSettingsHandler::HandleToggleDeveloperMode,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsLaunch",
      base::Bind(&ExtensionSettingsHandler::HandleLaunchMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsRepair",
      base::Bind(&ExtensionSettingsHandler::HandleRepairMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsEnableErrorCollection",
      base::Bind(&ExtensionSettingsHandler::HandleEnableErrorCollectionMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsAllowOnAllUrls",
      base::Bind(&ExtensionSettingsHandler::HandleAllowOnAllUrlsMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsOptions",
      base::Bind(&ExtensionSettingsHandler::HandleOptionsMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsShowButton",
      base::Bind(&ExtensionSettingsHandler::HandleShowButtonMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsAutoupdate",
      base::Bind(&ExtensionSettingsHandler::HandleAutoUpdateMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsDismissADTPromo",
      base::Bind(&ExtensionSettingsHandler::HandleDismissADTPromoMessage,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback("extensionSettingsShowPath",
      base::Bind(&ExtensionSettingsHandler::HandleShowPath,
                 AsWeakPtr()));
}

void ExtensionSettingsHandler::OnErrorAdded(const ExtensionError* error) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Profile* profile = Profile::FromWebUI(web_ui());
  Profile* source_profile = NULL;
  switch (type) {
    // We listen for notifications that will result in the page being
    // repopulated with data twice for the same event in certain cases.
    // For instance, EXTENSION_LOADED & EXTENSION_HOST_CREATED because
    // we don't know about the views for an extension at EXTENSION_LOADED, but
    // if we only listen to EXTENSION_HOST_CREATED, we'll miss extensions
    // that don't have a process at startup.
    //
    // Doing it this way gets everything but causes the page to be rendered
    // more than we need. It doesn't seem to result in any noticeable flicker.
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      deleting_rvh_ = content::Details<BackgroundContents>(details)->
          web_contents()->GetRenderViewHost();
      // Fall through.
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED:
    case extensions::NOTIFICATION_EXTENSION_HOST_CREATED:
      source_profile = content::Source<Profile>(source).ptr();
      if (!profile->IsSameProfile(source_profile))
        return;
      MaybeUpdateAfterNotification();
      break;
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED: {
      content::RenderWidgetHost* rwh =
          content::Source<content::RenderWidgetHost>(source).ptr();
      deleting_rwh_id_ = rwh->GetRoutingID();
      deleting_rph_id_ = rwh->GetProcess()->GetID();
      MaybeUpdateAfterNotification();
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED:
    case extensions::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED:
      MaybeUpdateAfterNotification();
      break;
    case extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED:
       // This notification is sent when the extension host destruction begins,
       // not when it finishes. We use PostTask to delay the update until after
       // the destruction finishes.
       base::MessageLoop::current()->PostTask(
           FROM_HERE,
           base::Bind(&ExtensionSettingsHandler::MaybeUpdateAfterNotification,
                      AsWeakPtr()));
       break;
    default:
      NOTREACHED();
  }
}

void ExtensionSettingsHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionDisableReasonsChanged(
    const std::string& extension_id, int disable_reasons) {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::OnExtensionManagementSettingsChanged() {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::ExtensionWarningsChanged() {
  MaybeUpdateAfterNotification();
}

void ExtensionSettingsHandler::ReloadUnpackedExtensions() {
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(extension_service_->profile());
  std::vector<const Extension*> unpacked_extensions;
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    if (Manifest::IsUnpackedLocation(extension->location()))
      unpacked_extensions.push_back(extension.get());
  }

  for (std::vector<const Extension*>::iterator iter =
       unpacked_extensions.begin(); iter != unpacked_extensions.end(); ++iter) {
    extension_service_->ReloadExtensionWithQuietFailure((*iter)->id());
  }
}

void ExtensionSettingsHandler::HandleRequestExtensionsData(
    const base::ListValue* args) {
  // The items which are to be written into results are also described in
  // chrome/browser/resources/extensions/extensions.js in @typedef for
  // ExtensionDataResponse. Please update it whenever you add or remove any keys
  // here.
  base::DictionaryValue results;

  Profile* profile = Profile::FromWebUI(web_ui());

  bool is_supervised = profile->IsSupervised();
  bool incognito_available =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
          IncognitoModePrefs::DISABLED;
  bool developer_mode =
      !is_supervised &&
      profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  results.SetBoolean("profileIsSupervised", is_supervised);
  results.SetBoolean("incognitoAvailable", incognito_available);
  results.SetBoolean("developerMode", developer_mode);
  results.SetBoolean("enableAppInfoDialog", CanShowAppInfoDialog());

  // Promote the Chrome Apps & Extensions Developer Tools if they are not
  // installed and the user has not previously dismissed the warning.
  bool promote_apps_dev_tools = false;
  if (!ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))->
          GetExtensionById(kAppsDeveloperToolsExtensionId,
                           ExtensionRegistry::EVERYTHING) &&
      !profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDismissedADTPromo)) {
    promote_apps_dev_tools = true;
  }
  results.SetBoolean("promoteAppsDevTools", promote_apps_dev_tools);

  const bool load_unpacked_disabled =
      ExtensionManagementFactory::GetForBrowserContext(profile)
          ->BlacklistedByDefault();
  results.SetBoolean("loadUnpackedDisabled", load_unpacked_disabled);

  web_ui()->CallJavascriptFunction(
      "extensions.ExtensionSettings.returnExtensionsData", results);

  MaybeRegisterForNotifications();
  UMA_HISTOGRAM_BOOLEAN("ExtensionSettings.ShouldDoVerificationCheck",
                        should_do_verification_check_);
  if (should_do_verification_check_) {
    should_do_verification_check_ = false;
    ExtensionSystem::Get(Profile::FromWebUI(web_ui()))
        ->install_verifier()
        ->VerifyAllExtensions();
  }
}

void ExtensionSettingsHandler::HandleToggleDeveloperMode(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->IsSupervised())
    return;

  bool developer_mode_on;
  CHECK(args->GetBoolean(0, &developer_mode_on));
  profile->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode,
                                  developer_mode_on);
}

void ExtensionSettingsHandler::HandleLaunchMessage(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, false);
  OpenApplication(AppLaunchParams(extension_service_->profile(), extension,
                                  extensions::LAUNCH_CONTAINER_WINDOW,
                                  NEW_WINDOW,
                                  extensions::SOURCE_EXTENSIONS_PAGE));
}

void ExtensionSettingsHandler::HandleRepairMessage(
    const base::ListValue* args) {
  std::string extension_id = base::UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  scoped_refptr<WebstoreReinstaller> reinstaller(new WebstoreReinstaller(
      web_contents(),
      extension_id,
      base::Bind(&ExtensionSettingsHandler::OnReinstallComplete,
                 AsWeakPtr())));
  reinstaller->BeginReinstall();
}

void ExtensionSettingsHandler::HandleEnableErrorCollectionMessage(
    const base::ListValue* args) {
  CHECK_EQ(2u, args->GetSize());
  std::string extension_id;
  std::string enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));
  bool enabled = enable_str == "true";
  ErrorConsole::Get(Profile::FromWebUI(web_ui()))
      ->SetReportingAllForExtension(extension_id, enabled);
}

void ExtensionSettingsHandler::HandleAllowOnAllUrlsMessage(
    const base::ListValue* args) {
  DCHECK(FeatureSwitch::scripts_require_action()->IsEnabled());
  CHECK_EQ(2u, args->GetSize());
  std::string extension_id;
  std::string allow_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &allow_str));
  util::SetAllowedScriptingOnAllUrls(extension_id,
                                     extension_service_->GetBrowserContext(),
                                     allow_str == "true");
}

void ExtensionSettingsHandler::HandleOptionsMessage(
    const base::ListValue* args) {
  const Extension* extension = GetActiveExtension(args);
  if (!extension || OptionsPageInfo::GetOptionsPage(extension).is_empty())
    return;
  ExtensionTabUtil::OpenOptionsPage(extension,
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()));
}

void ExtensionSettingsHandler::HandleShowButtonMessage(
    const base::ListValue* args) {
  const Extension* extension = GetActiveExtension(args);
  if (!extension)
    return;
  ExtensionActionAPI::SetBrowserActionVisibility(
      ExtensionPrefs::Get(extension_service_->profile()),
      extension->id(),
      true);
}

void ExtensionSettingsHandler::HandleAutoUpdateMessage(
    const base::ListValue* args) {
  ExtensionUpdater* updater = extension_service_->updater();
  if (updater) {
    ExtensionUpdater::CheckParams params;
    params.install_immediately = true;
    updater->CheckNow(params);
  }
}

void ExtensionSettingsHandler::HandleDismissADTPromoMessage(
    const base::ListValue* args) {
  DCHECK(args->empty());
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDismissedADTPromo, true);
}

void ExtensionSettingsHandler::HandleShowPath(const base::ListValue* args) {
  DCHECK(!args->empty());
  std::string extension_id = base::UTF16ToUTF8(ExtractStringValue(args));

  Profile* profile = Profile::FromWebUI(web_ui());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  const Extension* extension = registry->GetExtensionById(
      extension_id,
      ExtensionRegistry::EVERYTHING);
  CHECK(extension);
  // We explicitly show manifest.json in order to work around an issue in OSX
  // where opening the directory doesn't focus the Finder.
  platform_util::ShowItemInFolder(profile,
                                  extension->path().Append(kManifestFilename));
}

const Extension* ExtensionSettingsHandler::GetActiveExtension(
    const base::ListValue* args) {
  std::string extension_id = base::UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  return extension_service_->GetExtensionById(extension_id, false);
}

void ExtensionSettingsHandler::MaybeUpdateAfterNotification() {
  content::WebContents* contents = web_ui()->GetWebContents();
  if (!ignore_notifications_ && contents && contents->GetRenderViewHost())
    HandleRequestExtensionsData(NULL);
  deleting_rvh_ = NULL;
}

void ExtensionSettingsHandler::MaybeRegisterForNotifications() {
  if (registered_for_notifications_)
    return;

  registered_for_notifications_  = true;
  Profile* profile = Profile::FromWebUI(web_ui());

  // Register for notifications that we need to reload the page.
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<ExtensionPrefs>(ExtensionPrefs::Get(profile)));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());

  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));

  content::WebContentsObserver::Observe(web_ui()->GetWebContents());

  warning_service_observer_.Add(WarningService::Get(profile));

  error_console_observer_.Add(ErrorConsole::Get(profile));

  extension_management_observer_.Add(
      ExtensionManagementFactory::GetForBrowserContext(profile));
}

void ExtensionSettingsHandler::OnReinstallComplete(
    bool success,
    const std::string& error,
    webstore_install::Result result) {
  MaybeUpdateAfterNotification();
}

}  // namespace extensions
