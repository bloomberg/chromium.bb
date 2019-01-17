// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_COMMON_MEM_BUFFER_UTIL_H_
#define FUCHSIA_COMMON_MEM_BUFFER_UTIL_H_

#include <fuchsia/mem/cpp/fidl.h>
#include <lib/zx/channel.h>
#include <string>

#include "base/files/file.h"
#include "base/strings/utf_string_conversions.h"

namespace webrunner {

// Reads the contents of |buffer|, encoded in UTF-8, to a UTF-16 string.
// Returns |false| if |buffer| is not valid UTF-8.
bool ReadUTF8FromVMOAsUTF16(const fuchsia::mem::Buffer& buffer,
                            base::string16* output);

// Creates a Fuchsia memory buffer from |data|.
fuchsia::mem::Buffer MemBufferFromString(const base::StringPiece& data);

// Creates a Fuchsia memory buffer from the UTF-16 string |data|.
fuchsia::mem::Buffer MemBufferFromString16(const base::StringPiece16& data);

// Reads the contents of |buffer| into |output|.
// Returns true if the read operation succeeded.
bool StringFromMemBuffer(const fuchsia::mem::Buffer& buffer,
                         std::string* output);

// Creates a memory-mapped, read-only Buffer with the contents of |file|.
// Will return an empty Buffer if the file could not be opened.
fuchsia::mem::Buffer MemBufferFromFile(base::File file);

// Creates a non-resizeable, copy-on-write shared memory clone of |buffer|.
fuchsia::mem::Buffer CloneBuffer(const fuchsia::mem::Buffer& buffer);

}  // namespace webrunner

#endif  // FUCHSIA_COMMON_MEM_BUFFER_UTIL_H_
