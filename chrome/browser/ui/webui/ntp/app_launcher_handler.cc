// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"

#include <vector>

#include "apps/metrics_names.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/ntp/core_app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_application_info.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/favicon_url.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_set.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

using content::WebContents;
using extensions::AppSorting;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::UnloadedExtensionInfo;

namespace {

void RecordAppLauncherPromoHistogram(
      apps::AppLauncherPromoHistogramValues value) {
  DCHECK_LT(value, apps::APP_LAUNCHER_PROMO_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.AppLauncherPromo", value, apps::APP_LAUNCHER_PROMO_MAX);
}

// This is used to avoid a DCHECK due to an unhandled WebUI callback. The
// JavaScript used to switch between pages sends "pageSelected" which is used
// in the context of the NTP for recording metrics we don't need here.
void NoOpCallback(const base::ListValue* args) {}

}  // namespace

AppLauncherHandler::AppInstallInfo::AppInstallInfo() {}

AppLauncherHandler::AppInstallInfo::~AppInstallInfo() {}

AppLauncherHandler::AppLauncherHandler(ExtensionService* extension_service)
    : extension_service_(extension_service),
      ignore_changes_(false),
      attempted_bookmark_app_install_(false),
      has_loaded_apps_(false) {
  if (IsAppLauncherEnabled())
    RecordAppLauncherPromoHistogram(apps::APP_LAUNCHER_PROMO_ALREADY_INSTALLED);
  else if (ShouldShowAppLauncherPromo())
    RecordAppLauncherPromoHistogram(apps::APP_LAUNCHER_PROMO_SHOWN);
}

AppLauncherHandler::~AppLauncherHandler() {}

void AppLauncherHandler::CreateAppInfo(
    const Extension* extension,
    ExtensionService* service,
    base::DictionaryValue* value) {
  value->Clear();

  // The Extension class 'helpfully' wraps bidi control characters that
  // impede our ability to determine directionality.
  base::string16 short_name = base::UTF8ToUTF16(extension->short_name());
  base::i18n::UnadjustStringForLocaleDirection(&short_name);
  NewTabUI::SetUrlTitleAndDirection(
      value,
      short_name,
      extensions::AppLaunchInfo::GetFullLaunchURL(extension));

  base::string16 name = base::UTF8ToUTF16(extension->name());
  base::i18n::UnadjustStringForLocaleDirection(&name);
  NewTabUI::SetFullNameAndDirection(name, value);

  bool enabled =
      service->IsExtensionEnabled(extension->id()) &&
      !extensions::ExtensionRegistry::Get(service->GetBrowserContext())
           ->GetExtensionById(extension->id(),
                              extensions::ExtensionRegistry::TERMINATED);
  extensions::GetExtensionBasicInfo(extension, enabled, value);

  value->SetBoolean("mayDisable", extensions::ExtensionSystem::Get(
      service->profile())->management_policy()->UserMayModifySettings(
      extension, NULL));

  bool icon_big_exists = true;
  // Instead of setting grayscale here, we do it in apps_page.js.
  GURL icon_big = extensions::ExtensionIconSource::GetIconURL(
      extension,
      extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER,
      false,
      &icon_big_exists);
  value->SetString("icon_big", icon_big.spec());
  value->SetBoolean("icon_big_exists", icon_big_exists);
  bool icon_small_exists = true;
  GURL icon_small = extensions::ExtensionIconSource::GetIconURL(
      extension,
      extension_misc::EXTENSION_ICON_BITTY,
      ExtensionIconSet::MATCH_BIGGER,
      false,
      &icon_small_exists);
  value->SetString("icon_small", icon_small.spec());
  value->SetBoolean("icon_small_exists", icon_small_exists);
  value->SetInteger("launch_container",
                    extensions::AppLaunchInfo::GetLaunchContainer(extension));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(service->profile());
  value->SetInteger("launch_type", extensions::GetLaunchType(prefs, extension));
  value->SetBoolean("is_component",
                    extension->location() == extensions::Manifest::COMPONENT);
  value->SetBoolean("is_webstore",
      extension->id() == extension_misc::kWebStoreAppId);

  AppSorting* sorting = prefs->app_sorting();
  syncer::StringOrdinal page_ordinal = sorting->GetPageOrdinal(extension->id());
  if (!page_ordinal.IsValid()) {
    // Make sure every app has a page ordinal (some predate the page ordinal).
    // The webstore app should be on the first page.
    page_ordinal = extension->id() == extension_misc::kWebStoreAppId ?
        sorting->CreateFirstAppPageOrdinal() :
        sorting->GetNaturalAppPageOrdinal();
    sorting->SetPageOrdinal(extension->id(), page_ordinal);
  }
  value->SetInteger("page_index",
      sorting->PageStringOrdinalAsInteger(page_ordinal));

  syncer::StringOrdinal app_launch_ordinal =
      sorting->GetAppLaunchOrdinal(extension->id());
  if (!app_launch_ordinal.IsValid()) {
    // Make sure every app has a launch ordinal (some predate the launch
    // ordinal). The webstore's app launch ordinal is always set to the first
    // position.
    app_launch_ordinal = extension->id() == extension_misc::kWebStoreAppId ?
        sorting->CreateFirstAppLaunchOrdinal(page_ordinal) :
        sorting->CreateNextAppLaunchOrdinal(page_ordinal);
    sorting->SetAppLaunchOrdinal(extension->id(), app_launch_ordinal);
  }
  value->SetString("app_launch_ordinal", app_launch_ordinal.ToInternalValue());
}

void AppLauncherHandler::RegisterMessages() {
  registrar_.Add(this, chrome::NOTIFICATION_APP_INSTALLED_TO_NTP,
      content::Source<WebContents>(web_ui()->GetWebContents()));

  // Some tests don't have a local state.
#if defined(ENABLE_APP_LIST)
  if (g_browser_process->local_state()) {
    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kShowAppLauncherPromo,
        base::Bind(&AppLauncherHandler::OnLocalStatePreferenceChanged,
                   base::Unretained(this)));
  }
#endif
  web_ui()->RegisterMessageCallback("getApps",
      base::Bind(&AppLauncherHandler::HandleGetApps,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("launchApp",
      base::Bind(&AppLauncherHandler::HandleLaunchApp,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setLaunchType",
      base::Bind(&AppLauncherHandler::HandleSetLaunchType,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("uninstallApp",
      base::Bind(&AppLauncherHandler::HandleUninstallApp,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("createAppShortcut",
      base::Bind(&AppLauncherHandler::HandleCreateAppShortcut,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("reorderApps",
      base::Bind(&AppLauncherHandler::HandleReorderApps,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setPageIndex",
      base::Bind(&AppLauncherHandler::HandleSetPageIndex,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("saveAppPageName",
      base::Bind(&AppLauncherHandler::HandleSaveAppPageName,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("generateAppForLink",
      base::Bind(&AppLauncherHandler::HandleGenerateAppForLink,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("stopShowingAppLauncherPromo",
      base::Bind(&AppLauncherHandler::StopShowingAppLauncherPromo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onLearnMore",
      base::Bind(&AppLauncherHandler::OnLearnMore,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("pageSelected", base::Bind(&NoOpCallback));
}

void AppLauncherHandler::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_INSTALLED_TO_NTP) {
    highlight_app_id_ = *content::Details<const std::string>(details).ptr();
    if (has_loaded_apps_)
      SetAppToBeHighlighted();
    return;
  }

  if (ignore_changes_ || !has_loaded_apps_)
    return;

  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (!extension->is_app())
        return;

      if (!extensions::ui_util::ShouldDisplayInNewTabPage(
              extension, Profile::FromWebUI(web_ui()))) {
        return;
      }

      scoped_ptr<base::DictionaryValue> app_info(GetAppInfo(extension));
      if (app_info.get()) {
        visible_apps_.insert(extension->id());

        ExtensionPrefs* prefs =
            ExtensionPrefs::Get(extension_service_->profile());
        base::FundamentalValue highlight(
            prefs->IsFromBookmark(extension->id()) &&
            attempted_bookmark_app_install_);
        attempted_bookmark_app_install_ = false;
        web_ui()->CallJavascriptFunction("ntp.appAdded", *app_info, highlight);
      }

      break;
    }
    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED:
    case extensions::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED: {
      const Extension* extension = NULL;
      bool uninstalled = false;
      if (type == extensions::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED) {
        extension = content::Details<const Extension>(details).ptr();
        uninstalled = true;
      } else {  // NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED
        if (content::Details<UnloadedExtensionInfo>(details)->reason ==
            UnloadedExtensionInfo::REASON_UNINSTALL) {
          // Uninstalls are tracked by
          // NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED.
          return;
        }
        extension = content::Details<extensions::UnloadedExtensionInfo>(
            details)->extension;
        uninstalled = false;
      }
      if (!extension->is_app())
        return;

      if (!extensions::ui_util::ShouldDisplayInNewTabPage(
              extension, Profile::FromWebUI(web_ui()))) {
        return;
      }

      scoped_ptr<base::DictionaryValue> app_info(GetAppInfo(extension));
      if (app_info.get()) {
        if (uninstalled)
          visible_apps_.erase(extension->id());

        web_ui()->CallJavascriptFunction(
            "ntp.appRemoved",
            *app_info,
            base::FundamentalValue(uninstalled),
            base::FundamentalValue(!extension_id_prompting_.empty()));
      }
      break;
    }
    case chrome::NOTIFICATION_APP_LAUNCHER_REORDERED: {
      const std::string* id =
          content::Details<const std::string>(details).ptr();
      if (id) {
        const Extension* extension =
            extension_service_->GetInstalledExtension(*id);
        if (!extension) {
          // Extension could still be downloading or installing.
          return;
        }

        base::DictionaryValue app_info;
        CreateAppInfo(extension,
                      extension_service_,
                      &app_info);
        web_ui()->CallJavascriptFunction("ntp.appMoved", app_info);
      } else {
        HandleGetApps(NULL);
      }
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      CrxInstaller* crx_installer = content::Source<CrxInstaller>(source).ptr();
      if (!Profile::FromWebUI(web_ui())->IsSameProfile(
              crx_installer->profile())) {
        return;
      }
      // Fall through.
    }
    case extensions::NOTIFICATION_EXTENSION_LOAD_ERROR: {
      attempted_bookmark_app_install_ = false;
      break;
    }
    default:
      NOTREACHED();
  }
}

void AppLauncherHandler::FillAppDictionary(base::DictionaryValue* dictionary) {
  // CreateAppInfo and ClearOrdinals can change the extension prefs.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);

  base::ListValue* list = new base::ListValue();
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  for (std::set<std::string>::iterator it = visible_apps_.begin();
       it != visible_apps_.end(); ++it) {
    const Extension* extension = extension_service_->GetInstalledExtension(*it);
    if (extension && extensions::ui_util::ShouldDisplayInNewTabPage(
            extension, profile)) {
      base::DictionaryValue* app_info = GetAppInfo(extension);
      list->Append(app_info);
    }
  }

  dictionary->Set("apps", list);

  const base::ListValue* app_page_names =
      prefs->GetList(prefs::kNtpAppPageNames);
  if (!app_page_names || !app_page_names->GetSize()) {
    ListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
    base::ListValue* list = update.Get();
    list->Set(0, new base::StringValue(
        l10n_util::GetStringUTF16(IDS_APP_DEFAULT_PAGE_NAME)));
    dictionary->Set("appPageNames",
                    static_cast<base::ListValue*>(list->DeepCopy()));
  } else {
    dictionary->Set("appPageNames",
                    static_cast<base::ListValue*>(app_page_names->DeepCopy()));
  }
}

base::DictionaryValue* AppLauncherHandler::GetAppInfo(
    const Extension* extension) {
  base::DictionaryValue* app_info = new base::DictionaryValue();
  // CreateAppInfo can change the extension prefs.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  CreateAppInfo(extension,
                extension_service_,
                app_info);
  return app_info;
}

void AppLauncherHandler::HandleGetApps(const base::ListValue* args) {
  base::DictionaryValue dictionary;

  // Tell the client whether to show the promo for this view. We don't do this
  // in the case of PREF_CHANGED because:
  //
  // a) At that point in time, depending on the pref that changed, it can look
  //    like the set of apps installed has changed, and we will mark the promo
  //    expired.
  // b) Conceptually, it doesn't really make sense to count a
  //    prefchange-triggered refresh as a promo 'view'.
  Profile* profile = Profile::FromWebUI(web_ui());

  // The first time we load the apps we must add all current app to the list
  // of apps visible on the NTP.
  if (!has_loaded_apps_) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
    const ExtensionSet& enabled_set = registry->enabled_extensions();
    for (extensions::ExtensionSet::const_iterator it = enabled_set.begin();
         it != enabled_set.end(); ++it) {
      visible_apps_.insert((*it)->id());
    }

    const ExtensionSet& disabled_set = registry->disabled_extensions();
    for (ExtensionSet::const_iterator it = disabled_set.begin();
         it != disabled_set.end(); ++it) {
      visible_apps_.insert((*it)->id());
    }

    const ExtensionSet& terminated_set = registry->terminated_extensions();
    for (ExtensionSet::const_iterator it = terminated_set.begin();
         it != terminated_set.end(); ++it) {
      visible_apps_.insert((*it)->id());
    }
  }

  SetAppToBeHighlighted();
  FillAppDictionary(&dictionary);
  web_ui()->CallJavascriptFunction("ntp.getAppsCallback", dictionary);

  // First time we get here we set up the observer so that we can tell update
  // the apps as they change.
  if (!has_loaded_apps_) {
    base::Closure callback = base::Bind(
        &AppLauncherHandler::OnExtensionPreferenceChanged,
        base::Unretained(this));
    extension_pref_change_registrar_.Init(
        ExtensionPrefs::Get(profile)->pref_service());
    extension_pref_change_registrar_.Add(
        extensions::pref_names::kExtensions, callback);
    extension_pref_change_registrar_.Add(prefs::kNtpAppPageNames, callback);

    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                   content::Source<Profile>(profile));
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                   content::Source<Profile>(profile));
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
                   content::Source<Profile>(profile));
    registrar_.Add(this,
                   chrome::NOTIFICATION_APP_LAUNCHER_REORDERED,
                   content::Source<AppSorting>(
                       ExtensionPrefs::Get(profile)->app_sorting()));
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                   content::Source<CrxInstaller>(NULL));
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_LOAD_ERROR,
                   content::Source<Profile>(profile));
  }

  has_loaded_apps_ = true;
}

void AppLauncherHandler::HandleLaunchApp(const base::ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));
  double source = -1.0;
  CHECK(args->GetDouble(1, &source));
  std::string url;
  if (args->GetSize() > 2)
    CHECK(args->GetString(2, &url));

  extension_misc::AppLaunchBucket launch_bucket =
      static_cast<extension_misc::AppLaunchBucket>(
          static_cast<int>(source));
  CHECK(launch_bucket >= 0 &&
        launch_bucket < extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, false);

  // Prompt the user to re-enable the application if disabled.
  if (!extension) {
    PromptToEnableApp(extension_id);
    return;
  }

  Profile* profile = extension_service_->profile();

  WindowOpenDisposition disposition = args->GetSize() > 3 ?
        webui::GetDispositionFromClick(args, 3) : CURRENT_TAB;
  if (extension_id != extension_misc::kWebStoreAppId) {
    CHECK_NE(launch_bucket, extension_misc::APP_LAUNCH_BUCKET_INVALID);
    CoreAppLauncherHandler::RecordAppLaunchType(launch_bucket,
                                                extension->GetType());
  } else {
    CoreAppLauncherHandler::RecordWebStoreLaunch();
  }

  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB ||
      disposition == NEW_WINDOW) {
    // TODO(jamescook): Proper support for background tabs.
    AppLaunchParams params(profile, extension,
                           disposition == NEW_WINDOW ?
                               extensions::LAUNCH_CONTAINER_WINDOW :
                               extensions::LAUNCH_CONTAINER_TAB,
                           disposition);
    params.override_url = GURL(url);
    OpenApplication(params);
  } else {
    // To give a more "launchy" experience when using the NTP launcher, we close
    // it automatically.
    Browser* browser = chrome::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    WebContents* old_contents = NULL;
    if (browser)
      old_contents = browser->tab_strip_model()->GetActiveWebContents();

    AppLaunchParams params(profile, extension,
                           old_contents ? CURRENT_TAB : NEW_FOREGROUND_TAB);
    params.override_url = GURL(url);
    WebContents* new_contents = OpenApplication(params);

    // This will also destroy the handler, so do not perform any actions after.
    if (new_contents != old_contents && browser &&
        browser->tab_strip_model()->count() > 1) {
      chrome::CloseWebContents(browser, old_contents, true);
    }
  }
}

void AppLauncherHandler::HandleSetLaunchType(const base::ListValue* args) {
  std::string extension_id;
  double launch_type;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetDouble(1, &launch_type));

  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  if (!extension)
    return;

  // Don't update the page; it already knows about the launch type change.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);

  extensions::SetLaunchType(
      extension_service_,
      extension_id,
      static_cast<extensions::LaunchType>(static_cast<int>(launch_type)));
}

void AppLauncherHandler::HandleUninstallApp(const base::ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  const Extension* extension = extension_service_->GetInstalledExtension(
      extension_id);
  if (!extension)
    return;

  if (!extensions::ExtensionSystem::Get(extension_service_->profile())->
          management_policy()->UserMayModifySettings(extension, NULL)) {
    LOG(ERROR) << "Attempt to uninstall an extension that is non-usermanagable "
               << "was made. Extension id : " << extension->id();
    return;
  }
  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;

  bool dont_confirm = false;
  if (args->GetBoolean(1, &dont_confirm) && dont_confirm) {
    base::AutoReset<bool> auto_reset(&ignore_changes_, true);
    ExtensionUninstallAccepted();
  } else {
    GetExtensionUninstallDialog()->ConfirmUninstall(extension);
  }
}

void AppLauncherHandler::HandleCreateAppShortcut(const base::ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  if (!extension)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), extension,
      base::Callback<void(bool)>());
}

void AppLauncherHandler::HandleReorderApps(const base::ListValue* args) {
  CHECK(args->GetSize() == 2);

  std::string dragged_app_id;
  const base::ListValue* app_order;
  CHECK(args->GetString(0, &dragged_app_id));
  CHECK(args->GetList(1, &app_order));

  std::string predecessor_to_moved_ext;
  std::string successor_to_moved_ext;
  for (size_t i = 0; i < app_order->GetSize(); ++i) {
    std::string value;
    if (app_order->GetString(i, &value) && value == dragged_app_id) {
      if (i > 0)
        CHECK(app_order->GetString(i - 1, &predecessor_to_moved_ext));
      if (i + 1 < app_order->GetSize())
        CHECK(app_order->GetString(i + 1, &successor_to_moved_ext));
      break;
    }
  }

  // Don't update the page; it already knows the apps have been reordered.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  ExtensionPrefs* extension_prefs =
      ExtensionPrefs::Get(extension_service_->GetBrowserContext());
  extension_prefs->SetAppDraggedByUser(dragged_app_id);
  extension_prefs->app_sorting()->OnExtensionMoved(
      dragged_app_id, predecessor_to_moved_ext, successor_to_moved_ext);
}

void AppLauncherHandler::HandleSetPageIndex(const base::ListValue* args) {
  AppSorting* app_sorting =
      ExtensionPrefs::Get(extension_service_->profile())->app_sorting();

  std::string extension_id;
  double page_index;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetDouble(1, &page_index));
  const syncer::StringOrdinal& page_ordinal =
      app_sorting->PageIntegerAsStringOrdinal(static_cast<size_t>(page_index));

  // Don't update the page; it already knows the apps have been reordered.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  app_sorting->SetPageOrdinal(extension_id, page_ordinal);
}

void AppLauncherHandler::HandleSaveAppPageName(const base::ListValue* args) {
  base::string16 name;
  CHECK(args->GetString(0, &name));

  double page_index;
  CHECK(args->GetDouble(1, &page_index));

  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  ListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
  base::ListValue* list = update.Get();
  list->Set(static_cast<size_t>(page_index), new base::StringValue(name));
}

void AppLauncherHandler::HandleGenerateAppForLink(const base::ListValue* args) {
  std::string url;
  CHECK(args->GetString(0, &url));
  GURL launch_url(url);

  base::string16 title;
  CHECK(args->GetString(1, &title));

  double page_index;
  CHECK(args->GetDouble(2, &page_index));
  AppSorting* app_sorting =
      ExtensionPrefs::Get(extension_service_->profile())->app_sorting();
  const syncer::StringOrdinal& page_ordinal =
      app_sorting->PageIntegerAsStringOrdinal(static_cast<size_t>(page_index));

  Profile* profile = Profile::FromWebUI(web_ui());
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);
  if (!favicon_service) {
    LOG(ERROR) << "No favicon service";
    return;
  }

  scoped_ptr<AppInstallInfo> install_info(new AppInstallInfo());
  install_info->title = title;
  install_info->app_url = launch_url;
  install_info->page_ordinal = page_ordinal;

  favicon_service->GetFaviconImageForPageURL(
      launch_url,
      base::Bind(&AppLauncherHandler::OnFaviconForApp,
                 base::Unretained(this),
                 base::Passed(&install_info)),
      &cancelable_task_tracker_);
}

void AppLauncherHandler::StopShowingAppLauncherPromo(
    const base::ListValue* args) {
#if defined(ENABLE_APP_LIST)
  g_browser_process->local_state()->SetBoolean(
      prefs::kShowAppLauncherPromo, false);
  RecordAppLauncherPromoHistogram(apps::APP_LAUNCHER_PROMO_DISMISSED);
#endif
}

void AppLauncherHandler::OnLearnMore(const base::ListValue* args) {
  RecordAppLauncherPromoHistogram(apps::APP_LAUNCHER_PROMO_LEARN_MORE);
}

void AppLauncherHandler::OnFaviconForApp(
    scoped_ptr<AppInstallInfo> install_info,
    const favicon_base::FaviconImageResult& image_result) {
  scoped_ptr<WebApplicationInfo> web_app(new WebApplicationInfo());
  web_app->title = install_info->title;
  web_app->app_url = install_info->app_url;

  if (!image_result.image.IsEmpty()) {
    WebApplicationInfo::IconInfo icon;
    icon.data = image_result.image.AsBitmap();
    icon.width = icon.data.width();
    icon.height = icon.data.height();
    web_app->icons.push_back(icon);
  }

  scoped_refptr<CrxInstaller> installer(
      CrxInstaller::CreateSilent(extension_service_));
  installer->set_error_on_unsupported_requirements(true);
  installer->set_page_ordinal(install_info->page_ordinal);
  installer->InstallWebApp(*web_app);
  attempted_bookmark_app_install_ = true;
}

void AppLauncherHandler::SetAppToBeHighlighted() {
  if (highlight_app_id_.empty())
    return;

  base::StringValue app_id(highlight_app_id_);
  web_ui()->CallJavascriptFunction("ntp.setAppToBeHighlighted", app_id);
  highlight_app_id_.clear();
}

void AppLauncherHandler::OnExtensionPreferenceChanged() {
  base::DictionaryValue dictionary;
  FillAppDictionary(&dictionary);
  web_ui()->CallJavascriptFunction("ntp.appsPrefChangeCallback", dictionary);
}

void AppLauncherHandler::OnLocalStatePreferenceChanged() {
#if defined(ENABLE_APP_LIST)
  web_ui()->CallJavascriptFunction(
      "ntp.appLauncherPromoPrefChangeCallback",
      base::FundamentalValue(g_browser_process->local_state()->GetBoolean(
          prefs::kShowAppLauncherPromo)));
#endif
}

void AppLauncherHandler::CleanupAfterUninstall() {
  extension_id_prompting_.clear();
}

void AppLauncherHandler::PromptToEnableApp(const std::string& extension_id) {
  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;
  extension_enable_flow_.reset(new ExtensionEnableFlow(
      Profile::FromWebUI(web_ui()), extension_id, this));
  extension_enable_flow_->StartForWebContents(web_ui()->GetWebContents());
}

void AppLauncherHandler::ExtensionUninstallAccepted() {
  // Do the uninstall work here.
  DCHECK(!extension_id_prompting_.empty());

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension =
      extension_service_->GetInstalledExtension(extension_id_prompting_);
  if (!extension)
    return;

  extension_service_->UninstallExtension(
      extension_id_prompting_,
      extensions::UNINSTALL_REASON_USER_INITIATED,
      base::Bind(&base::DoNothing),
      NULL);
  CleanupAfterUninstall();
}

void AppLauncherHandler::ExtensionUninstallCanceled() {
  CleanupAfterUninstall();
}

void AppLauncherHandler::ExtensionEnableFlowFinished() {
  DCHECK_EQ(extension_id_prompting_, extension_enable_flow_->extension_id());

  // We bounce this off the NTP so the browser can update the apps icon.
  // If we don't launch the app asynchronously, then the app's disabled
  // icon disappears but isn't replaced by the enabled icon, making a poor
  // visual experience.
  base::StringValue app_id(extension_id_prompting_);
  web_ui()->CallJavascriptFunction("ntp.launchAppAfterEnable", app_id);

  extension_enable_flow_.reset();
  extension_id_prompting_ = "";
}

void AppLauncherHandler::ExtensionEnableFlowAborted(bool user_initiated) {
  DCHECK_EQ(extension_id_prompting_, extension_enable_flow_->extension_id());

  // We record the histograms here because ExtensionUninstallCanceled is also
  // called when the extension uninstall dialog is canceled.
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id_prompting_, true);
  std::string histogram_name = user_initiated
                                   ? "Extensions.Permissions_ReEnableCancel2"
                                   : "Extensions.Permissions_ReEnableAbort2";
  ExtensionService::RecordPermissionMessagesHistogram(
      extension, histogram_name.c_str());

  extension_enable_flow_.reset();
  CleanupAfterUninstall();
}

extensions::ExtensionUninstallDialog*
AppLauncherHandler::GetExtensionUninstallDialog() {
  if (!extension_uninstall_dialog_.get()) {
    Browser* browser = chrome::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    extension_uninstall_dialog_.reset(
        extensions::ExtensionUninstallDialog::Create(
            extension_service_->profile(),
            browser->window()->GetNativeWindow(),
            this));
  }
  return extension_uninstall_dialog_.get();
}
