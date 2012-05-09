// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/handler_options_handler2.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options2 {

HandlerOptionsHandler::HandlerOptionsHandler() {
}

HandlerOptionsHandler::~HandlerOptionsHandler() {
}

void HandlerOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      { "handlers_tab_label", IDS_HANDLERS_TAB_LABEL },
      { "handlers_allow", IDS_HANDLERS_ALLOW_RADIO },
      { "handlers_block", IDS_HANDLERS_DONOTALLOW_RADIO },
      { "handlers_type_column_header", IDS_HANDLERS_TYPE_COLUMN_HEADER },
      { "handlers_site_column_header", IDS_HANDLERS_SITE_COLUMN_HEADER },
      { "handlers_remove_link", IDS_HANDLERS_REMOVE_HANDLER_LINK },
      { "handlers_none_handler", IDS_HANDLERS_NONE_HANDLER },
      { "handlers_active_heading", IDS_HANDLERS_ACTIVE_HEADING },
      { "handlers_ignored_heading", IDS_HANDLERS_IGNORED_HEADING },
  };
  RegisterTitle(localized_strings, "handlersPage",
                IDS_HANDLER_OPTIONS_WINDOW_TITLE);
  RegisterStrings(localized_strings, resources, arraysize(resources));
}

void HandlerOptionsHandler::InitializeHandler() {
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
      content::Source<Profile>(Profile::FromWebUI(web_ui())));
}

void HandlerOptionsHandler::InitializePage() {
  UpdateHandlerList();
}

void HandlerOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("clearDefault",
      base::Bind(&HandlerOptionsHandler::ClearDefault,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeHandler",
      base::Bind(&HandlerOptionsHandler::RemoveHandler,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setHandlersEnabled",
      base::Bind(&HandlerOptionsHandler::SetHandlersEnabled,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setDefault",
      base::Bind(&HandlerOptionsHandler::SetDefault,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeIgnoredHandler",
      base::Bind(&HandlerOptionsHandler::RemoveIgnoredHandler,
                 base::Unretained(this)));
}

ProtocolHandlerRegistry* HandlerOptionsHandler::GetProtocolHandlerRegistry() {
  return Profile::FromWebUI(web_ui())->GetProtocolHandlerRegistry();
}

static void GetHandlersAsListValue(
    const ProtocolHandlerRegistry::ProtocolHandlerList& handlers,
    ListValue* handler_list) {
  ProtocolHandlerRegistry::ProtocolHandlerList::const_iterator handler;
  for (handler = handlers.begin(); handler != handlers.end(); ++handler) {
    ListValue* handlerValue = new ListValue();
    handlerValue->Append(Value::CreateStringValue(handler->protocol()));
    handlerValue->Append(Value::CreateStringValue(handler->url().spec()));
    handlerValue->Append(Value::CreateStringValue(handler->title()));
    handler_list->Append(handlerValue);
  }
}

void HandlerOptionsHandler::GetHandlersForProtocol(
    const std::string& protocol,
    DictionaryValue* handlers_value) {
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  handlers_value->SetString("protocol", protocol);
  handlers_value->SetInteger("default_handler",
      registry->GetHandlerIndex(protocol));

  ListValue* handlers_list = new ListValue();
  GetHandlersAsListValue(registry->GetHandlersFor(protocol), handlers_list);
  handlers_value->Set("handlers", handlers_list);
}

void HandlerOptionsHandler::GetIgnoredHandlers(ListValue* handlers) {
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  ProtocolHandlerRegistry::ProtocolHandlerList ignored_handlers =
      registry->GetIgnoredHandlers();
  return GetHandlersAsListValue(ignored_handlers, handlers);
}

void HandlerOptionsHandler::UpdateHandlerList() {
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  std::vector<std::string> protocols;
  registry->GetRegisteredProtocols(&protocols);

  ListValue handlers;
  for (std::vector<std::string>::iterator protocol = protocols.begin();
       protocol != protocols.end(); protocol++) {
    DictionaryValue* handler_value = new DictionaryValue();
    GetHandlersForProtocol(*protocol, handler_value);
    handlers.Append(handler_value);
  }

  scoped_ptr<ListValue> ignored_handlers(new ListValue());
  GetIgnoredHandlers(ignored_handlers.get());
  web_ui()->CallJavascriptFunction("HandlerOptions.setHandlers", handlers);
  web_ui()->CallJavascriptFunction("HandlerOptions.setIgnoredHandlers",
                                   *ignored_handlers);
}

void HandlerOptionsHandler::RemoveHandler(const ListValue* args) {
  ListValue* list;
  if (!args->GetList(0, &list)) {
    NOTREACHED();
    return;
  }

  ProtocolHandler handler(ParseHandlerFromArgs(list));
  GetProtocolHandlerRegistry()->RemoveHandler(handler);

  // No need to call UpdateHandlerList() - we should receive a notification
  // that the ProtocolHandlerRegistry has changed and we will update the view
  // then.
}

void HandlerOptionsHandler::RemoveIgnoredHandler(const ListValue* args) {
  ListValue* list;
  if (!args->GetList(0, &list)) {
    NOTREACHED();
    return;
  }

  ProtocolHandler handler(ParseHandlerFromArgs(list));
  GetProtocolHandlerRegistry()->RemoveIgnoredHandler(handler);
}

void HandlerOptionsHandler::SetHandlersEnabled(const ListValue* args) {
  bool enabled = true;
  CHECK(args->GetBoolean(0, &enabled));
  if (enabled)
    GetProtocolHandlerRegistry()->Enable();
  else
    GetProtocolHandlerRegistry()->Disable();
}

void HandlerOptionsHandler::ClearDefault(const ListValue* args) {
  Value* value;
  CHECK(args->Get(0, &value));
  std::string protocol_to_clear;
  CHECK(value->GetAsString(&protocol_to_clear));
  GetProtocolHandlerRegistry()->ClearDefault(protocol_to_clear);
}

void HandlerOptionsHandler::SetDefault(const ListValue* args) {
  Value* value;
  CHECK(args->Get(0, &value));
  ListValue* list;
  CHECK(args->GetList(0, &list));
  const ProtocolHandler& handler(ParseHandlerFromArgs(list));
  CHECK(!handler.IsEmpty());
  GetProtocolHandlerRegistry()->OnAcceptRegisterProtocolHandler(handler);
}

ProtocolHandler HandlerOptionsHandler::ParseHandlerFromArgs(
    const ListValue* args) const {
  string16 protocol;
  string16 url;
  string16 title;
  bool ok = args->GetString(0, &protocol) && args->GetString(1, &url) &&
    args->GetString(2, &title);
  if (!ok)
    return ProtocolHandler::EmptyProtocolHandler();
  return ProtocolHandler::CreateProtocolHandler(UTF16ToUTF8(protocol),
                                                GURL(UTF16ToUTF8(url)),
                                                title);
}

void HandlerOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED)
    UpdateHandlerList();
  else
    NOTREACHED();
}

}  // namespace options2
