// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui.h"

#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/webui/generic_handler.h"
#include "content/common/view_messages.h"
#include "content/public/common/bindings_policy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

// static
string16 WebUI::GetJavascriptCall(
    const std::string& function_name,
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

WebUI::WebUI(TabContents* contents)
    : hide_favicon_(false),
      focus_location_bar_by_default_(false),
      should_hide_url_(false),
      link_transition_type_(content::PAGE_TRANSITION_LINK),
      bindings_(content::BINDINGS_POLICY_WEB_UI),
      register_callback_overwrites_(false),
      tab_contents_(contents) {
  DCHECK(contents);
  GenericHandler* handler = new GenericHandler();
  AddMessageHandler(handler->Attach(this));
}

WebUI::~WebUI() {
  STLDeleteContainerPointers(handlers_.begin(), handlers_.end());
}

// WebUI, public: -------------------------------------------------------------

const WebUI::TypeID WebUI::kNoWebUI = NULL;

bool WebUI::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebUI, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WebUISend, OnWebUISend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebUI::OnWebUISend(const GURL& source_url,
                        const std::string& message,
                        const ListValue& args) {
  if (!ChildProcessSecurityPolicy::GetInstance()->
          HasWebUIBindings(tab_contents_->GetRenderProcessHost()->GetID())) {
    NOTREACHED() << "Blocked unauthorized use of WebUIBindings.";
    return;
  }

  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(message);
  if (callback != message_callbacks_.end()) {
    // Forward this message and content on.
    callback->second.Run(&args);
  }
}

void WebUI::RenderViewCreated(RenderViewHost* render_view_host) {
  // Do not attempt to set the toolkit property if WebUI is not enabled, e.g.,
  // the bookmarks manager page.
  if (!(bindings_ & content::BINDINGS_POLICY_WEB_UI))
    return;

#if defined(TOOLKIT_VIEWS)
  render_view_host->SetWebUIProperty("toolkit", "views");
#elif defined(TOOLKIT_GTK)
  render_view_host->SetWebUIProperty("toolkit", "GTK");
#endif  // defined(TOOLKIT_VIEWS)
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
  ExecuteJavascript(WebUI::GetJavascriptCall(function_name, args));
}

void WebUI::CallJavascriptFunction(
    const std::string& function_name,
    const Value& arg1, const Value& arg2) {
  DCHECK(IsStringASCII(function_name));
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  ExecuteJavascript(WebUI::GetJavascriptCall(function_name, args));
}

void WebUI::CallJavascriptFunction(
    const std::string& function_name,
    const Value& arg1, const Value& arg2, const Value& arg3) {
  DCHECK(IsStringASCII(function_name));
  std::vector<const Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  ExecuteJavascript(WebUI::GetJavascriptCall(function_name, args));
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
  ExecuteJavascript(WebUI::GetJavascriptCall(function_name, args));
}

void WebUI::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const Value*>& args) {
  DCHECK(IsStringASCII(function_name));
  ExecuteJavascript(WebUI::GetJavascriptCall(function_name, args));
}

void WebUI::RegisterMessageCallback(const std::string &message,
                                    const MessageCallback& callback) {
  std::pair<MessageCallbackMap::iterator, bool> result =
      message_callbacks_.insert(std::make_pair(message, callback));

  // Overwrite preexisting message callback mappings.
  if (!result.second && register_callback_overwrites())
    result.first->second = callback;
}

// WebUI, protected: ----------------------------------------------------------

void WebUI::AddMessageHandler(WebUIMessageHandler* handler) {
  handlers_.push_back(handler);
}

void WebUI::ExecuteJavascript(const string16& javascript) {
  tab_contents_->render_view_host()->ExecuteJavascriptInWebFrame(string16(),
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
  double double_value;
  if (value->GetDouble(0, &double_value)) {
    *out_int = static_cast<int>(double_value);
    return true;
  }
  NOTREACHED();
  return false;
}

bool WebUIMessageHandler::ExtractDoubleValue(const ListValue* value,
                                             double* out_value) {
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToDouble(string_value, out_value);
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
