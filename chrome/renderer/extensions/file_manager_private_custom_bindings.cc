// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_manager_private_custom_bindings.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDOMFileSystem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

FileManagerPrivateCustomBindings::FileManagerPrivateCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetFileSystem",
       base::Bind(&FileManagerPrivateCustomBindings::GetFileSystem,
                  base::Unretained(this)));
}

void FileManagerPrivateCustomBindings::GetFileSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsString());
  std::string name(*v8::String::Utf8Value(args[0]));
  std::string root_url(*v8::String::Utf8Value(args[1]));

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::frameForContext(context()->v8_context());
  DCHECK(webframe);
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::create(webframe,
                                      blink::WebFileSystemTypeExternal,
                                      blink::WebString::fromUTF8(name),
                                      GURL(root_url))
          .toV8Value(args.Holder(), args.GetIsolate()));
}

}  // namespace extensions
