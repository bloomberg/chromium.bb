// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui.h"

#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/webui/generic_handler.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"

namespace {

string16 GetJavascript(const std::string& function_name,
                       const std::vector<const Value*>& arg_list) {
  string16 parameters;
  std::string json;
  for (size_t i = 0; i < arg_list.size(); ++i) {
    if (i > 0)
      parameters += char16(',');

    base::JSONWriter::Write(arg_list[i], false, &json);
    parameters += UTF8ToUTF16(json);
  }
  return ASCIIToUTF16(function_name) +
      char16('(') + parameters + char16(')') + char16(';');
}

}  // namespace

WebUI::WebUI(TabContents* contents)
    : hide_favicon_(false),
      force_bookmark_bar_visible_(false),
      focus_location_bar_by_default_(false),
      should_hide_url_(false),
      link_transition_type_(PageTransition::LINK),
      bindings_(BindingsPolicy::WEB_UI),
      register_callback_overwrites_(false),
      tab_contents_(contents) {
  GenericHandler* handler = new GenericHandler();
  AddMessageHandler(handler->Attach(this));
}

WebUI::~WebUI() {
  STLDeleteContainerPairSecondPointers(message_callbacks_.begin(),
                                       message_callbacks_.end());
  STLDeleteContainerPointers(handlers_.begin(), handlers_.end());
}

// WebUI, public: -------------------------------------------------------------

void WebUI::ProcessWebUIMessage(const ViewHostMsg_DomMessage_Params& params) {
  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(params.name);
  if (callback == message_callbacks_.end())
    return;

  // Forward this message and content on.
  callback->second->Run(&params.arguments);
}

void WebUI::CallJavascriptFunction(const std::string& function_name) {
  DCHECK(IsStringASCII(function_name));
  string16 javascript = ASCIIToUTF16(function_name + "();");
  ExecuteJavascript(javascript);
}

void WebUI::CallJavascriptFunction(const std::string& function_name,
                                   const Value& arg) {
  DCHECK(IsStringASCII(function_name));
  std::vector<const Value*> args;
  args.push_back(&arg);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void WebUI::CallJavascriptFunction(
    const std::string& function_name,
    const Value& arg1, const Value& arg2) {
  DCHECK(IsStringASCII(function_name));
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void WebUI::CallJavascriptFunction(
    const std::string& function_name,
    const Value& arg1, const Value& arg2, const Value& arg3) {
  DCHECK(IsStringASCII(function_name));
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void WebUI::CallJavascriptFunction(
    const std::string& function_name,
    const Value& arg1,
    const Value& arg2,
    const Value& arg3,
    const Value& arg4) {
  DCHECK(IsStringASCII(function_name));
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  args.push_back(&arg4);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void WebUI::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const Value*>& args) {
  DCHECK(IsStringASCII(function_name));
  ExecuteJavascript(GetJavascript(function_name, args));
}

ui::ThemeProvider* WebUI::GetThemeProvider() const {
  return GetProfile()->GetThemeProvider();
}

void WebUI::RegisterMessageCallback(const std::string &message,
                                    MessageCallback *callback) {
  std::pair<MessageCallbackMap::iterator, bool> result =
      message_callbacks_.insert(std::make_pair(message, callback));

  // Overwrite preexisting message callback mappings.
  if (register_callback_overwrites() && !result.second)
    result.first->second = callback;
}

Profile* WebUI::GetProfile() const {
  DCHECK(tab_contents());
  return tab_contents()->profile();
}

RenderViewHost* WebUI::GetRenderViewHost() const {
  DCHECK(tab_contents());
  return tab_contents()->render_view_host();
}

// WebUI, protected: ----------------------------------------------------------

void WebUI::AddMessageHandler(WebUIMessageHandler* handler) {
  handlers_.push_back(handler);
}

void WebUI::ExecuteJavascript(const string16& javascript) {
  GetRenderViewHost()->ExecuteJavascriptInWebFrame(string16(),
                                                   javascript);
}

///////////////////////////////////////////////////////////////////////////////
// WebUIMessageHandler
WebUIMessageHandler::WebUIMessageHandler() : web_ui_(NULL) {
}

WebUIMessageHandler::~WebUIMessageHandler() {
}

WebUIMessageHandler* WebUIMessageHandler::Attach(WebUI* web_ui) {
  web_ui_ = web_ui;
  RegisterMessages();
  return this;
}

// WebUIMessageHandler, protected: ---------------------------------------------

void WebUIMessageHandler::SetURLAndTitle(DictionaryValue* dictionary,
                                         string16 title,
                                         const GURL& gurl) {
  dictionary->SetString("url", gurl.spec());

  bool using_url_as_the_title = false;
  if (title.empty()) {
    using_url_as_the_title = true;
    title = UTF8ToUTF16(gurl.spec());
  }

  // Since the title can contain BiDi text, we need to mark the text as either
  // RTL or LTR, depending on the characters in the string. If we use the URL
  // as the title, we mark the title as LTR since URLs are always treated as
  // left to right strings.
  string16 title_to_set(title);
  if (base::i18n::IsRTL()) {
    if (using_url_as_the_title) {
      base::i18n::WrapStringWithLTRFormatting(&title_to_set);
    } else {
      base::i18n::AdjustStringForLocaleDirection(&title_to_set);
    }
  }
  dictionary->SetString("title", title_to_set);
}

bool WebUIMessageHandler::ExtractIntegerValue(const ListValue* value,
                                              int* out_int) {
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToInt(string_value, out_int);
  NOTREACHED();
  return false;
}

string16 WebUIMessageHandler::ExtractStringValue(const ListValue* value) {
  string16 string16_value;
  if (value->GetString(0, &string16_value))
    return string16_value;
  NOTREACHED();
  return string16();
}
