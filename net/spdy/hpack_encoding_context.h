// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_ENCODING_CONTEXT_H_
#define NET_SPDY_HPACK_ENCODING_CONTEXT_H_

#include <cstddef>
#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/spdy/hpack_header_table.h"

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-06

namespace net {

namespace test {
class HpackEncodingContextPeer;
}  // namespace test

// An encoding context is simply a header table and its associated
// reference set and a static table.
class NET_EXPORT_PRIVATE HpackEncodingContext {
 public:
  friend class test::HpackEncodingContextPeer;

  // The constant returned by GetTouchesAt() if the indexed entry
  // hasn't been touched (which is distinct from having a touch count
  // of 0).
  //
  // TODO(akalin): The distinction between untouched and having a
  // touch count of 0 is confusing. Think of a better way to represent
  // this state.
  static const uint32 kUntouched;

  HpackEncodingContext();

  ~HpackEncodingContext();

  uint32 GetMutableEntryCount() const;

  uint32 GetEntryCount() const;

  // For all read accessors below, index must be >= 1 and <=
  // GetEntryCount().  For all mutating accessors below, index must be
  // >= 1 and <= GetMutableEntryCount().

  // The StringPieces returned by Get{Name,Value}At() live as long as
  // the next call to SetMaxSize() or the Process*() functions.

  base::StringPiece GetNameAt(uint32 index) const;

  base::StringPiece GetValueAt(uint32 index) const;

  bool IsReferencedAt(uint32 index) const;

  uint32 GetTouchCountAt(uint32 index) const;

  void SetReferencedAt(uint32 index, bool referenced);

  // Adds the given number of touches to the entry at the given
  // index. It is guaranteed that GetTouchCountAt(index) will not
  // equal kUntouched after this function is called (even if
  // touch_count == 0).
  void AddTouchesAt(uint32 index, uint32 touch_count);

  // Sets the touch count of the entry at the given index to
  // kUntouched.
  void ClearTouchesAt(uint32 index);

  // Called upon acknowledgement of SETTINGS_HEADER_TABLE_SIZE.
  // If |max_size| is smaller than the current header table size, the change
  // is treated as an implicit maximum-size context update.
  void ApplyHeaderTableSizeSetting(uint32 max_size);

  // The Process*() functions below return true on success and false
  // if an error was encountered.

  // Section 4.4. Sets the maximum size of the header table, evicting entries
  // as needed. Fails if |max_size| is larger than SETTINGS_HEADER_TABLE_SIZE.
  bool ProcessContextUpdateNewMaximumSize(uint32 max_size);

  // Section 4.4. Drops all headers from the reference set.
  bool ProcessContextUpdateEmptyReferenceSet();

  // Tries to update the encoding context given an indexed header
  // opcode for the given index as described in section 3.2.1.
  // new_index is filled in with the index of a mutable entry,
  // or 0 if one was not created. removed_referenced_indices is filled
  // in with the indices of all entries removed from the reference set.
  bool ProcessIndexedHeader(uint32 nonzero_index,
                            uint32* new_index,
                            std::vector<uint32>* removed_referenced_indices);

  // Tries to update the encoding context given a literal header with
  // incremental indexing opcode for the given name and value as
  // described in 3.2.1. index is filled in with the index of the new
  // entry if the header was successfully indexed, or 0 if
  // not. removed_referenced_indices is filled in with the indices of
  // all entries removed from the reference set.
  bool ProcessLiteralHeaderWithIncrementalIndexing(
      base::StringPiece name,
      base::StringPiece value,
      uint32* index,
      std::vector<uint32>* removed_referenced_indices);

 private:
  // Last acknowledged value for SETTINGS_HEADER_TABLE_SIZE.
  uint32 settings_header_table_size_;

  HpackHeaderTable header_table_;

  DISALLOW_COPY_AND_ASSIGN(HpackEncodingContext);
};

}  // namespace net

#endif  // NET_SPDY_HPACK_ENCODING_CONTEXT_H_
