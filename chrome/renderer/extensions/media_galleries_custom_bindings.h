// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_MEDIA_GALLERIES_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_MEDIA_GALLERIES_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

// Implements custom bindings for the media galleries API.
class MediaGalleriesCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit MediaGalleriesCustomBindings(ScriptContext* context);

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_MEDIA_GALLERIES_CUSTOM_BINDINGS_H_
