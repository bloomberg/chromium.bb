// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_MARKER_H_
#define CHROME_COMMON_SPELLCHECK_MARKER_H_

struct SpellCheckMarker {
  SpellCheckMarker(uint32 hash, size_t offset) : hash(hash), offset(offset) {}

  uint32 hash;
  size_t offset;
};

#endif  // CHROME_COMMON_SPELLCHECK_MARKER_H_
