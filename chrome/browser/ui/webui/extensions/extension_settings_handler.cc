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
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser.h"
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
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

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
      warning_service_observer_(this),
      extension_prefs_observer_(this),
      extension_management_observer_(this) {
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

  web_ui()->RegisterMessageCallback("extensionSettingsRegister",
      base::Bind(&ExtensionSettingsHandler::HandleRegisterMessage,
                 AsWeakPtr()));
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

void ExtensionSettingsHandler::HandleRegisterMessage(
    const base::ListValue* args) {
  if (!registrar_.IsEmpty())
    return;  // Only register once.

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

  content::WebContentsObserver::Observe(web_ui()->GetWebContents());

  warning_service_observer_.Add(WarningService::Get(profile));

  extension_management_observer_.Add(
      ExtensionManagementFactory::GetForBrowserContext(profile));

  // Clear the preference for the ADT Promo before fully removing it.
  // TODO(devlin): Take this out when everyone's been updated.
  profile->GetPrefs()->ClearPref(prefs::kExtensionsUIDismissedADTPromo);
}

void ExtensionSettingsHandler::MaybeUpdateAfterNotification() {
  content::WebContents* contents = web_ui()->GetWebContents();
  if (!ignore_notifications_ && contents && contents->GetRenderViewHost()) {
    web_ui()->CallJavascriptFunction(
        "extensions.ExtensionSettings.onExtensionsChanged");
  }
  deleting_rvh_ = NULL;
}

}  // namespace extensions
