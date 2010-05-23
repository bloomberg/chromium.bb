// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FONT_LOADER_MAC_H_
#define CHROME_COMMON_FONT_LOADER_MAC_H_

#include <ApplicationServices/ApplicationServices.h>

#include "base/shared_memory.h"
#include "base/string16.h"

// Provides functionality to transmit fonts over IPC.
//
// Note about font formats: .dfont (datafork suitcase) fonts are currently not
// supported by this code since ATSFontActivateFromMemory() can't handle them
// directly.

class FontLoader {
 public:
  // Load a font specified by |font_name| and |font_point_size| into a shared
  // memory buffer suitable for sending over IPC.
  //
  // On return:
  //  returns true on success, false on failure.
  //  |font_data| - shared memory buffer containing the raw data for the font
  // file.
  //  |font_data_size| - size of data contained in |font_data|.
  static bool LoadFontIntoBuffer(const string16& font_name,
                                 float font_point_size,
                                 base::SharedMemory* font_data,
                                 uint32* font_data_size);

  // Given a shared memory buffer containing the raw data for a font file, load
  // the font into a CGFontRef.
  //
  // |data| - A shared memory handle pointing to the raw data from a font file.
  // |data_size| - Size of |data|.
  //
  // On return:
  //  returns true on success, false on failure.
  //  |font| - A CGFontRef containing the designated font, the caller is
  // responsible for releasing this value.
  static bool CreateCGFontFromBuffer(base::SharedMemoryHandle font_data,
                                     uint32 font_data_size,
                                     CGFontRef* font);
};

#endif // CHROME_COMMON_FONT_LOADER_MAC_H_
