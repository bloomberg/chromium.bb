// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/sync_file_system_custom_bindings.h"

#include <string>

#include "chrome/common/extensions/extension_constants.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"
#include "webkit/fileapi/file_system_util.h"

namespace extensions {

SyncFileSystemCustomBindings::SyncFileSystemCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteFunction(
      "GetSyncFileSystemObject",
      base::Bind(&SyncFileSystemCustomBindings::GetSyncFileSystemObject,
                 base::Unretained(this)));
}

v8::Handle<v8::Value> SyncFileSystemCustomBindings::GetSyncFileSystemObject(
    const v8::Arguments& args) {
  if (args.Length() != 2) {
    NOTREACHED();
    return v8::Undefined();
  }
  if (!args[0]->IsString()) {
    NOTREACHED();
    return v8::Undefined();
  }
  if (!args[1]->IsString()) {
    NOTREACHED();
    return v8::Undefined();
  }

  std::string name(*v8::String::Utf8Value(args[0]));
  if (name.empty()) {
    NOTREACHED();
    return v8::Undefined();
  }
  std::string root_url(*v8::String::Utf8Value(args[1]));
  if (root_url.empty()) {
    NOTREACHED();
    return v8::Undefined();
  }

  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  return webframe->createFileSystem(WebKit::WebFileSystem::TypeExternal,
                                    WebKit::WebString::fromUTF8(name),
                                    WebKit::WebString::fromUTF8(root_url));
}

}  // namespace extensions
