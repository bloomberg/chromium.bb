// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDOMFileSystem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptBindings.h"

namespace extensions {

FileBrowserHandlerCustomBindings::FileBrowserHandlerCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetExternalFileEntry",
      base::Bind(&FileBrowserHandlerCustomBindings::GetExternalFileEntry,
                 base::Unretained(this)));
  RouteFunction("GetEntryURL",
                base::Bind(&FileBrowserHandlerCustomBindings::GetEntryURL,
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
            v8::String::NewFromUtf8(args.GetIsolate(), "fileSystemName"))));
    GURL file_system_root(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::NewFromUtf8(args.GetIsolate(), "fileSystemRoot"))));
    std::string file_full_path(
        *v8::String::Utf8Value(file_def->Get(
            v8::String::NewFromUtf8(args.GetIsolate(), "fileFullPath"))));
    bool is_directory = file_def->Get(v8::String::NewFromUtf8(
        args.GetIsolate(), "fileIsDirectory"))->ToBoolean()->Value();
    blink::WebDOMFileSystem::EntryType entry_type =
        is_directory ? blink::WebDOMFileSystem::EntryTypeDirectory
                     : blink::WebDOMFileSystem::EntryTypeFile;
    blink::WebLocalFrame* webframe =
        blink::WebLocalFrame::frameForContext(context()->v8_context());
    args.GetReturnValue().Set(
        blink::WebDOMFileSystem::create(
            webframe,
            blink::WebFileSystemTypeExternal,
            blink::WebString::fromUTF8(file_system_name),
            file_system_root)
            .createV8Entry(blink::WebString::fromUTF8(file_full_path),
                           entry_type,
                           args.Holder(),
                           args.GetIsolate()));
#endif
}

void FileBrowserHandlerCustomBindings::GetEntryURL(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsObject());
  const blink::WebURL& url =
      blink::WebDOMFileSystem::createFileSystemURL(args[0]);
  args.GetReturnValue().Set(
      blink::WebScriptBindings::toV8String(url.string(), args.GetIsolate()));
}

}  // namespace extensions
