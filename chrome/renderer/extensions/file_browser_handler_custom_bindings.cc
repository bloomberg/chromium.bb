// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"

#include <string>

#include "base/logging.h"
#include "build/build_config.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_dom_file_system.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

FileBrowserHandlerCustomBindings::FileBrowserHandlerCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void FileBrowserHandlerCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetExternalFileEntry", "fileBrowserHandler",
      base::Bind(
          &FileBrowserHandlerCustomBindings::GetExternalFileEntryCallback,
          base::Unretained(this)));
}

void FileBrowserHandlerCustomBindings::GetExternalFileEntry(
    const v8::FunctionCallbackInfo<v8::Value>& args,
    ScriptContext* context) {
// TODO(zelidrag): Make this magic work on other platforms when file browser
// matures enough on ChromeOS.
#if defined(OS_CHROMEOS)
    CHECK(args.Length() == 1);
    CHECK(args[0]->IsObject());
    v8::Local<v8::Object> file_def = args[0].As<v8::Object>();
    v8::Isolate* isolate = args.GetIsolate();
    std::string file_system_name(*v8::String::Utf8Value(
        isolate,
        file_def->Get(v8::String::NewFromUtf8(isolate, "fileSystemName"))));
    GURL file_system_root(*v8::String::Utf8Value(
        isolate,
        file_def->Get(v8::String::NewFromUtf8(isolate, "fileSystemRoot"))));
    std::string file_full_path(*v8::String::Utf8Value(
        isolate,
        file_def->Get(v8::String::NewFromUtf8(isolate, "fileFullPath"))));
    bool is_directory =
        file_def->Get(v8::String::NewFromUtf8(isolate, "fileIsDirectory"))
            ->BooleanValue(context->v8_context())
            .FromMaybe(false);
    blink::WebDOMFileSystem::EntryType entry_type =
        is_directory ? blink::WebDOMFileSystem::kEntryTypeDirectory
                     : blink::WebDOMFileSystem::kEntryTypeFile;
    blink::WebLocalFrame* webframe =
        blink::WebLocalFrame::FrameForContext(context->v8_context());
    args.GetReturnValue().Set(
        blink::WebDOMFileSystem::Create(
            webframe, blink::kWebFileSystemTypeExternal,
            blink::WebString::FromUTF8(file_system_name), file_system_root)
            .CreateV8Entry(blink::WebString::FromUTF8(file_full_path),
                           entry_type, args.Holder(), isolate));
#endif
}

void FileBrowserHandlerCustomBindings::GetExternalFileEntryCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  GetExternalFileEntry(args, context());
}

}  // namespace extensions
