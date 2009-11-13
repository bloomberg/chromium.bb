// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/character_set_converters.h"

namespace browser_sync {

void TrimPathStringToValidCharacter(std::string* string) {
  // Constants from http://en.wikipedia.org/wiki/UTF-8
  CHECK(string);
  if (string->empty())
    return;
  if (0 == (string->at(string->length() - 1) & 0x080))
    return;
  size_t partial_enc_bytes = 0;
  for (partial_enc_bytes = 0 ; true ; ++partial_enc_bytes) {
    if (4 == partial_enc_bytes || partial_enc_bytes == string->length()) {
      // original string was broken, garbage in, garbage out.
      return;
    }
    PathChar c = string->at(string->length() - 1 - partial_enc_bytes);
    if ((c & 0x0c0) == 0x080)  // utf continuation char;
      continue;
    if ((c & 0x0e0) == 0x0e0) {  // 2-byte encoded char.
      if (1 == partial_enc_bytes)
        return;
      else
        break;
    }
    if ((c & 0x0f0) == 0xc0) {  // 3-byte encoded char.
      if (2 == partial_enc_bytes)
        return;
      else
        break;
    }
    if ((c & 0x0f8) == 0x0f0) {  // 4-byte encoded char.
      if (3 == partial_enc_bytes)
        return;
      else
        break;
    }
  }
  string->resize(string->length() - 1 - partial_enc_bytes);
}

}  // namespace browser_sync
