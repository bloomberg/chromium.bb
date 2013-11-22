// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_BLOB_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_BLOB_NATIVE_HANDLER_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// This native handler is used to extract Blobs' UUIDs and pass them over to the
// browser process extension implementation via argument modification. This is
// necessary to support extension functions that take Blob parameters, as Blobs
// are not serialized and sent over to the browser process in the normal way.
//
// Blobs sent via this method don't have their ref-counts incremented, so the
// app using this technique must be sure to keep a reference.
class BlobNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit BlobNativeHandler(ChromeV8Context* context);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_BLOB_NATIVE_HANDLER_H_
