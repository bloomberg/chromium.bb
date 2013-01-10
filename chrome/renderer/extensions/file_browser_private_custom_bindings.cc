// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_browser_private_custom_bindings.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

namespace {

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

}  // namespace

namespace extensions {

FileBrowserPrivateCustomBindings::FileBrowserPrivateCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteStaticFunction("GetLocalFileSystem", &GetLocalFileSystem);
}

}  // namespace extensions
