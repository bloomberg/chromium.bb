// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/extension_constants.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace extensions {

MediaGalleriesCustomBindings::MediaGalleriesCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction(
      "GetMediaFileSystemObject",
      base::Bind(&MediaGalleriesCustomBindings::GetMediaFileSystemObject,
                 base::Unretained(this)));
}

void MediaGalleriesCustomBindings::GetMediaFileSystemObject(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1) {
    NOTREACHED();
    return;
  }
  if (!args[0]->IsString()) {
    NOTREACHED();
    return;
  }

  std::string fsid(*v8::String::Utf8Value(args[0]));
  if (fsid.empty()) {
    NOTREACHED();
    return;
  }

  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  const GURL origin = GURL(webframe->document().securityOrigin().toString());
  const std::string fs_name = fileapi::GetIsolatedFileSystemName(origin, fsid);
  const std::string root_url =
      fileapi::GetIsolatedFileSystemRootURIString(
          origin, fsid, extension_misc::kMediaFileSystemPathPart);
  args.GetReturnValue().Set(
      webframe->createFileSystem(WebKit::WebFileSystemTypeIsolated,
                                 WebKit::WebString::fromUTF8(fs_name),
                                 WebKit::WebString::fromUTF8(root_url)));
}

}  // namespace extensions
