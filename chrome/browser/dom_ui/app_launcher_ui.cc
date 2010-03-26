// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/app_launcher_ui.h"

#include "app/resource_bundle.h"
#include "base/base64.h"
#include "base/message_loop.h"
#include "base/ref_counted_memory.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"

namespace {

bool TreatAsApp(const Extension* extension) {
  return !extension->GetFullLaunchURL().is_empty();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// AppLauncherUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

AppLauncherUIHTMLSource::AppLauncherUIHTMLSource()
    : DataSource(chrome::kChromeUIAppsHost, MessageLoop::current()) {
}

void AppLauncherUIHTMLSource::StartDataRequest(const std::string& path,
    bool is_off_the_record, int request_id) {
  DictionaryValue localized_strings;
  // TODO(arv): What strings do we need?
  localized_strings.SetString(L"title", L"App Launcher");
  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece app_launcher_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_APP_LAUNCHER_HTML));
  std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      app_launcher_html, &localized_strings);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string AppLauncherUIHTMLSource::GetMimeType(
    const std::string& path) const {
  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// AppLauncherHandler
//
////////////////////////////////////////////////////////////////////////////////

AppLauncherDOMHandler::AppLauncherDOMHandler(
    ExtensionsService* extension_service)
    : extensions_service_(extension_service) {
}

AppLauncherDOMHandler::~AppLauncherDOMHandler() {}

DOMMessageHandler* AppLauncherDOMHandler::Attach(DOMUI* dom_ui) {
  // TODO(arv): Add initialization code to the Apps store etc.
  return DOMMessageHandler::Attach(dom_ui);
}

void AppLauncherDOMHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getAll",
      NewCallback(this, &AppLauncherDOMHandler::HandleGetAll));
  dom_ui_->RegisterMessageCallback("launch",
      NewCallback(this, &AppLauncherDOMHandler::HandleLaunch));
}

// static
void AppLauncherDOMHandler::CreateAppInfo(Extension* extension,
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

void AppLauncherDOMHandler::HandleGetAll(const Value* value) {
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

  dom_ui_->CallJavascriptFunction(L"getAllCallback", list);
}

void AppLauncherDOMHandler::HandleLaunch(const Value* value) {
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
////////////////////////////////////////////////////////////////////////////////
//
// AppLauncherUI
//
////////////////////////////////////////////////////////////////////////////////

AppLauncherUI::AppLauncherUI(TabContents* contents) : DOMUI(contents) {
  ExtensionsService *extension_service =
      GetProfile()->GetOriginalProfile()->GetExtensionsService();

  AppLauncherDOMHandler* handler = new AppLauncherDOMHandler(extension_service);
  AddMessageHandler(handler);
  handler->Attach(this);

  AppLauncherUIHTMLSource* html_source = new AppLauncherUIHTMLSource();

  // Set up the chrome://bookmarks/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
