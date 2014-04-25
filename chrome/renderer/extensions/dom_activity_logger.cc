// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/dom_activity_logger.h"

#include "chrome/common/extensions/ad_injection_constants.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/activity_log_converter_strategy.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_messages.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDOMActivityLogger.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;
using blink::WebString;
using blink::WebURL;

namespace extensions {

namespace {

scoped_ptr<base::Value> ConvertV8Value(const std::string& api_name,
                                       const v8::Handle<v8::Value> v8_value) {
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  ActivityLogConverterStrategy strategy;
  strategy.set_enable_detailed_parsing(
      ad_injection_constants::ApiCanInjectAds(api_name));
  converter->SetFunctionAllowed(true);
  converter->SetStrategy(&strategy);
  return scoped_ptr<base::Value>(converter->FromV8Value(
      v8_value, v8::Isolate::GetCurrent()->GetCurrentContext()));
}

}  // namespace

DOMActivityLogger::DOMActivityLogger(const std::string& extension_id)
    : extension_id_(extension_id) {
}

DOMActivityLogger::~DOMActivityLogger() {}

void DOMActivityLogger::log(
    const WebString& api_name,
    int argc,
    const v8::Handle<v8::Value> argv[],
    const WebString& call_type,
    const WebURL& url,
    const WebString& title) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  std::string api_name_utf8 = api_name.utf8();
  for (int i = 0; i < argc; ++i)
    args->Append(ConvertV8Value(api_name_utf8, argv[i]).release());

  DomActionType::Type type = DomActionType::METHOD;
  if (call_type == "Getter")
    type = DomActionType::GETTER;
  else if (call_type == "Setter")
    type = DomActionType::SETTER;
  // else DomActionType::METHOD is correct.
  SendDomActionMessage(
      api_name_utf8, url, title, type, args.Pass());
}

void DOMActivityLogger::AttachToWorld(int world_id,
                                      const std::string& extension_id) {
#if defined(ENABLE_EXTENSIONS)
  // If there is no logger registered for world_id, construct a new logger
  // and register it with world_id.
  if (!blink::hasDOMActivityLogger(world_id)) {
    DOMActivityLogger* logger = new DOMActivityLogger(extension_id);
    blink::setDOMActivityLogger(world_id, logger);
  }
#endif
}

void DOMActivityLogger::logGetter(const WebString& api_name,
                                  const WebURL& url,
                                  const WebString& title) {
  SendDomActionMessage(api_name.utf8(),
                       url,
                       title,
                       DomActionType::GETTER,
                       scoped_ptr<base::ListValue>(new base::ListValue()));
}

void DOMActivityLogger::logSetter(const WebString& api_name,
                                  const v8::Handle<v8::Value>& new_value,
                                  const WebURL& url,
                                  const WebString& title) {
  logSetter(api_name, new_value, v8::Handle<v8::Value>(), url, title);
}

void DOMActivityLogger::logSetter(const WebString& api_name,
                                  const v8::Handle<v8::Value>& new_value,
                                  const v8::Handle<v8::Value>& old_value,
                                  const WebURL& url,
                                  const WebString& title) {
  scoped_ptr<base::ListValue> args(new base::ListValue);
  std::string api_name_utf8 = api_name.utf8();
  args->Append(ConvertV8Value(api_name_utf8, new_value).release());
  if (!old_value.IsEmpty())
    args->Append(ConvertV8Value(api_name_utf8, old_value).release());
  SendDomActionMessage(
      api_name_utf8, url, title, DomActionType::SETTER, args.Pass());
}

void DOMActivityLogger::logMethod(const WebString& api_name,
                                  int argc,
                                  const v8::Handle<v8::Value>* argv,
                                  const WebURL& url,
                                  const WebString& title) {
  scoped_ptr<base::ListValue> args(new base::ListValue);
  std::string api_name_utf8 = api_name.utf8();
  for (int i = 0; i < argc; ++i)
    args->Append(ConvertV8Value(api_name_utf8, argv[i]).release());
  SendDomActionMessage(
      api_name_utf8, url, title, DomActionType::METHOD, args.Pass());
}

void DOMActivityLogger::SendDomActionMessage(const std::string& api_call,
                                             const GURL& url,
                                             const base::string16& url_title,
                                             DomActionType::Type call_type,
                                             scoped_ptr<base::ListValue> args) {
  ExtensionHostMsg_DOMAction_Params params;
  params.api_call = api_call;
  params.url = url;
  params.url_title = url_title;
  params.call_type = call_type;
  params.arguments.Swap(args.get());
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_AddDOMActionToActivityLog(extension_id_, params));
}

}  // namespace extensions
