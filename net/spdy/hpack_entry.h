// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_ENTRY_H_
#define NET_SPDY_HPACK_ENTRY_H_

#include <cstddef>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-06

namespace net {

// A structure for an entry in the header table (3.1.2) and the
// reference set (3.1.3).
class NET_EXPORT_PRIVATE HpackEntry {
 public:
  // The constant amount added to name().size() and value().size() to
  // get the size of an HpackEntry as defined in 3.3.1.
  static const size_t kSizeOverhead;

  // Implements a total ordering of HpackEntry on name(), value(), then Index()
  // ascending. Note that Index() may change over the lifetime of an HpackEntry,
  // but the relative Index() order of two entries will not. This comparator is
  // composed with the 'lookup' HpackEntry constructor to allow for efficient
  // lower-bounding of matching entries.
  struct NET_EXPORT_PRIVATE Comparator {
    bool operator() (const HpackEntry* lhs, const HpackEntry* rhs) const;
  };
  typedef std::set<HpackEntry*, Comparator> OrderedSet;

  // Creates an entry. Preconditions:
  // - |is_static| captures whether this entry is a member of the static
  //   or dynamic header table.
  // - |insertion_index| is this entry's index in the total set of entries ever
  //   inserted into the header table (including static entries).
  // - |total_table_insertions_or_current_size| references an externally-
  //   updated count of either the total number of header insertions (if
  //   !|is_static|), or the current size of the header table (if |is_static|).
  //
  // The combination of |is_static|, |insertion_index|, and
  // |total_table_insertions_or_current_size| allows an HpackEntry to determine
  // its current table index in O(1) time.
  HpackEntry(base::StringPiece name,
             base::StringPiece value,
             bool is_static,
             size_t insertion_index,
             const size_t* total_table_insertions_or_current_size);

  // Create a 'lookup' entry (only) suitable for querying a HpackEntrySet. The
  // instance Index() always returns 0, and will lower-bound all entries
  // matching |name| & |value| in an OrderedSet.
  HpackEntry(base::StringPiece name, base::StringPiece value);

  // Creates an entry with empty name a value. Only defined so that
  // entries can be stored in STL containers.
  HpackEntry();

  ~HpackEntry();

  const std::string& name() const { return name_; }
  const std::string& value() const { return value_; }

  // Returns whether this entry is a member of the static (as opposed to
  // dynamic) table.
  bool IsStatic() const { return is_static_; }

  // Returns and sets the state of the entry, or zero if never set.
  // The semantics of |state| are specific to the encoder or decoder.
  uint8 state() const { return state_; }
  void set_state(uint8 state) { state_ = state; }

  // Returns the entry's current index in the header table.
  size_t Index() const;

  // Returns the size of an entry as defined in 3.3.1.
  static size_t Size(base::StringPiece name, base::StringPiece value);
  size_t Size() const;

  std::string GetDebugString() const;

 private:
  // TODO(jgraettinger): Reduce copies, possibly via SpdyPinnableBufferPiece.
  std::string name_;
  std::string value_;

  bool is_static_;
  uint8 state_;

  // The entry's index in the total set of entries ever inserted into the header
  // table.
  size_t insertion_index_;

  // If |is_static_|, references the current size of the headers table.
  // Else, references the total number of header insertions which have occurred.
  const size_t* total_insertions_or_size_;
};

}  // namespace net

#endif  // NET_SPDY_HPACK_ENTRY_H_
