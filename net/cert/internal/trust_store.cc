// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store.h"

#include "base/memory/ptr_util.h"

namespace net {

scoped_refptr<TrustAnchor> TrustAnchor::CreateFromCertificateNoConstraints(
    scoped_refptr<ParsedCertificate> cert) {
  return scoped_refptr<TrustAnchor>(new TrustAnchor(std::move(cert)));
}

der::Input TrustAnchor::spki() const {
  return cert_->tbs().spki_tlv;
}

der::Input TrustAnchor::normalized_subject() const {
  return cert_->normalized_subject();
}

const scoped_refptr<ParsedCertificate>& TrustAnchor::cert() const {
  return cert_;
}

TrustAnchor::TrustAnchor(scoped_refptr<ParsedCertificate> cert)
    : cert_(std::move(cert)) {
  DCHECK(cert_);
}

TrustAnchor::~TrustAnchor() {}

TrustStore::TrustStore() {}
TrustStore::~TrustStore() {}

void TrustStore::Clear() {
  anchors_.clear();
}

void TrustStore::AddTrustAnchor(scoped_refptr<TrustAnchor> anchor) {
  // TODO(mattm): should this check for duplicate anchors?
  anchors_.insert(std::make_pair(anchor->normalized_subject().AsStringPiece(),
                                 std::move(anchor)));
}

void TrustStore::FindTrustAnchorsByNormalizedName(
    const der::Input& normalized_name,
    TrustAnchors* matches) const {
  auto range = anchors_.equal_range(normalized_name.AsStringPiece());
  for (auto it = range.first; it != range.second; ++it)
    matches->push_back(it->second);
}

}  // namespace net
