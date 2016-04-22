// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/sync_file_system_custom_bindings.h"

#include <string>

#include "extensions/renderer/script_context.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/WebKit/public/web/WebDOMFileSystem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

namespace extensions {

SyncFileSystemCustomBindings::SyncFileSystemCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetSyncFileSystemObject", "syncFileSystem",
      base::Bind(&SyncFileSystemCustomBindings::GetSyncFileSystemObject,
                 base::Unretained(this)));
}

void SyncFileSystemCustomBindings::GetSyncFileSystemObject(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2) {
    NOTREACHED();
    return;
  }
  if (!args[0]->IsString()) {
    NOTREACHED();
    return;
  }
  if (!args[1]->IsString()) {
    NOTREACHED();
    return;
  }

  std::string name(*v8::String::Utf8Value(args[0]));
  if (name.empty()) {
    NOTREACHED();
    return;
  }
  std::string root_url(*v8::String::Utf8Value(args[1]));
  if (root_url.empty()) {
    NOTREACHED();
    return;
  }

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::frameForContext(context()->v8_context());
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::create(webframe,
                                      blink::WebFileSystemTypeExternal,
                                      blink::WebString::fromUTF8(name),
                                      GURL(root_url))
          .toV8Value(context()->v8_context()->Global(), args.GetIsolate()));
}

}  // namespace extensions
