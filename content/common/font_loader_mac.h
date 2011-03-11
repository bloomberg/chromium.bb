// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FONT_LOADER_MAC_H_
#define CONTENT_COMMON_FONT_LOADER_MAC_H_
#pragma once

#include <ApplicationServices/ApplicationServices.h>

#include "base/shared_memory.h"

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

// Provides functionality to transmit fonts over IPC.
//
// Note about font formats: .dfont (datafork suitcase) fonts are currently not
// supported by this code since ATSFontActivateFromMemory() can't handle them
// directly.

class FontLoader {
 public:
  // Load a font specified by |font_to_encode| into a shared memory buffer
  // suitable for sending over IPC.
  //
  // On return:
  //  returns true on success, false on failure.
  //  |font_data| - shared memory buffer containing the raw data for the font
  // file.
  //  |font_data_size| - size of data contained in |font_data|.
  static bool LoadFontIntoBuffer(NSFont* font_to_encode,
                                 base::SharedMemory* font_data,
                                 uint32* font_data_size);

  // Given a shared memory buffer containing the raw data for a font file, load
  // the font and return a container ref.
  //
  // |data| - A shared memory handle pointing to the raw data from a font file.
  // |data_size| - Size of |data|.
  //
  // On return:
  //  returns true on success, false on failure.
  //  |font_container| - A font container corresponding to the designated font.
  //  The caller is responsible for releasing this value via ATSFontDeactivate()
  //  when done
  static bool ATSFontContainerFromBuffer(base::SharedMemoryHandle font_data,
                                         uint32 font_data_size,
                                         ATSFontContainerRef* font_container);
};

#endif // CONTENT_COMMON_FONT_LOADER_MAC_H_
