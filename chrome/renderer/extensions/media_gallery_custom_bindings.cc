// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/media_gallery_custom_bindings.h"

#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

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
  if (args.Length() != 1) {
    NOTREACHED();
    return v8::Undefined();
  }
  if (!args[0]->IsString()) {
    NOTREACHED();
    return v8::Undefined();
  }

  std::string file_system_url;
  file_system_url = *v8::String::Utf8Value(args[1]->ToString());
  if (file_system_url.empty())
    return v8::Undefined();

  // TODO(vandebo) Create and return a FileSystem object.
  return v8::Undefined();
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
