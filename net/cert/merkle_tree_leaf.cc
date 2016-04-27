// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/merkle_tree_leaf.h"

#include "net/cert/ct_objects_extractor.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace ct {

MerkleTreeLeaf::MerkleTreeLeaf() {}

MerkleTreeLeaf::~MerkleTreeLeaf() {}

bool GetMerkleTreeLeaf(const X509Certificate* cert,
                       const SignedCertificateTimestamp* sct,
                       MerkleTreeLeaf* merkle_tree_leaf) {
  if (sct->origin == SignedCertificateTimestamp::SCT_EMBEDDED) {
    if (cert->GetIntermediateCertificates().empty() ||
        !GetPrecertLogEntry(cert->os_cert_handle(),
                            cert->GetIntermediateCertificates().front(),
                            &merkle_tree_leaf->log_entry)) {
      return false;
    }
  } else {
    if (!GetX509LogEntry(cert->os_cert_handle(),
                         &merkle_tree_leaf->log_entry)) {
      return false;
    }
  }

  merkle_tree_leaf->log_id = sct->log_id;
  merkle_tree_leaf->timestamp = sct->timestamp;
  merkle_tree_leaf->extensions = sct->extensions;
  return true;
}

}  // namespace ct

}  // namespace net
