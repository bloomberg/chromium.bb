// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"

#include <string>

#include "chrome/common/extensions/extension_constants.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace extensions {

namespace {

// FileSystemObject GetMediaFileSystem(string file_system_url): construct
// a file system object from a file system url.
void GetMediaFileSystemObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());

  std::string fsid(*v8::String::Utf8Value(args[0]));
  CHECK(!fsid.empty());

  blink::WebFrame* webframe = blink::WebFrame::frameForCurrentContext();
  const GURL origin = GURL(webframe->document().securityOrigin().toString());
  const std::string fs_name = fileapi::GetIsolatedFileSystemName(origin, fsid);
  const std::string root_url =
      fileapi::GetIsolatedFileSystemRootURIString(
          origin, fsid, extension_misc::kMediaFileSystemPathPart);
  args.GetReturnValue().Set(
      webframe->createFileSystem(blink::WebFileSystemTypeIsolated,
                                 blink::WebString::fromUTF8(fs_name),
                                 blink::WebString::fromUTF8(root_url)));
}

}  // namespace

MediaGalleriesCustomBindings::MediaGalleriesCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetMediaFileSystemObject",
                base::Bind(&GetMediaFileSystemObject));
}

}  // namespace extensions
