// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_MARKER_H_
#define CHROME_COMMON_SPELLCHECK_MARKER_H_

class SpellCheckMarker {
 public:
  // A predicate to test spellcheck marker validity.
  class IsValidPredicate {
   public:
    typedef SpellCheckMarker argument_type;
    explicit IsValidPredicate(size_t text_length) : text_length_(text_length) {}
    bool operator()(const SpellCheckMarker& marker) const {
      return marker.offset < text_length_;
    }
   private:
    size_t text_length_;
  };

  // IPC requires a default constructor.
  SpellCheckMarker() : hash(0xFFFFFFFF), offset(static_cast<size_t>(-1)) {}

  SpellCheckMarker(uint32 hash, size_t offset) : hash(hash), offset(offset) {}

  uint32 hash;
  size_t offset;
};

#endif  // CHROME_COMMON_SPELLCHECK_MARKER_H_
