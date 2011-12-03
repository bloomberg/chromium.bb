// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_browser_private_bindings.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace {

const char* kDeps[] = {
  "extensions/event.js"
};

static v8::Handle<v8::Value> GetExternalFileEntry(const v8::Arguments& args) {
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
    WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
    return webframe->createFileEntry(
        WebKit::WebFileSystem::TypeExternal,
        WebKit::WebString::fromUTF8(file_system_name.c_str()),
        WebKit::WebString::fromUTF8(file_system_path.c_str()),
        WebKit::WebString::fromUTF8(file_full_path.c_str()),
        is_directory);
#else
    return v8::Undefined();
#endif
  }

}

FileBrowserPrivateBindings::FileBrowserPrivateBindings()
    : ChromeV8Extension("extensions/file_browser_private.js",
                        IDR_FILE_BROWSER_PRIVATE_BINDINGS_JS,
                        arraysize(kDeps), kDeps, NULL) {
}

v8::Handle<v8::FunctionTemplate> FileBrowserPrivateBindings::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetExternalFileEntry")))
    return v8::FunctionTemplate::New(GetExternalFileEntry);
  else
    return ChromeV8Extension::GetNativeFunction(name);
}
