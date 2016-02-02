// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"

#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

namespace extensions {

ExtensionSettingsHandler::ExtensionSettingsHandler()
    : extension_service_(nullptr) {
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
  source->AddString("viewIframe",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VIEW_IFRAME));
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
#if defined(ENABLE_SUPERVISED_USERS)
  const SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  source->AddString("extensionSettingsSupervisedUser",
                    supervised_user_service->GetExtensionsLockedMessage());
#endif
  source->AddString("loading",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOADING));
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

  source->AddLocalizedString("extensionLogLevelInfo",
                             IDS_EXTENSIONS_LOG_LEVEL_INFO);
  source->AddLocalizedString("extensionLogLevelWarn",
                             IDS_EXTENSIONS_LOG_LEVEL_WARN);
  source->AddLocalizedString("extensionLogLevelError",
                             IDS_EXTENSIONS_LOG_LEVEL_ERROR);

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
  source->AddString("extensionErrorHeading",
                    l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_HEADING));
  source->AddString("extensionErrorClearAll",
                    l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_CLEAR_ALL));
  source->AddString("extensionErrorNoErrors",
                    l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_NO_ERRORS));
  source->AddString(
      "extensionErrorNoErrorsCodeMessage",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ERROR_NO_ERRORS_CODE_MESSAGE));
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

  // Extension Commands Overlay:
  source->AddString("extensionCommandsOverlay",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_DIALOG_TITLE));
  source->AddString("extensionCommandsEmpty",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_EMPTY));
  source->AddString("extensionCommandsInactive",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_INACTIVE));
  source->AddString("extensionCommandsStartTyping",
      l10n_util::GetStringUTF16(IDS_EXTENSION_TYPE_SHORTCUT));
  source->AddString("extensionCommandsDelete",
      l10n_util::GetStringUTF16(IDS_EXTENSION_DELETE_SHORTCUT));
  source->AddString("extensionCommandsGlobal",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_GLOBAL));
  source->AddString("extensionCommandsRegular",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_NOT_GLOBAL));
  source->AddString("ok", l10n_util::GetStringUTF16(IDS_OK));
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
  // Clear the preference for the ADT Promo before fully removing it.
  // TODO(devlin): Take this out when everyone's been updated.
  Profile::FromWebUI(web_ui())->GetPrefs()->ClearPref(
      prefs::kExtensionsUIDismissedADTPromo);

  content::WebContentsObserver::Observe(web_ui()->GetWebContents());
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

}  // namespace extensions
