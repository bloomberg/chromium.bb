// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/app_notification.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/favicon_url.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_apps.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/animation/animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

using application_launch::LaunchParams;
using application_launch::OpenApplication;
using content::WebContents;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionPrefs;

namespace {

const net::UnescapeRule::Type kUnescapeRules =
    net::UnescapeRule::NORMAL | net::UnescapeRule::URL_SPECIAL_CHARS;

extension_misc::AppLaunchBucket ParseLaunchSource(
    const std::string& launch_source) {
  int bucket_num = extension_misc::APP_LAUNCH_BUCKET_INVALID;
  base::StringToInt(launch_source, &bucket_num);
  extension_misc::AppLaunchBucket bucket =
      static_cast<extension_misc::AppLaunchBucket>(bucket_num);
  CHECK(bucket < extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  return bucket;
}

}  // namespace

AppLauncherHandler::AppInstallInfo::AppInstallInfo() {}

AppLauncherHandler::AppInstallInfo::~AppInstallInfo() {}

AppLauncherHandler::AppLauncherHandler(ExtensionService* extension_service)
    : extension_service_(extension_service),
      ignore_changes_(false),
      attempted_bookmark_app_install_(false),
      has_loaded_apps_(false) {
}

AppLauncherHandler::~AppLauncherHandler() {}

// Serializes |notification| into a new DictionaryValue which the caller then
// owns.
static DictionaryValue* SerializeNotification(
    const extensions::AppNotification& notification) {
  DictionaryValue* dictionary = new DictionaryValue();
  dictionary->SetString("title", notification.title());
  dictionary->SetString("body", notification.body());
  if (!notification.link_url().is_empty()) {
    dictionary->SetString("linkUrl", notification.link_url().spec());
    dictionary->SetString("linkText", notification.link_text());
  }
  return dictionary;
}

void AppLauncherHandler::CreateAppInfo(
    const Extension* extension,
    const extensions::AppNotification* notification,
    ExtensionService* service,
    DictionaryValue* value) {
  value->Clear();

  // The Extension class 'helpfully' wraps bidi control characters that
  // impede our ability to determine directionality.
  string16 name = UTF8ToUTF16(extension->name());
  base::i18n::UnadjustStringForLocaleDirection(&name);
  NewTabUI::SetUrlTitleAndDirection(value, name, extension->GetFullLaunchURL());

  bool enabled = service->IsExtensionEnabled(extension->id()) &&
      !service->GetTerminatedExtension(extension->id());
  extension->GetBasicInfo(enabled, value);

  value->SetBoolean("mayDisable", extensions::ExtensionSystem::Get(
      service->profile())->management_policy()->UserMayModifySettings(
      extension, NULL));

  bool icon_big_exists = true;
  // Instead of setting grayscale here, we do it in apps_page.js.
  GURL icon_big =
      ExtensionIconSource::GetIconURL(extension,
                                      extension_misc::EXTENSION_ICON_LARGE,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      false, &icon_big_exists);
  value->SetString("icon_big", icon_big.spec());
  value->SetBoolean("icon_big_exists", icon_big_exists);
  bool icon_small_exists = true;
  GURL icon_small =
      ExtensionIconSource::GetIconURL(extension,
                                      extension_misc::EXTENSION_ICON_BITTY,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      false, &icon_small_exists);
  value->SetString("icon_small", icon_small.spec());
  value->SetBoolean("icon_small_exists", icon_small_exists);
  value->SetInteger("launch_container", extension->launch_container());
  ExtensionPrefs* prefs = service->extension_prefs();
  value->SetInteger("launch_type",
      prefs->GetLaunchType(extension->id(),
                           ExtensionPrefs::LAUNCH_DEFAULT));
  value->SetBoolean("is_component",
      extension->location() == Extension::COMPONENT);
  value->SetBoolean("is_webstore",
      extension->id() == extension_misc::kWebStoreAppId);

  if (extension->HasAPIPermission(
        extensions::APIPermission::kAppNotifications)) {
    value->SetBoolean("notifications_disabled",
                      prefs->IsAppNotificationDisabled(extension->id()));
  }

  if (notification)
    value->Set("notification", SerializeNotification(*notification));

  ExtensionSorting* sorting = prefs->extension_sorting();
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
  web_ui()->RegisterMessageCallback("recordAppLaunchByURL",
      base::Bind(&AppLauncherHandler::HandleRecordAppLaunchByUrl,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("closeNotification",
      base::Bind(&AppLauncherHandler::HandleNotificationClose,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setNotificationsDisabled",
      base::Bind(&AppLauncherHandler::HandleSetNotificationsDisabled,
                 base::Unretained(this)));
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
    case chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED: {
      const std::string& id =
          *content::Details<const std::string>(details).ptr();
      const extensions::AppNotification* notification =
          extension_service_->app_notification_manager()->GetLast(id);
      base::StringValue id_value(id);
      if (notification) {
        scoped_ptr<DictionaryValue> notification_value(
            SerializeNotification(*notification));
        web_ui()->CallJavascriptFunction("ntp.appNotificationChanged",
            id_value, *notification_value.get());
      } else {
        web_ui()->CallJavascriptFunction("ntp.appNotificationChanged",
                                         id_value);
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (!extension->is_app())
        return;

      scoped_ptr<DictionaryValue> app_info(GetAppInfo(extension));
      if (app_info.get()) {
        visible_apps_.insert(extension->id());

        ExtensionPrefs* prefs = extension_service_->extension_prefs();
        scoped_ptr<base::FundamentalValue> highlight(Value::CreateBooleanValue(
              prefs->IsFromBookmark(extension->id()) &&
              attempted_bookmark_app_install_));
        attempted_bookmark_app_install_ = false;
        web_ui()->CallJavascriptFunction(
            "ntp.appAdded", *app_info, *highlight);
      }

      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<extensions::UnloadedExtensionInfo>(
              details)->extension;
      if (!extension->is_app())
        return;

      scoped_ptr<DictionaryValue> app_info(GetAppInfo(extension));
      scoped_ptr<base::FundamentalValue> uninstall_value(
          Value::CreateBooleanValue(
              content::Details<extensions::UnloadedExtensionInfo>(
                  details)->reason == extension_misc::UNLOAD_REASON_UNINSTALL));
      if (app_info.get()) {
        visible_apps_.erase(extension->id());

        scoped_ptr<base::FundamentalValue> from_page(
            Value::CreateBooleanValue(!extension_id_prompting_.empty()));
        web_ui()->CallJavascriptFunction(
            "ntp.appRemoved", *app_info, *uninstall_value, *from_page);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LAUNCHER_REORDERED: {
      const std::string* id =
          content::Details<const std::string>(details).ptr();
      if (id) {
        const Extension* extension =
            extension_service_->GetExtensionById(*id, false);
        DictionaryValue app_info;
        CreateAppInfo(extension,
                      NULL,
                      extension_service_,
                      &app_info);
        web_ui()->CallJavascriptFunction("ntp.appMoved", app_info);
      } else {
        HandleGetApps(NULL);
      }
      break;
    }
    // The promo may not load until a couple seconds after the first NTP view,
    // so we listen for the load notification and notify the NTP when ready.
    case chrome::NOTIFICATION_WEB_STORE_PROMO_LOADED:
      // TODO(estade): Try to get rid of this inefficient operation.
      HandleGetApps(NULL);
      break;
    case chrome::NOTIFICATION_PREF_CHANGED: {
      DictionaryValue dictionary;
      FillAppDictionary(&dictionary);
      web_ui()->CallJavascriptFunction("ntp.appsPrefChangeCallback",
                                       dictionary);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      CrxInstaller* crx_installer = content::Source<CrxInstaller>(source).ptr();
      if (!Profile::FromWebUI(web_ui())->IsSameProfile(
              crx_installer->profile())) {
        return;
      }
      // Fall through.
    }
    case chrome::NOTIFICATION_EXTENSION_LOAD_ERROR: {
      attempted_bookmark_app_install_ = false;
      break;
    }
    default:
      NOTREACHED();
  }
}

void AppLauncherHandler::FillAppDictionary(DictionaryValue* dictionary) {
  // CreateAppInfo and ClearOrdinals can change the extension prefs.
  AutoReset<bool> auto_reset(&ignore_changes_, true);

  ListValue* list = new ListValue();

  for (std::set<std::string>::iterator it = visible_apps_.begin();
       it != visible_apps_.end(); ++it) {
    const Extension* extension = extension_service_->GetInstalledExtension(*it);
    if (extension && extension->ShouldDisplayInLauncher()) {
      DictionaryValue* app_info = GetAppInfo(extension);
      list->Append(app_info);
    }
  }

  dictionary->Set("apps", list);

  // TODO(estade): remove these settings when the old NTP is removed. The new
  // NTP does it in js.
#if defined(OS_MACOSX)
  // App windows are not yet implemented on mac.
  dictionary->SetBoolean("disableAppWindowLaunch", true);
  dictionary->SetBoolean("disableCreateAppShortcut", true);
#endif

#if defined(OS_CHROMEOS)
  // Making shortcut does not make sense on ChromeOS because it does not have
  // a desktop.
  dictionary->SetBoolean("disableCreateAppShortcut", true);
#endif

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  const ListValue* app_page_names = prefs->GetList(prefs::kNtpAppPageNames);
  if (!app_page_names || !app_page_names->GetSize()) {
    ListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
    ListValue* list = update.Get();
    list->Set(0, Value::CreateStringValue(
        l10n_util::GetStringUTF16(IDS_APP_DEFAULT_PAGE_NAME)));
    dictionary->Set("appPageNames",
                    static_cast<ListValue*>(list->DeepCopy()));
  } else {
    dictionary->Set("appPageNames",
                    static_cast<ListValue*>(app_page_names->DeepCopy()));
  }
}

DictionaryValue* AppLauncherHandler::GetAppInfo(const Extension* extension) {
  extensions::AppNotificationManager* notification_manager =
      extension_service_->app_notification_manager();
  DictionaryValue* app_info = new DictionaryValue();
  // CreateAppInfo can change the extension prefs.
  AutoReset<bool> auto_reset(&ignore_changes_, true);
  CreateAppInfo(extension,
                notification_manager->GetLast(extension->id()),
                extension_service_,
                app_info);
  return app_info;
}

void AppLauncherHandler::HandleGetApps(const ListValue* args) {
  DictionaryValue dictionary;

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
    const ExtensionSet* extensions = extension_service_->extensions();
    for (ExtensionSet::const_iterator it = extensions->begin();
         it != extensions->end(); ++it) {
      visible_apps_.insert((*it)->id());
    }

    extensions = extension_service_->disabled_extensions();
    for (ExtensionSet::const_iterator it = extensions->begin();
         it != extensions->end(); ++it) {
      visible_apps_.insert((*it)->id());
    }

    extensions = extension_service_->terminated_extensions();
    for (ExtensionSet::const_iterator it = extensions->begin();
         it != extensions->end(); ++it) {
      visible_apps_.insert((*it)->id());
    }
  }

  SetAppToBeHighlighted();
  FillAppDictionary(&dictionary);
  web_ui()->CallJavascriptFunction("ntp.getAppsCallback", dictionary);

  // First time we get here we set up the observer so that we can tell update
  // the apps as they change.
  if (!has_loaded_apps_) {
    pref_change_registrar_.Init(
        extension_service_->extension_prefs()->pref_service());
    pref_change_registrar_.Add(ExtensionPrefs::kExtensionsPref, this);
    pref_change_registrar_.Add(prefs::kNtpAppPageNames, this);

    registrar_.Add(this, chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
        content::Source<Profile>(profile));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
        content::Source<Profile>(profile));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
        content::Source<Profile>(profile));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LAUNCHER_REORDERED,
        content::Source<ExtensionSorting>(
            extension_service_->extension_prefs()->extension_sorting()));
    registrar_.Add(this, chrome::NOTIFICATION_WEB_STORE_PROMO_LOADED,
        content::Source<Profile>(profile));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
        content::Source<CrxInstaller>(NULL));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOAD_ERROR,
        content::Source<Profile>(profile));
  }

  has_loaded_apps_ = true;
}

void AppLauncherHandler::HandleLaunchApp(const ListValue* args) {
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
        web_ui_util::GetDispositionFromClick(args, 3) : CURRENT_TAB;
  if (extension_id != extension_misc::kWebStoreAppId) {
    RecordAppLaunchByID(launch_bucket);
  } else {
    RecordWebStoreLaunch();
  }

  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB ||
      disposition == NEW_WINDOW) {
    // TODO(jamescook): Proper support for background tabs.
    LaunchParams params(profile, extension,
                        disposition == NEW_WINDOW ?
                            extension_misc::LAUNCH_WINDOW :
                            extension_misc::LAUNCH_TAB,
                        disposition);
    params.override_url = GURL(url);
    OpenApplication(params);
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    extension_misc::LaunchContainer launch_container =
        extension_service_->extension_prefs()->GetLaunchContainer(
            extension, ExtensionPrefs::LAUNCH_REGULAR);

    // To give a more "launchy" experience when using the NTP launcher, we close
    // it automatically.
    Browser* browser = browser::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    WebContents* old_contents = NULL;
    if (browser)
      old_contents = chrome::GetActiveWebContents(browser);

    LaunchParams params(profile, extension, launch_container,
                        old_contents ? CURRENT_TAB : NEW_FOREGROUND_TAB);
    params.override_url = GURL(url);
    WebContents* new_contents = OpenApplication(params);

    // This will also destroy the handler, so do not perform any actions after.
    if (new_contents != old_contents && browser && browser->tab_count() > 1)
      chrome::CloseWebContents(browser, old_contents);
  }
}

void AppLauncherHandler::HandleSetLaunchType(const ListValue* args) {
  std::string extension_id;
  double launch_type;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetDouble(1, &launch_type));

  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  if (!extension)
    return;

  // Don't update the page; it already knows about the launch type change.
  AutoReset<bool> auto_reset(&ignore_changes_, true);

  extension_service_->extension_prefs()->SetLaunchType(
      extension_id,
      static_cast<ExtensionPrefs::LaunchType>(
          static_cast<int>(launch_type)));
}

void AppLauncherHandler::HandleUninstallApp(const ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  const Extension* extension = extension_service_->GetExtensionById(
      extension_id, true);
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
    AutoReset<bool> auto_reset(&ignore_changes_, true);
    ExtensionUninstallAccepted();
  } else {
    GetExtensionUninstallDialog()->ConfirmUninstall(extension);
  }
}

void AppLauncherHandler::HandleCreateAppShortcut(const ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  if (!extension)
    return;

  Browser* browser = browser::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
  browser->window()->ShowCreateChromeAppShortcutsDialog(
      browser->profile(), extension);
}

void AppLauncherHandler::HandleReorderApps(const ListValue* args) {
  CHECK(args->GetSize() == 2);

  std::string dragged_app_id;
  const ListValue* app_order;
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
  AutoReset<bool> auto_reset(&ignore_changes_, true);
  extension_service_->extension_prefs()->SetAppDraggedByUser(dragged_app_id);
  extension_service_->OnExtensionMoved(dragged_app_id,
                                       predecessor_to_moved_ext,
                                       successor_to_moved_ext);
}

void AppLauncherHandler::HandleSetPageIndex(const ListValue* args) {
  ExtensionSorting* extension_sorting =
      extension_service_->extension_prefs()->extension_sorting();

  std::string extension_id;
  double page_index;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetDouble(1, &page_index));
  const syncer::StringOrdinal& page_ordinal =
      extension_sorting->PageIntegerAsStringOrdinal(
          static_cast<size_t>(page_index));

  // Don't update the page; it already knows the apps have been reordered.
  AutoReset<bool> auto_reset(&ignore_changes_, true);
  extension_sorting->SetPageOrdinal(extension_id, page_ordinal);
}

void AppLauncherHandler::HandleSaveAppPageName(const ListValue* args) {
  string16 name;
  CHECK(args->GetString(0, &name));

  double page_index;
  CHECK(args->GetDouble(1, &page_index));

  AutoReset<bool> auto_reset(&ignore_changes_, true);
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  ListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
  ListValue* list = update.Get();
  list->Set(static_cast<size_t>(page_index), Value::CreateStringValue(name));
}

void AppLauncherHandler::HandleGenerateAppForLink(const ListValue* args) {
  std::string url;
  CHECK(args->GetString(0, &url));
  GURL launch_url(url);

  string16 title;
  CHECK(args->GetString(1, &title));

  double page_index;
  CHECK(args->GetDouble(2, &page_index));
  ExtensionSorting* extension_sorting =
      extension_service_->extension_prefs()->extension_sorting();
  const syncer::StringOrdinal& page_ordinal =
      extension_sorting->PageIntegerAsStringOrdinal(
          static_cast<size_t>(page_index));

  Profile* profile = Profile::FromWebUI(web_ui());
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);
  if (!favicon_service) {
    LOG(ERROR) << "No favicon service";
    return;
  }

  scoped_ptr<AppInstallInfo> install_info(new AppInstallInfo());
  install_info->is_bookmark_app = true;
  install_info->title = title;
  install_info->app_url = launch_url;
  install_info->page_ordinal = page_ordinal;

  FaviconService::Handle h = favicon_service->GetFaviconImageForURL(
      FaviconService::FaviconForURLParams(profile, launch_url, history::FAVICON,
          gfx::kFaviconSize, &favicon_consumer_),
      base::Bind(&AppLauncherHandler::OnFaviconForApp, base::Unretained(this)));
  favicon_consumer_.SetClientData(favicon_service, h, install_info.release());
}

void AppLauncherHandler::HandleRecordAppLaunchByUrl(
    const base::ListValue* args) {
  std::string url;
  CHECK(args->GetString(0, &url));
  double source;
  CHECK(args->GetDouble(1, &source));

  extension_misc::AppLaunchBucket bucket =
      static_cast<extension_misc::AppLaunchBucket>(static_cast<int>(source));
  CHECK(source < extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

  RecordAppLaunchByUrl(Profile::FromWebUI(web_ui()), url, bucket);
}

void AppLauncherHandler::HandleNotificationClose(const ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  const Extension* extension = extension_service_->GetExtensionById(
      extension_id, true);
  if (!extension)
    return;

  UMA_HISTOGRAM_COUNTS("AppNotification.NTPNotificationClosed", 1);

  extensions::AppNotificationManager* notification_manager =
      extension_service_->app_notification_manager();
  notification_manager->ClearAll(extension_id);
}

void AppLauncherHandler::HandleSetNotificationsDisabled(
    const ListValue* args) {
  std::string extension_id;
  bool disabled = false;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetBoolean(1, &disabled));

  const Extension* extension = extension_service_->GetExtensionById(
      extension_id, true);
  if (!extension)
    return;
  extension_service_->SetAppNotificationDisabled(extension_id, disabled);
}

void AppLauncherHandler::OnFaviconForApp(
    FaviconService::Handle handle,
    const history::FaviconImageResult& image_result) {
  scoped_ptr<AppInstallInfo> install_info(
      favicon_consumer_.GetClientDataForCurrentRequest());
  scoped_ptr<WebApplicationInfo> web_app(new WebApplicationInfo());
  web_app->is_bookmark_app = install_info->is_bookmark_app;
  web_app->title = install_info->title;
  web_app->app_url = install_info->app_url;
  web_app->urls.push_back(install_info->app_url);

  if (!image_result.image.IsEmpty()) {
    WebApplicationInfo::IconInfo icon;
    icon.data = image_result.image.AsBitmap();
    icon.width = icon.data.width();
    icon.height = icon.data.height();
    web_app->icons.push_back(icon);
  }

  scoped_refptr<CrxInstaller> installer(
      CrxInstaller::Create(extension_service_, NULL));
  installer->set_error_on_unsupported_requirements(true);
  installer->set_page_ordinal(install_info->page_ordinal);
  installer->InstallWebApp(*web_app);
  attempted_bookmark_app_install_ = true;
}

void AppLauncherHandler::SetAppToBeHighlighted() {
  if (highlight_app_id_.empty())
    return;

  StringValue app_id(highlight_app_id_);
  web_ui()->CallJavascriptFunction("ntp.setAppToBeHighlighted", app_id);
  highlight_app_id_.clear();
}

// static
void AppLauncherHandler::RegisterUserPrefs(PrefServiceBase* pref_service) {
  pref_service->RegisterListPref(prefs::kNtpAppPageNames,
                                 PrefService::SYNCABLE_PREF);
}

void AppLauncherHandler::CleanupAfterUninstall() {
  extension_id_prompting_.clear();
}

// static
void AppLauncherHandler::RecordAppLaunchType(
    extension_misc::AppLaunchBucket bucket) {
  UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram, bucket,
                            extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
}

// static
void AppLauncherHandler::RecordWebStoreLaunch() {
  RecordAppLaunchType(extension_misc::APP_LAUNCH_NTP_WEBSTORE);
}

// static
void AppLauncherHandler::RecordAppLaunchByID(
    extension_misc::AppLaunchBucket bucket) {
  CHECK(bucket != extension_misc::APP_LAUNCH_BUCKET_INVALID);

  RecordAppLaunchType(bucket);
}

// static
void AppLauncherHandler::RecordAppLaunchByUrl(
    Profile* profile,
    std::string escaped_url,
    extension_misc::AppLaunchBucket bucket) {
  CHECK(bucket != extension_misc::APP_LAUNCH_BUCKET_INVALID);

  GURL url(net::UnescapeURLComponent(escaped_url, kUnescapeRules));
  DCHECK(profile->GetExtensionService());
  if (!profile->GetExtensionService()->IsInstalledApp(url))
    return;

  RecordAppLaunchType(bucket);
}

void AppLauncherHandler::PromptToEnableApp(const std::string& extension_id) {
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  if (!extension) {
    extension = extension_service_->GetTerminatedExtension(extension_id);
    // It's possible (though unlikely) the app could have been uninstalled since
    // the user clicked on it.
    if (!extension)
      return;
    // If the app was terminated, reload it first. (This reallocates the
    // Extension object.)
    extension_service_->ReloadExtension(extension_id);
    extension = extension_service_->GetExtensionById(extension_id, true);
  }

  ExtensionPrefs* extension_prefs = extension_service_->extension_prefs();
  if (!extension_prefs->DidExtensionEscalatePermissions(extension_id)) {
    // Enable the extension immediately if its privileges weren't escalated.
    // This is a no-op if the extension was previously terminated.
    extension_service_->EnableExtension(extension_id);

    // Launch app asynchronously so the image will update.
    StringValue app_id(extension_id);
    web_ui()->CallJavascriptFunction("ntp.launchAppAfterEnable", app_id);
    return;
  }

  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;
  GetExtensionInstallPrompt()->ConfirmReEnable(this, extension);
}

void AppLauncherHandler::ExtensionUninstallAccepted() {
  // Do the uninstall work here.
  DCHECK(!extension_id_prompting_.empty());

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id_prompting_, true);
  if (!extension)
    return;

  extension_service_->UninstallExtension(extension_id_prompting_,
                                         false /* external_uninstall */, NULL);
  CleanupAfterUninstall();
}

void AppLauncherHandler::ExtensionUninstallCanceled() {
  CleanupAfterUninstall();
}

void AppLauncherHandler::InstallUIProceed() {
  // Do the re-enable work here.
  DCHECK(!extension_id_prompting_.empty());

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id_prompting_, true);
  if (!extension)
    return;

  extension_service_->GrantPermissionsAndEnableExtension(
      extension, extension_install_ui_->record_oauth2_grant());

  // We bounce this off the NTP so the browser can update the apps icon.
  // If we don't launch the app asynchronously, then the app's disabled
  // icon disappears but isn't replaced by the enabled icon, making a poor
  // visual experience.
  StringValue app_id(extension->id());
  web_ui()->CallJavascriptFunction("ntp.launchAppAfterEnable", app_id);

  extension_id_prompting_ = "";
}

void AppLauncherHandler::InstallUIAbort(bool user_initiated) {
  // We record the histograms here because ExtensionUninstallCanceled is also
  // called when the extension uninstall dialog is canceled.
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id_prompting_, true);
  std::string histogram_name = user_initiated ?
      "Extensions.Permissions_ReEnableCancel" :
      "Extensions.Permissions_ReEnableAbort";
  ExtensionService::RecordPermissionMessagesHistogram(
      extension, histogram_name.c_str());

  CleanupAfterUninstall();
}

ExtensionUninstallDialog* AppLauncherHandler::GetExtensionUninstallDialog() {
  if (!extension_uninstall_dialog_.get()) {
    Browser* browser = browser::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    extension_uninstall_dialog_.reset(
        ExtensionUninstallDialog::Create(browser, this));
  }
  return extension_uninstall_dialog_.get();
}

ExtensionInstallPrompt* AppLauncherHandler::GetExtensionInstallPrompt() {
  if (!extension_install_ui_.get()) {
    Browser* browser = browser::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    extension_install_ui_.reset(
        chrome::CreateExtensionInstallPromptWithBrowser(browser));
  }
  return extension_install_ui_.get();
}
