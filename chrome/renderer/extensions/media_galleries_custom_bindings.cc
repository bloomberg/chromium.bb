// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"

#include <string>

#include "base/files/file_path.h"
#include "base/stringprintf.h"
#include "chrome/common/extensions/extension_constants.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"
#include "webkit/fileapi/file_system_util.h"

namespace extensions {

MediaGalleriesCustomBindings::MediaGalleriesCustomBindings(
    Dispatcher* dispatcher, v8::Handle<v8::Context> v8_context)
    : ChromeV8Extension(dispatcher, v8_context) {
  RouteFunction(
      "GetMediaFileSystemObject",
      base::Bind(&MediaGalleriesCustomBindings::GetMediaFileSystemObject,
                 base::Unretained(this)));
  RouteFunction(
      "ExtractEmbeddedThumbnails",
      base::Bind(&MediaGalleriesCustomBindings::ExtractEmbeddedThumbnails,
                 base::Unretained(this)));
}

v8::Handle<v8::Value> MediaGalleriesCustomBindings::GetMediaFileSystemObject(
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

  std::string fsid(*v8::String::Utf8Value(args[0]));
  if (fsid.empty()) {
    NOTREACHED();
    return v8::Undefined();
  }
  std::string name(*v8::String::Utf8Value(args[1]));
  if (name.empty()) {
    NOTREACHED();
    return v8::Undefined();
  }

  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  const GURL origin = GURL(webframe->document().securityOrigin().toString());
  const std::string root_url =
      fileapi::GetIsolatedFileSystemRootURIString(
          origin, fsid, extension_misc::kMediaFileSystemPathPart);
  return webframe->createFileSystem(
                                    WebKit::WebFileSystemTypeIsolated,
                                    WebKit::WebString::fromUTF8(name),
                                    WebKit::WebString::fromUTF8(root_url));
}

v8::Handle<v8::Value> MediaGalleriesCustomBindings::ExtractEmbeddedThumbnails(
    const v8::Arguments& args) {
  if (args.Length() != 1) {
    NOTREACHED() << "Bad arguments";
    return v8::Undefined();
  }
  // TODO(vandebo) Check that the object is a FileEntry.

  // TODO(vandebo) Create and return a Directory entry object.
  return v8::Null();
}

}  // namespace extensions
