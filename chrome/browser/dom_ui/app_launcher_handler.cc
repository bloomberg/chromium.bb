// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/app_launcher_handler.h"

#include "app/animation.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/app_launched_animation.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/default_apps.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "gfx/rect.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

// This extracts an int from a ListValue at the given |index|.
bool ExtractInt(const ListValue* list, size_t index, int* out_int) {
  std::string string_value;

  if (list->GetString(index, &string_value)) {
    base::StringToInt(string_value, out_int);
    return true;
  }

  return false;
}

std::string GetIconURL(const Extension* extension, Extension::Icons icon,
                       const std::string& default_val) {
  GURL url = extension->GetIconURL(icon, ExtensionIconSet::MATCH_EXACTLY);
  if (!url.is_empty())
    return url.spec();
  else
    return default_val;
}

}  // namespace

AppLauncherHandler::AppLauncherHandler(ExtensionsService* extension_service)
    : extensions_service_(extension_service) {
}

AppLauncherHandler::~AppLauncherHandler() {}

DOMMessageHandler* AppLauncherHandler::Attach(DOMUI* dom_ui) {
  // TODO(arv): Add initialization code to the Apps store etc.
  return DOMMessageHandler::Attach(dom_ui);
}

void AppLauncherHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getApps",
      NewCallback(this, &AppLauncherHandler::HandleGetApps));
  dom_ui_->RegisterMessageCallback("launchApp",
      NewCallback(this, &AppLauncherHandler::HandleLaunchApp));
  dom_ui_->RegisterMessageCallback("setLaunchType",
      NewCallback(this, &AppLauncherHandler::HandleSetLaunchType));
  dom_ui_->RegisterMessageCallback("uninstallApp",
      NewCallback(this, &AppLauncherHandler::HandleUninstallApp));
  dom_ui_->RegisterMessageCallback("hideAppsPromo",
      NewCallback(this, &AppLauncherHandler::HandleHideAppsPromo));
}

void AppLauncherHandler::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_UNLOADED:
      if (dom_ui_->tab_contents())
        HandleGetApps(NULL);
      break;
    case NotificationType::PREF_CHANGED: {
      if (!dom_ui_->tab_contents())
        break;

      DictionaryValue dictionary;
      FillAppDictionary(&dictionary);
      dom_ui_->CallJavascriptFunction(L"appsPrefChangeCallback", dictionary);
      break;
    }
    default:
      NOTREACHED();
  }
}

// static
void AppLauncherHandler::CreateAppInfo(const Extension* extension,
                                       ExtensionPrefs* extension_prefs,
                                       DictionaryValue* value) {
  value->Clear();
  value->SetString("id", extension->id());
  value->SetString("name", extension->name());
  value->SetString("description", extension->description());
  value->SetString("launch_url", extension->GetFullLaunchURL().spec());
  value->SetString("options_url", extension->options_url().spec());
  value->SetString("icon_big", GetIconURL(
      extension, Extension::EXTENSION_ICON_LARGE,
      "chrome://theme/IDR_APP_DEFAULT_ICON"));
  value->SetString("icon_small", GetIconURL(
      extension, Extension::EXTENSION_ICON_BITTY,
      std::string("chrome://favicon/") + extension->GetFullLaunchURL().spec()));
  value->SetInteger("launch_container", extension->launch_container());
  value->SetInteger("launch_type",
      extension_prefs->GetLaunchType(extension->id()));

  int app_launch_index = extension_prefs->GetAppLaunchIndex(extension->id());
  if (app_launch_index == -1) {
    // Make sure every app has a launch index (some predate the launch index).
    app_launch_index = extension_prefs->GetNextAppLaunchIndex();
    extension_prefs->SetAppLaunchIndex(extension->id(), app_launch_index);
  }
  value->SetInteger("app_launch_index", app_launch_index);
}

void AppLauncherHandler::FillAppDictionary(DictionaryValue* dictionary) {
  ListValue* list = new ListValue();
  const ExtensionList* extensions = extensions_service_->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    // Don't include the WebStore component app. The WebStore launcher
    // gets special treatment in ntp/apps.js.
    if ((*it)->is_app() && (*it)->id() != extension_misc::kWebStoreAppId) {
      DictionaryValue* app_info = new DictionaryValue();
      CreateAppInfo(*it, extensions_service_->extension_prefs(), app_info);
      list->Append(app_info);
    }
  }
  dictionary->Set("apps", list);

  DefaultApps* default_apps = extensions_service_->default_apps();
  if (default_apps->ShouldShowPromo(extensions_service_->GetAppIds())) {
    dictionary->SetBoolean("showPromo", true);
    default_apps->DidShowPromo();
  } else {
    dictionary->SetBoolean("showPromo", false);
  }

  bool showLauncher =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAppLauncher);
  dictionary->SetBoolean("showLauncher", showLauncher);
}

void AppLauncherHandler::HandleGetApps(const ListValue* args) {
  DictionaryValue dictionary;
  FillAppDictionary(&dictionary);
  dom_ui_->CallJavascriptFunction(L"getAppsCallback", dictionary);

  // First time we get here we set up the observer so that we can tell update
  // the apps as they change.
  if (registrar_.IsEmpty()) {
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
        NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
        NotificationService::AllSources());
  }
  if (pref_change_registrar_.IsEmpty()) {
    pref_change_registrar_.Init(
        extensions_service_->extension_prefs()->pref_service());
    pref_change_registrar_.Add(ExtensionPrefs::kExtensionsPref, this);
  }
}

void AppLauncherHandler::HandleLaunchApp(const ListValue* args) {
  std::string extension_id;
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;

  if (!args->GetString(0, &extension_id) ||
      !ExtractInt(args, 1, &left) ||
      !ExtractInt(args, 2, &top) ||
      !ExtractInt(args, 3, &width) ||
      !ExtractInt(args, 4, &height)) {
    NOTREACHED();
    return;
  }

  // The rect we get from the client is relative to the browser client viewport.
  // Offset the rect by the tab contents bounds.
  gfx::Rect rect(left, top, width, height);
  gfx::Rect tab_contents_bounds;
  dom_ui_->tab_contents()->GetContainerBounds(&tab_contents_bounds);
  rect.Offset(tab_contents_bounds.origin());

  const Extension* extension =
      extensions_service_->GetExtensionById(extension_id, false);
  DCHECK(extension);
  Profile* profile = extensions_service_->profile();

  // To give a more "launchy" experience when using the NTP launcher, we close
  // it automatically.
  Browser* browser = BrowserList::GetLastActive();
  TabContents* old_contents = NULL;
  if (browser)
    old_contents = browser->GetSelectedTabContents();

  AnimateAppIcon(extension, rect);
  TabContents* new_contents = Browser::OpenApplication(
      profile, extension, extension->launch_container(), old_contents);

  if (new_contents != old_contents && browser->tab_count() > 1)
    browser->CloseTabContents(old_contents);
}

void AppLauncherHandler::HandleSetLaunchType(const ListValue* args) {
  std::string extension_id;
  int launch_type;
  if (!args->GetString(0, &extension_id) ||
      !ExtractInt(args, 1, &launch_type)) {
    NOTREACHED();
    return;
  }

  const Extension* extension =
      extensions_service_->GetExtensionById(extension_id, false);
  DCHECK(extension);

  extensions_service_->extension_prefs()->SetLaunchType(
      extension_id,
      static_cast<ExtensionPrefs::LaunchType>(launch_type));
}

void AppLauncherHandler::AnimateAppIcon(const Extension* extension,
                                        const gfx::Rect& rect) {
  // We make this check for the case of minimized windows, unit tests, etc.
  if (platform_util::IsVisible(dom_ui_->tab_contents()->GetNativeView()) &&
      Animation::ShouldRenderRichAnimation()) {
#if defined(OS_WIN)
    AppLaunchedAnimation::Show(extension, rect);
#else
    NOTIMPLEMENTED();
#endif
  }
}

void AppLauncherHandler::HandleUninstallApp(const ListValue* args) {
  std::string extension_id = WideToUTF8(ExtractStringValue(args));
  const Extension* extension = extensions_service_->GetExtensionById(
      extension_id, false);
  if (!extension)
    return;

  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;
  GetExtensionInstallUI()->ConfirmUninstall(this, extension);
}

void AppLauncherHandler::HandleHideAppsPromo(const ListValue* args) {
  // If the user has intentionally hidden the promotion, we'll uninstall all the
  // default apps (we know the user hasn't installed any apps on their own at
  // this point, or the promotion wouldn't have been shown).
  DefaultApps* default_apps = extensions_service_->default_apps();
  const ExtensionIdSet* app_ids = default_apps->GetDefaultApps();
  DCHECK(*app_ids == extensions_service_->GetAppIds());

  for (ExtensionIdSet::const_iterator iter = app_ids->begin();
       iter != app_ids->end(); ++iter) {
    if (extensions_service_->GetExtensionById(*iter, true))
      extensions_service_->UninstallExtension(*iter, false);
  }

  extensions_service_->default_apps()->SetPromoHidden();
}

ExtensionInstallUI* AppLauncherHandler::GetExtensionInstallUI() {
  if (!install_ui_.get())
    install_ui_.reset(new ExtensionInstallUI(dom_ui_->GetProfile()));
  return install_ui_.get();
}

void AppLauncherHandler::InstallUIProceed() {
  DCHECK(!extension_id_prompting_.empty());

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension =
      extensions_service_->GetExtensionById(extension_id_prompting_, true);
  if (!extension)
    return;

  extensions_service_->UninstallExtension(extension_id_prompting_,
                                          false /* external_uninstall */);
  extension_id_prompting_ = "";
}

void AppLauncherHandler::InstallUIAbort() {
  extension_id_prompting_ = "";
}
