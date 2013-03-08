// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/dom_activity_logger.h"

#include "base/logging.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMActivityLogger.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

DOMActivityLogger::DOMActivityLogger(const std::string& extension_id,
                                     const GURL& url,
                                     const string16& title)
  : extension_id_(extension_id), url_(url), title_(title) {
}

void DOMActivityLogger::log(
    const WebString& api_name,
    int argc,
    const v8::Handle<v8::Value> argv[],
    const WebString& extra_info) {
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  scoped_ptr<ListValue> argv_list_value(new ListValue());
  for (int i =0; i < argc; i++) {
    argv_list_value->Set(
        i, converter->FromV8Value(argv[i], v8::Context::GetCurrent()));
  }
  ExtensionHostMsg_DOMAction_Params params;
  params.url = url_;
  params.url_title = title_;
  params.api_call = api_name.utf8();
  params.arguments.Swap(argv_list_value.get());
  params.extra = extra_info.utf8();

  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_AddDOMActionToActivityLog(extension_id_, params));
}

void DOMActivityLogger::AttachToWorld(int world_id,
                                      const std::string& extension_id,
                                      const GURL& url,
                                      const string16& title) {
  // Check if extension activity logging is enabled.
  if (!ChromeRenderProcessObserver::extension_activity_log_enabled())
    return;
  // If there is no logger registered for world_id, construct a new logger
  // and register it with world_id.
  if (!WebKit::hasDOMActivityLogger(world_id)) {
    DOMActivityLogger* logger = new DOMActivityLogger(extension_id, url, title);
    WebKit::setDOMActivityLogger(world_id, logger);
  }
}

}  // namespace extensions

