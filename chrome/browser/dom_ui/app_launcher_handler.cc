// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/app_launcher_handler.h"

#include "app/resource_bundle.h"
#include "base/base64.h"
#include "base/values.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"

namespace {

bool TreatAsApp(const Extension* extension) {
  return !extension->GetFullLaunchURL().is_empty();
}

}  // namespace

AppLauncherHandler::AppLauncherHandler(
    ExtensionsService* extension_service)
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
}

// static
void AppLauncherHandler::CreateAppInfo(Extension* extension,
                                          DictionaryValue* value) {
  value->Clear();
  value->SetString(L"id", extension->id());
  value->SetString(L"name", extension->name());
  value->SetString(L"description", extension->description());
  value->SetString(L"launch_url", extension->GetFullLaunchURL().spec());

  // TODO(arv): Get the icon from the  extension
  std::string file_contents =
      ResourceBundle::GetSharedInstance().GetDataResource(
          IDR_EXTENSION_DEFAULT_ICON);
  std::string base64_encoded;
  base::Base64Encode(file_contents, &base64_encoded);
  GURL icon_url("data:image/png;base64," + base64_encoded);
  value->SetString(L"icon", icon_url.spec());
}

void AppLauncherHandler::HandleGetApps(const Value* value) {
  ListValue list;
  const ExtensionList* extensions = extensions_service_->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
     if (TreatAsApp(*it)) {
       DictionaryValue* app_info = new DictionaryValue();
       CreateAppInfo(*it, app_info);
       list.Append(app_info);
     }
  }

  dom_ui_->CallJavascriptFunction(L"getAppsCallback", list);
}

void AppLauncherHandler::HandleLaunchApp(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  std::string url;
  const ListValue* list = static_cast<const ListValue*>(value);
  if (list->GetSize() == 0 || !list->GetString(0, &url)) {
    NOTREACHED();
    return;
  }

  TabContents* tab_contents = dom_ui_->tab_contents();
  tab_contents->OpenURL(GURL(url), GURL(), NEW_FOREGROUND_TAB,
                        PageTransition::LINK);
}
