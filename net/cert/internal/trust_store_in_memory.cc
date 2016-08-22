// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_in_memory.h"

namespace net {

TrustStoreInMemory::TrustStoreInMemory() = default;
TrustStoreInMemory::~TrustStoreInMemory() = default;

void TrustStoreInMemory::Clear() {
  anchors_.clear();
}

void TrustStoreInMemory::AddTrustAnchor(scoped_refptr<TrustAnchor> anchor) {
  // TODO(mattm): should this check for duplicate anchors?
  anchors_.insert(std::make_pair(anchor->normalized_subject().AsStringPiece(),
                                 std::move(anchor)));
}

void TrustStoreInMemory::FindTrustAnchorsByNormalizedName(
    const der::Input& normalized_name,
    TrustAnchors* matches) const {
  auto range = anchors_.equal_range(normalized_name.AsStringPiece());
  for (auto it = range.first; it != range.second; ++it)
    matches->push_back(it->second);
}

}  // namespace net
