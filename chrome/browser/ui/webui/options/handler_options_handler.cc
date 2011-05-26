// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/handler_options_handler.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"


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
  };
  RegisterTitle(localized_strings, "handlersPage",
                IDS_HANDLER_OPTIONS_WINDOW_TITLE);
  RegisterStrings(localized_strings, resources, arraysize(resources));
}

void HandlerOptionsHandler::Initialize() {
  UpdateHandlerList();
  notification_registrar_.Add(
      this, NotificationType::PROTOCOL_HANDLER_REGISTRY_CHANGED,
      NotificationService::AllSources());
}

void HandlerOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("clearDefault",
      NewCallback(this, &HandlerOptionsHandler::ClearDefault));
  web_ui_->RegisterMessageCallback("removeHandler",
      NewCallback(this, &HandlerOptionsHandler::RemoveHandler));
  web_ui_->RegisterMessageCallback("setHandlersEnabled",
      NewCallback(this, &HandlerOptionsHandler::SetHandlersEnabled));
  web_ui_->RegisterMessageCallback("setDefault",
      NewCallback(this, &HandlerOptionsHandler::SetDefault));
}

ProtocolHandlerRegistry* HandlerOptionsHandler::GetProtocolHandlerRegistry() {
  DCHECK(web_ui_);
  return web_ui_->GetProfile()->GetProtocolHandlerRegistry();
}

DictionaryValue* HandlerOptionsHandler::GetHandlersForProtocol(
    const std::string& protocol) {
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  DictionaryValue* handlers_value = new DictionaryValue();
  handlers_value->SetString("protocol", protocol);
  handlers_value->SetInteger("default_handler",
      registry->GetHandlerIndex(protocol));

  ListValue* handler_list = new ListValue();
  const ProtocolHandlerRegistry::ProtocolHandlerList* handlers =
      registry->GetHandlersFor(protocol);
  ProtocolHandlerRegistry::ProtocolHandlerList::const_iterator handler;
  for (handler = handlers->begin(); handler != handlers->end(); ++handler) {
    ListValue* handlerValue = new ListValue();
    handlerValue->Append(Value::CreateStringValue(handler->url().spec()));
    handlerValue->Append(Value::CreateStringValue(handler->title()));
    handler_list->Append(handlerValue);
  }
  handlers_value->Set("handlers", handler_list);
  return handlers_value;
}

void HandlerOptionsHandler::UpdateHandlerList() {
#if defined(ENABLE_REGISTER_PROTOCOL_HANDLER)
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  std::vector<std::string> protocols;
  registry->GetHandledProtocols(&protocols);

  ListValue handlers;
  for (std::vector<std::string>::iterator protocol = protocols.begin();
       protocol != protocols.end(); protocol++) {
    handlers.Append(GetHandlersForProtocol(*protocol));
  }

  web_ui_->CallJavascriptFunction("HandlerOptions.setHandlers", handlers);
#endif // defined(ENABLE_REGISTER_PROTOCOL_HANDLER)
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
  GetProtocolHandlerRegistry()->SetDefault(handler);
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

void HandlerOptionsHandler::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  if (type == NotificationType::PROTOCOL_HANDLER_REGISTRY_CHANGED)
    UpdateHandlerList();
  else
    NOTREACHED();
}
