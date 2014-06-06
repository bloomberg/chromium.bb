// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"

#include <string>

#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDOMFileSystem.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace extensions {

namespace {

// FileSystemObject GetMediaFileSystem(string file_system_url): construct
// a file system object from a file system url.
void GetMediaFileSystemObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());

  std::string fs_mount(*v8::String::Utf8Value(args[0]));
  CHECK(!fs_mount.empty());

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::frameForCurrentContext();
  const GURL origin = GURL(webframe->document().securityOrigin().toString());
  std::string fs_name =
      fileapi::GetFileSystemName(origin, fileapi::kFileSystemTypeExternal);
  fs_name.append("_");
  fs_name.append(fs_mount);
  const GURL root_url(
      fileapi::GetExternalFileSystemRootURIString(origin, fs_mount));
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::create(webframe,
                                      blink::WebFileSystemTypeExternal,
                                      blink::WebString::fromUTF8(fs_name),
                                      root_url)
          .toV8Value(args.Holder(), args.GetIsolate()));
}

}  // namespace

MediaGalleriesCustomBindings::MediaGalleriesCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("GetMediaFileSystemObject",
                base::Bind(&GetMediaFileSystemObject));
}

}  // namespace extensions
