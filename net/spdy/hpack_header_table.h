// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_HEADER_TABLE_H_
#define NET_SPDY_HPACK_HEADER_TABLE_H_

#include <cstddef>
#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/spdy/hpack_entry.h"

namespace net {

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-06

// A data structure for both the header table (described in 3.1.2) and
// the reference set (3.1.3). This structure also keeps track of how
// many times a header has been 'touched', which is useful for both
// encoding and decoding.
class NET_EXPORT_PRIVATE HpackHeaderTable {
 public:
  HpackHeaderTable();

  ~HpackHeaderTable();

  uint32 size() const { return size_; }
  uint32 max_size() const { return max_size_; }

  // Returns the total number of entries.
  uint32 GetEntryCount() const;

  // The given index must be >= 1 and <= GetEntryCount().
  const HpackEntry& GetEntry(uint32 index) const;

  // The given index must be >= 1 and <= GetEntryCount().
  HpackEntry* GetMutableEntry(uint32 index);

  // Sets the maximum size of the header table, evicting entries if
  // necessary as described in 3.3.2.
  void SetMaxSize(uint32 max_size);

  // The given entry must not be one from the header table, since it
  // may get evicted. Tries to add the given entry to the header
  // table, evicting entries if necessary as described in 3.3.3. index
  // will be filled in with the index of the added entry, or 0 if the
  // entry could not be added. removed_referenced_indices will be
  // filled in with the indices of any removed entries that were in
  // the reference set.
  void TryAddEntry(const HpackEntry& entry,
                   uint32* index,
                   std::vector<uint32>* removed_referenced_indices);

 private:
  std::deque<HpackEntry> entries_;
  uint32 size_;
  uint32 max_size_;

  DISALLOW_COPY_AND_ASSIGN(HpackHeaderTable);
};

}  // namespace net

#endif  // NET_SPDY_HPACK_HEADER_TABLE_H_
