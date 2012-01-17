// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_browser_private_custom_bindings.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace extensions {

FileBrowserPrivateCustomBindings::FileBrowserPrivateCustomBindings(
    int dependency_count,
    const char** dependencies)
    : ChromeV8Extension("extensions/file_browser_private_custom_bindings.js",
                        IDR_FILE_BROWSER_PRIVATE_CUSTOM_BINDINGS_JS,
                        dependency_count,
                        dependencies,
                        NULL) {}

static v8::Handle<v8::Value> GetLocalFileSystem(
    const v8::Arguments& args) {
  DCHECK(args.Length() == 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsString());
  std::string name(*v8::String::Utf8Value(args[0]));
  std::string path(*v8::String::Utf8Value(args[1]));

  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  DCHECK(webframe);
  return webframe->createFileSystem(
      WebKit::WebFileSystem::TypeExternal,
      WebKit::WebString::fromUTF8(name.c_str()),
      WebKit::WebString::fromUTF8(path.c_str()));
}

v8::Handle<v8::FunctionTemplate>
FileBrowserPrivateCustomBindings::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetLocalFileSystem")))
    return v8::FunctionTemplate::New(GetLocalFileSystem);

  return ChromeV8Extension::GetNativeFunction(name);
}

}  // namespace extensions
