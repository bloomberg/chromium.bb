// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_STRING_MESSAGE_CODEC_H_
#define CONTENT_BROWSER_ANDROID_STRING_MESSAGE_CODEC_H_

#include <vector>
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// To support exposing HTML message ports to Java, it is necessary to be able
// to encode and decode message data using the same serialization format as V8.
// That format is an implementation detail of V8, but we cannot invoke V8 in
// the browser process. Rather than IPC over to the renderer process to execute
// the V8 serialization code, we duplicate some of the serialization logic
// (just for simple string messages) here. This is a trade-off between overall
// complexity / performance and code duplication. Fortunately, we only need to
// handle string messages and this serialization format is static, as it is a
// format we currently persist to disk via IndexedDB.

CONTENT_EXPORT std::vector<uint8_t> EncodeStringMessage(
    const base::string16& data);

CONTENT_EXPORT bool DecodeStringMessage(
    const std::vector<uint8_t>& encoded_data,
    base::string16* result);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_STRING_MESSAGE_CODEC_H_
