// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_ENTRY_H_
#define NET_SPDY_HPACK_ENTRY_H_

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-06

// A structure for an entry in the header table (3.1.2) and the
// reference set (3.1.3). This structure also keeps track of how many
// times the entry has been 'touched', which is useful for both
// encoding and decoding.
class NET_EXPORT_PRIVATE HpackEntry {
 public:
  // The constant amount added to name().size() and value().size() to
  // get the size of an HpackEntry as defined in 3.3.1.
  static const uint32 kSizeOverhead;

  // The constant returned by touch_count() if an entry hasn't been
  // touched (which is distinct from an entry having a touch count of
  // 0).
  //
  // TODO(akalin): The distinction between untouched and having a
  // touch count of 0 is confusing. Think of a better way to represent
  // this state.
  static const uint32 kUntouched;

  // Creates an entry with empty name a value. Only defined so that
  // entries can be stored in STL containers.
  HpackEntry();

  // Creates an entry with a copy is made of the given name and value.
  //
  // TODO(akalin): Add option to not make a copy (for static table
  // entries).
  HpackEntry(base::StringPiece name, base::StringPiece value);

  // Copy constructor and assignment operator welcome.

  // The name() and value() StringPieces have the same lifetime as
  // this entry.

  base::StringPiece name() const { return base::StringPiece(name_); }
  base::StringPiece value() const { return base::StringPiece(value_); }

  // Returns whether or not this entry is in the reference set.
  bool IsReferenced() const;

  // Returns how many touches this entry has, or kUntouched if this
  // entry hasn't been touched at all. The meaning of the touch count
  // is defined by whatever is calling
  // AddTouchCount()/ClearTouchCount() (i.e., the encoder or decoder).
  uint32 TouchCount() const;

  // Returns the size of an entry as defined in 3.3.1. The returned
  // value may not necessarily fit in 32 bits.
  size_t Size() const;

  std::string GetDebugString() const;

  // Returns whether this entry has the same name, value, referenced
  // state, and touch count as the given one.
  bool Equals(const HpackEntry& other) const;

  void SetReferenced(bool referenced);

  // Adds the given number of touches to this entry (see
  // TouchCount()). The total number of touches must not exceed 2^31 -
  // 2. It is guaranteed that this entry's touch count will not equal
  // kUntouched after this function is called (even if touch_count ==
  // 0).
  void AddTouches(uint32 additional_touch_count);

  // Sets the touch count of this entry to kUntouched.
  void ClearTouches();

 private:
  std::string name_;
  std::string value_;

  // The high bit stores 'referenced' and the rest stores the touch
  // count.
  uint32 referenced_and_touch_count_;
};

}  // namespace net

#endif  // NET_SPDY_HPACK_ENTRY_H_
