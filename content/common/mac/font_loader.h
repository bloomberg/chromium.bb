// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_FONT_LOADER_H_
#define CONTENT_COMMON_MAC_FONT_LOADER_H_

#include <memory>

#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>

#include "base/callback_forward.h"
#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"

struct FontDescriptor;

namespace content {

// Provides functionality to transmit fonts over IPC.
//
// Note about font formats: .dfont (datafork suitcase) fonts are currently not
// supported by this code since CGFontCreateWithDataProvider() can't handle them
// directly.
class FontLoader {
 public:
  // Internal font load result data. Exposed here for testing.
  struct ResultInternal {
    uint32_t font_data_size = 0;
    base::SharedMemory font_data;
    uint32_t font_id = 0;
  };

  // Callback for the reporting result of LoadFont().
  // - The first argument is the data size.
  // - The SharedMemoryHandle points to a shared memory buffer containing the
  //   raw data for the font file.
  // - The last argument is the font_id: a unique identifier for the on-disk
  //   file we load for the font.
  using LoadedCallback =
      base::OnceCallback<void(uint32_t, base::SharedMemoryHandle, uint32_t)>;

  // Load a font specified by |font| into a shared memory buffer suitable for
  // sending over IPC. On failure, zeroes and an invalid handle are reported
  // to the callback.
  CONTENT_EXPORT
  static void LoadFont(const FontDescriptor& font, LoadedCallback callback);

  // Given a shared memory buffer containing the raw data for a font file, load
  // the font and return a CGFontRef.
  //
  // |data| - A shared memory handle pointing to the raw data from a font file.
  // |data_size| - Size of |data|.
  //
  // On return:
  //  returns true on success, false on failure.
  //  |out| - A CGFontRef corresponding to the designated font.
  //  The caller is responsible for releasing this value via CGFontRelease()
  //  when done.
  CONTENT_EXPORT
  static bool CGFontRefFromBuffer(base::SharedMemoryHandle font_data,
                                  uint32_t font_data_size,
                                  CGFontRef* out);

  CONTENT_EXPORT
  static std::unique_ptr<ResultInternal> LoadFontForTesting(
      const FontDescriptor& font);
};

}  // namespace content

#endif  // CONTENT_COMMON_MAC_FONT_LOADER_H_
