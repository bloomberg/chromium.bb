// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"

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

FileBrowserHandlerCustomBindings::FileBrowserHandlerCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction(
      "GetExternalFileEntry",
      base::Bind(&FileBrowserHandlerCustomBindings::GetExternalFileEntry,
                 base::Unretained(this)));
}

void FileBrowserHandlerCustomBindings::GetExternalFileEntry(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(zelidrag): Make this magic work on other platforms when file browser
  // matures enough on ChromeOS.
#if defined(OS_CHROMEOS)
    CHECK(args.Length() == 1);
    CHECK(args[0]->IsObject());
    v8::Local<v8::Object> file_def = args[0]->ToObject();
    std::string file_system_name(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::New("fileSystemName"))));
    std::string file_system_path(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::New("fileSystemRoot"))));
    std::string file_full_path(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::New("fileFullPath"))));
    bool is_directory =
        file_def->Get(v8::String::New("fileIsDirectory"))->ToBoolean()->Value();
    WebKit::WebFrame* webframe =
        WebKit::WebFrame::frameForContext(context()->v8_context());
    args.GetReturnValue().Set(webframe->createFileEntry(
        WebKit::WebFileSystemTypeExternal,
        WebKit::WebString::fromUTF8(file_system_name.c_str()),
        WebKit::WebString::fromUTF8(file_system_path.c_str()),
        WebKit::WebString::fromUTF8(file_full_path.c_str()),
        is_directory));
#endif
}

}  // namespace extensions
