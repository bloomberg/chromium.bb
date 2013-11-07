// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_browser_private_custom_bindings.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"
#include "third_party/WebKit/public/platform/WebFileSystemType.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace extensions {

FileBrowserPrivateCustomBindings::FileBrowserPrivateCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction(
      "GetFileSystem",
       base::Bind(&FileBrowserPrivateCustomBindings::GetFileSystem,
                  base::Unretained(this)));
}

void FileBrowserPrivateCustomBindings::GetFileSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsString());
  std::string name(*v8::String::Utf8Value(args[0]));
  std::string root_url(*v8::String::Utf8Value(args[1]));

  blink::WebFrame* webframe =
      blink::WebFrame::frameForContext(context()->v8_context());
  DCHECK(webframe);
  args.GetReturnValue().Set(
      webframe->createFileSystem(
          blink::WebFileSystemTypeExternal,
          blink::WebString::fromUTF8(name.c_str()),
          blink::WebString::fromUTF8(root_url.c_str())));
}

}  // namespace extensions
