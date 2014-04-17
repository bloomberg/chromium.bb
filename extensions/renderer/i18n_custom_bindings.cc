// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/i18n_custom_bindings.h"

#include "base/bind.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/message_bundle.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

I18NCustomBindings::I18NCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetL10nMessage",
      base::Bind(&I18NCustomBindings::GetL10nMessage, base::Unretained(this)));
  RouteFunction("GetL10nUILanguage",
                base::Bind(&I18NCustomBindings::GetL10nUILanguage,
                           base::Unretained(this)));
}

void I18NCustomBindings::GetL10nMessage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 || !args[0]->IsString()) {
    NOTREACHED() << "Bad arguments";
    return;
  }

  std::string extension_id;
  if (args[2]->IsNull() || !args[2]->IsString()) {
    return;
  } else {
    extension_id = *v8::String::Utf8Value(args[2]->ToString());
    if (extension_id.empty())
      return;
  }

  L10nMessagesMap* l10n_messages = GetL10nMessagesMap(extension_id);
  if (!l10n_messages) {
    // Get the current RenderView so that we can send a routed IPC message
    // from the correct source.
    content::RenderView* renderview = context()->GetRenderView();
    if (!renderview)
      return;

    L10nMessagesMap messages;
    // A sync call to load message catalogs for current extension.
    renderview->Send(
        new ExtensionHostMsg_GetMessageBundle(extension_id, &messages));

    // Save messages we got.
    ExtensionToL10nMessagesMap& l10n_messages_map =
        *GetExtensionToL10nMessagesMap();
    l10n_messages_map[extension_id] = messages;

    l10n_messages = GetL10nMessagesMap(extension_id);
  }

  std::string message_name = *v8::String::Utf8Value(args[0]);
  std::string message =
      MessageBundle::GetL10nMessage(message_name, *l10n_messages);

  v8::Isolate* isolate = args.GetIsolate();
  std::vector<std::string> substitutions;
  if (args[1]->IsArray()) {
    // chrome.i18n.getMessage("message_name", ["more", "params"]);
    v8::Local<v8::Array> placeholders = v8::Local<v8::Array>::Cast(args[1]);
    uint32_t count = placeholders->Length();
    if (count > 9)
      return;
    for (uint32_t i = 0; i < count; ++i) {
      substitutions.push_back(
          *v8::String::Utf8Value(
              placeholders->Get(v8::Integer::New(isolate, i))->ToString()));
    }
  } else if (args[1]->IsString()) {
    // chrome.i18n.getMessage("message_name", "one param");
    substitutions.push_back(*v8::String::Utf8Value(args[1]->ToString()));
  }

  args.GetReturnValue().Set(v8::String::NewFromUtf8(
      isolate,
      ReplaceStringPlaceholders(message, substitutions, NULL).c_str()));
}

void I18NCustomBindings::GetL10nUILanguage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8::String::NewFromUtf8(
      args.GetIsolate(), content::RenderThread::Get()->GetLocale().c_str()));
}

}  // namespace extensions
