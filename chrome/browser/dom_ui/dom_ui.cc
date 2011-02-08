// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui.h"

#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/generic_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

namespace {

std::wstring GetJavascript(const std::wstring& function_name,
                           const std::vector<const Value*>& arg_list) {
  std::wstring parameters;
  std::string json;
  for (size_t i = 0; i < arg_list.size(); ++i) {
    if (i > 0)
      parameters += L",";

    base::JSONWriter::Write(arg_list[i], false, &json);
    parameters += UTF8ToWide(json);
  }
  return function_name + L"(" + parameters + L");";
}

}  // namespace

DOMUI::DOMUI(TabContents* contents)
    : hide_favicon_(false),
      force_bookmark_bar_visible_(false),
      focus_location_bar_by_default_(false),
      should_hide_url_(false),
      link_transition_type_(PageTransition::LINK),
      bindings_(BindingsPolicy::DOM_UI),
      register_callback_overwrites_(false),
      tab_contents_(contents) {
  GenericHandler* handler = new GenericHandler();
  AddMessageHandler(handler->Attach(this));
}

DOMUI::~DOMUI() {
  STLDeleteContainerPairSecondPointers(message_callbacks_.begin(),
                                       message_callbacks_.end());
  STLDeleteContainerPointers(handlers_.begin(), handlers_.end());
}

// DOMUI, public: -------------------------------------------------------------

void DOMUI::ProcessDOMUIMessage(const ViewHostMsg_DomMessage_Params& params) {
  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(params.name);
  if (callback == message_callbacks_.end())
    return;

  // Forward this message and content on.
  callback->second->Run(&params.arguments);
}

void DOMUI::CallJavascriptFunction(const std::wstring& function_name) {
  std::wstring javascript = function_name + L"();";
  ExecuteJavascript(javascript);
}

void DOMUI::CallJavascriptFunction(const std::wstring& function_name,
                                   const Value& arg) {
  std::vector<const Value*> args;
  args.push_back(&arg);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void DOMUI::CallJavascriptFunction(
    const std::wstring& function_name,
    const Value& arg1, const Value& arg2) {
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void DOMUI::CallJavascriptFunction(
    const std::wstring& function_name,
    const Value& arg1, const Value& arg2, const Value& arg3) {
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void DOMUI::CallJavascriptFunction(
    const std::wstring& function_name,
    const Value& arg1,
    const Value& arg2,
    const Value& arg3,
    const Value& arg4) {
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  args.push_back(&arg4);
  ExecuteJavascript(GetJavascript(function_name, args));
}

void DOMUI::CallJavascriptFunction(
    const std::wstring& function_name,
    const std::vector<const Value*>& args) {
  ExecuteJavascript(GetJavascript(function_name, args));
}

ui::ThemeProvider* DOMUI::GetThemeProvider() const {
  return GetProfile()->GetThemeProvider();
}

void DOMUI::RegisterMessageCallback(const std::string &message,
                                    MessageCallback *callback) {
  std::pair<MessageCallbackMap::iterator, bool> result =
      message_callbacks_.insert(std::make_pair(message, callback));

  // Overwrite preexisting message callback mappings.
  if (register_callback_overwrites() && !result.second)
    result.first->second = callback;
}

Profile* DOMUI::GetProfile() const {
  DCHECK(tab_contents());
  return tab_contents()->profile();
}

RenderViewHost* DOMUI::GetRenderViewHost() const {
  DCHECK(tab_contents());
  return tab_contents()->render_view_host();
}

// DOMUI, protected: ----------------------------------------------------------

void DOMUI::AddMessageHandler(DOMMessageHandler* handler) {
  handlers_.push_back(handler);
}

void DOMUI::ExecuteJavascript(const std::wstring& javascript) {
  GetRenderViewHost()->ExecuteJavascriptInWebFrame(string16(),
                                                   WideToUTF16Hack(javascript));
}

///////////////////////////////////////////////////////////////////////////////
// DOMMessageHandler

DOMMessageHandler* DOMMessageHandler::Attach(DOMUI* dom_ui) {
  dom_ui_ = dom_ui;
  RegisterMessages();
  return this;
}

// DOMMessageHandler, protected: ----------------------------------------------

void DOMMessageHandler::SetURLAndTitle(DictionaryValue* dictionary,
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

bool DOMMessageHandler::ExtractIntegerValue(const ListValue* value,
                                            int* out_int) {
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToInt(string_value, out_int);
  NOTREACHED();
  return false;
}

// TODO(viettrungluu): convert to string16 (or UTF-8 std::string?).
std::wstring DOMMessageHandler::ExtractStringValue(const ListValue* value) {
  string16 string16_value;
  if (value->GetString(0, &string16_value))
    return UTF16ToWideHack(string16_value);
  NOTREACHED();
  return std::wstring();
}
