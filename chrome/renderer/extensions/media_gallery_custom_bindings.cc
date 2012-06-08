// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/media_gallery_custom_bindings.h"

#include <string>

#include "base/file_path.h"
#include "base/stringprintf.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"
#include "webkit/fileapi/file_system_util.h"

namespace extensions {

MediaGalleryCustomBindings::MediaGalleryCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteFunction(
      "GetMediaFileSystemObject",
      base::Bind(&MediaGalleryCustomBindings::GetMediaFileSystemObject,
                 base::Unretained(this)));
  RouteFunction(
      "ExtractEmbeddedThumbnails",
      base::Bind(&MediaGalleryCustomBindings::ExtractEmbeddedThumbnails,
                 base::Unretained(this)));
}

v8::Handle<v8::Value> MediaGalleryCustomBindings::GetMediaFileSystemObject(
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
  std::string dirname(*v8::String::Utf8Value(args[1]));
  if (dirname.empty()) {
    NOTREACHED();
    return v8::Undefined();
  }

  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  const GURL origin = GURL(webframe->document().securityOrigin().toString());
  const GURL root_url =
      fileapi::GetFileSystemRootURI(origin, fileapi::kFileSystemTypeIsolated);
  const std::string fsname = fileapi::GetIsolatedFileSystemName(origin, fsid);
  const std::string url = base::StringPrintf("%s%s/%s/",
                                             root_url.spec().c_str(),
                                             fsid.c_str(),
                                             dirname.c_str());
  return webframe->createFileSystem(WebKit::WebFileSystem::TypeIsolated,
                                    WebKit::WebString::fromUTF8(fsname),
                                    WebKit::WebString::fromUTF8(url));
}

v8::Handle<v8::Value> MediaGalleryCustomBindings::ExtractEmbeddedThumbnails(
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
