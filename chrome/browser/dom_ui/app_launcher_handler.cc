// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/app_launcher_handler.h"

#include "app/animation.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/app_launched_animation.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
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

std::string GetIconURL(Extension* extension, Extension::Icons icon,
                       const std::string& default_val) {
  GURL url = extension->GetIconURL(icon);
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
  dom_ui_->RegisterMessageCallback("uninstallApp",
      NewCallback(this, &AppLauncherHandler::HandleUninstallApp));
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

    default:
      NOTREACHED();
  }
}

// static
void AppLauncherHandler::CreateAppInfo(Extension* extension,
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
}

void AppLauncherHandler::HandleGetApps(const ListValue* args) {
  bool show_debug_link = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAppsDebug);

  DictionaryValue dictionary;
  dictionary.SetBoolean("showDebugLink", show_debug_link);

  ListValue* list = new ListValue();
  const ExtensionList* extensions = extensions_service_->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    // Don't include the WebStore component app. The WebStore launcher
    // gets special treatment in ntp/apps.js.
    if ((*it)->is_app() && (*it)->id() != extension_misc::kWebStoreAppId) {
      DictionaryValue* app_info = new DictionaryValue();
      CreateAppInfo(*it, app_info);
      list->Append(app_info);
    }
  }

  dictionary.Set("apps", list);
  dom_ui_->CallJavascriptFunction(L"getAppsCallback", dictionary);

  // First time we get here we set up the observer so that we can tell update
  // the apps as they change.
  if (registrar_.IsEmpty()) {
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
        NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
        NotificationService::AllSources());
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

  Extension* extension =
      extensions_service_->GetExtensionById(extension_id, false);
  DCHECK(extension);
  Profile* profile = extensions_service_->profile();
  Extension::LaunchContainer container = extension->launch_container();

  // To give a more "launchy" experience when using the NTP launcher, we close
  // it automatically.
  Browser* browser = BrowserList::GetLastActive();
  TabContents* old_contents = NULL;
  if (browser)
    old_contents = browser->GetSelectedTabContents();

  AnimateAppIcon(extension, rect);
  Browser::OpenApplication(profile, extension, container);

  if (old_contents &&
      old_contents->GetURL().GetOrigin() ==
          GURL(chrome::kChromeUINewTabURL).GetOrigin()) {
    browser->CloseTabContents(old_contents);
  }
}

void AppLauncherHandler::AnimateAppIcon(Extension* extension,
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

  // Make sure that the extension exists.
  Extension* extension =
      extensions_service_->GetExtensionById(extension_id, false);
  DCHECK(extension);

  extensions_service_->UninstallExtension(extension_id, false);
}
