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
  RouteStaticFunction("CreateMediaGalleryObject", &CreateMediaGalleryObject);
}

// static
v8::Handle<v8::Value> MediaGalleryCustomBindings::CreateMediaGalleryObject(
    const v8::Arguments& args) {
  if (args.Length() != 3 || !args[0]->IsUint32() || args[1]->IsNull() ||
      !args[1]->IsString() || !args[2]->IsUint32()) {
    NOTREACHED() << "Bad arguments";
    return v8::Undefined();
  }

  std::string name;
  name = *v8::String::Utf8Value(args[1]->ToString());
  if (name.empty())
    return v8::Undefined();

  // TODO(vandebo) Create and return a MediaGallery object.
  // int id = args[0]->Uint32Value();
  // int flags = args[2]->Uint32Value();
  return v8::Undefined();
}

}  // namespace extensions
