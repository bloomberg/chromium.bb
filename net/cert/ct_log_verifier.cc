// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include "base/logging.h"
#include "net/cert/ct_log_verifier_util.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/merkle_consistency_proof.h"
#include "net/cert/signed_tree_head.h"

namespace net {

// static
scoped_refptr<const CTLogVerifier> CTLogVerifier::Create(
    const base::StringPiece& public_key,
    const base::StringPiece& description,
    const base::StringPiece& url) {
  GURL log_url(url.as_string());
  if (!log_url.is_valid())
    return nullptr;
  scoped_refptr<CTLogVerifier> result(new CTLogVerifier(description, log_url));
  if (!result->Init(public_key))
    return nullptr;
  return result;
}

CTLogVerifier::CTLogVerifier(const base::StringPiece& description,
                             const GURL& url)
    : description_(description.as_string()),
      url_(url),
      hash_algorithm_(ct::DigitallySigned::HASH_ALGO_NONE),
      signature_algorithm_(ct::DigitallySigned::SIG_ALGO_ANONYMOUS),
      public_key_(NULL) {
  DCHECK(url_.is_valid());
}

bool CTLogVerifier::Verify(const ct::LogEntry& entry,
                           const ct::SignedCertificateTimestamp& sct) const {
  if (sct.log_id != key_id()) {
    DVLOG(1) << "SCT is not signed by this log.";
    return false;
  }

  if (!SignatureParametersMatch(sct.signature))
    return false;

  std::string serialized_log_entry;
  if (!ct::EncodeLogEntry(entry, &serialized_log_entry)) {
    DVLOG(1) << "Unable to serialize entry.";
    return false;
  }
  std::string serialized_data;
  if (!ct::EncodeV1SCTSignedData(sct.timestamp, serialized_log_entry,
                                 sct.extensions, &serialized_data)) {
    DVLOG(1) << "Unable to create SCT to verify.";
    return false;
  }

  return VerifySignature(serialized_data, sct.signature.signature_data);
}

bool CTLogVerifier::VerifySignedTreeHead(
    const ct::SignedTreeHead& signed_tree_head) const {
  if (!SignatureParametersMatch(signed_tree_head.signature))
    return false;

  std::string serialized_data;
  ct::EncodeTreeHeadSignature(signed_tree_head, &serialized_data);
  if (VerifySignature(serialized_data,
                      signed_tree_head.signature.signature_data)) {
    return true;
  }
  return false;
}

bool CTLogVerifier::SignatureParametersMatch(
    const ct::DigitallySigned& signature) const {
  if (!signature.SignatureParametersMatch(hash_algorithm_,
                                          signature_algorithm_)) {
    DVLOG(1) << "Mismatched hash or signature algorithm. Hash: "
             << hash_algorithm_ << " vs " << signature.hash_algorithm
             << " Signature: " << signature_algorithm_ << " vs "
             << signature.signature_algorithm << ".";
    return false;
  }

  return true;
}

namespace {

// Indicates whether the node value it accompanies should be
// concatenated on the left or right before hashing.
enum HashOrientation { ORIENTATION_LEFT, ORIENTATION_RIGHT };

// The consistency proof described in RFC6962 section 2.1.2 is the (minimal)
// set of intermediate nodes sufficient to verify the Merkle Tree Hash (MTH)
// of the new tree.
// From the traversal of the consistency proof two vectors are derived:
// One of nodes that prove commitment to the old tree head hash and one of
// nodes that prove commitment to the new tree head hash.
//
// This function calculates a root hash from such a vector by keeping a
// 'cumulative hash' which is a root hash for a subtree of the entire tree.
//
// The traversal of the consistency proof provides not only the nodes but also
// the orientation of each node (whether it should be appended to the left or
// the right of the 'cumulative hash' before hashing the pair), which is the
// second item in the pair.
//
// The 'cumulative hash' is initialized with |first_hash|.
//
// Returns the hash calculated from |first_hash| and |hashes|.
std::string CalculateRootHashFromNodes(
    const std::vector<std::pair<base::StringPiece, HashOrientation>>& hashes,
    base::StringPiece first_hash) {
  std::string calculated_root_hash(first_hash.as_string());
  // Skip the last element as it was used to initialize calculated_root_hash
  for (auto it = hashes.rbegin(); it != hashes.rend(); ++it) {
    if (it->second == ORIENTATION_LEFT) {
      calculated_root_hash =
          ct::internal::HashNodes(it->first.as_string(), calculated_root_hash);
    } else {
      calculated_root_hash =
          ct::internal::HashNodes(calculated_root_hash, it->first.as_string());
    }
  }

  return calculated_root_hash;
}

}  // namespace

bool CTLogVerifier::VerifyConsistencyProof(
    const ct::MerkleConsistencyProof& proof,
    const std::string& old_tree_hash,
    const std::string& new_tree_hash) const {
  // Proof does not originate from this log.
  if (key_id_ != proof.log_id)
    return false;

  // Cannot prove consistency from a tree of a certain size to a tree smaller
  // than that - only the other way around.
  if (proof.first_tree_size > proof.second_tree_size)
    return false;

  // If the proof is between trees of the same size, then the 'proof'
  // is really just a statement that the tree hasn't changed. If this
  // is the case, there should be no proof nodes, and both the old
  // and new hash should be equivalent.
  //
  // In RFC 6962, Section 2.1.2, this is
  // PROOF(m, D[n]) = SUBPROOF(m, D[n], true)
  // SUBPROOF(m, D[m], true) = {}
  //
  // If m == n, then
  // PROOF(m, D[m]) = {}
  if (proof.first_tree_size == proof.second_tree_size)
    return proof.nodes.empty() && old_tree_hash == new_tree_hash;

  // It is possible to call this method to prove consistency between the
  // initial state of a log (i.e. an empty tree) and a later root. In that
  // case, the only valid proof is an empty proof.
  if (proof.first_tree_size == 0)
    return proof.nodes.empty();

  // Implement the algorithm outlined in
  // http://tools.ietf.org/html/rfc6962#section-2.1.2
  // to identify where each proof node belongs to, in the old and new
  // tree hashes.
  uint64_t m = proof.first_tree_size;
  uint64_t n = proof.second_tree_size;
  auto proof_back(proof.nodes.rbegin());

  // Vectors to hold hashes from the proof used to calculate old and new tree
  // hashes that the proof proves consistency for.
  // The consistency proof is a list of nodes required to verify that the
  // first |proof.first_tree_size| entries are identical in both trees, and
  // that |new_tree_hash| consists of these entries and new entries.
  std::vector<std::pair<base::StringPiece, HashOrientation>>
      new_tree_head_hashes;
  std::vector<std::pair<base::StringPiece, HashOrientation>>
      old_tree_head_hashes;

  // is_complete_subtree is true while |m| has not been split into two
  // subtrees. It corresponds to the third parameter of SUBPROOF in RFC
  // 6962, 2.1.2.
  bool is_complete_subtree = true;
  // This loop reverses the proof assembly process as described in
  // http://tools.ietf.org/html/rfc6962#section-2.1.2
  // A consistency proof between trees of size m and n is a list of nodes
  // that proves the following:
  // (1) The first m inputs in both trees are identical. This is proven by
  // providing nodes that would allow calculation of MTH(m) (to prove
  // it's the same as the old tree hash) and participate in the calculation
  // of MTH(n) (to prove they are a part of MTH(n)).
  // (2) Inputs m to n-1 are in MTH(n). This is proven by providing nodes that
  // complement the MTH(n) calculation in addition to the nodes that were
  // provided in (1).
  //
  // The loop iterates over all provided proof nodes; Each proof node will
  // be placed into either |new_tree_head_hashes|, |old_tree_head_hashes| or
  // both, depending on the case (detailed below).
  while (m != n) {
    // Incomplete proof - ran out of proof nodes before reaching
    // termination condition.
    if (proof_back == proof.nodes.rend())
      return false;

    uint64_t k = ct::internal::CalculateNearestPowerOfTwo(n);
    if (m <= k) {
      // This case in the RFC:
      //   SUBPROOF(m, D[n], b) = SUBPROOF(m, D[0:k], b) : MTH(D[k:n])
      // The right subtree entries (entries k to n-1) are only in the new tree.
      // The node pointed by |proof_back| is the Merkle tree head for the right
      // subtree (entries k to n-1) and only participates in the calculation
      // of the new tree root hash.
      new_tree_head_hashes.push_back(
          // Explicitly cast to StringPiece otherwise the pair created will
          // be <string, HashOrientation> and the copied string will be used
          // by the StringPiece, leading to use-after-free.
          std::make_pair(base::StringPiece(*proof_back), ORIENTATION_RIGHT));
      // Nothing to add to old_tree_head_hashes as the right subtree is outside
      // the old tree.
      ++proof_back;
      // m remains the same
      n = k;
    } else {
      DCHECK_GT(m, k);
      // This case in the RFC:
      //   SUBPROOF(m, D[n], b) = SUBPROOF(m - k, D[k:n], false) : MTH(D[0:k])
      // The left subtree entries (Entries 0 to k-1) should be identical in
      // both trees, so the node pointed to by |proof_back| is the Merkle tree
      // head for that subtree and will be used in hash calculation for
      // both the old STH and new STH.
      base::StringPiece curr_node(*proof_back);
      ++proof_back;

      new_tree_head_hashes.push_back(
          std::make_pair(curr_node, ORIENTATION_LEFT));
      old_tree_head_hashes.push_back(
          std::make_pair(curr_node, ORIENTATION_LEFT));
      // Continue traversal on the right-hand side of the tree (entries k to
      // n-1)
      m = m - k;
      n = n - k;
      is_complete_subtree = false;
    }
  }  // while loop

  DCHECK_EQ(m, n);
  base::StringPiece new_tree_leaf_hash;
  if (is_complete_subtree) {
    // This case in the RFC:
    //   SUBPROOF(m, D[m], true) = {}
    // |is_complete_subtree| being true at the final iteration implies m
    // was never greater than k (m > k was never true), so the tree
    // denoted by m was a complete subtree of the tree denoted by n.

    // If the old tree was a complete subtree of the new tree then the old
    // root hash is included in the new root hash calculation to prove the
    // new tree is derived from the old tree and new entries.
    new_tree_leaf_hash = old_tree_hash;
    DCHECK(old_tree_head_hashes.empty());
  } else {
    // This case in the RFC:
    //   SUBPROOF(m, D[m], false) = {MTH(D[m])}
    // The old tree is not a complete subtree of the new tree. The last
    // proof node is the root hash of the full Merkle tree closest
    // in size to m and n.

    // There should be exactly one more proof node.
    if (proof_back == proof.nodes.rend())
      return false;
    // The root hash of the full Merkle tree closest in size to m and n
    // should be used as the initial value for calculating root hashes
    // both for the new and old trees.
    new_tree_leaf_hash = *proof_back;
    // Assemble the old root hash and compare with the expected one. Only
    // done if the old tree was not a complete subtree of the new tree.
    if (CalculateRootHashFromNodes(old_tree_head_hashes, *proof_back) !=
        old_tree_hash) {
      return false;
    }
    // Advance |proof_back| so it should point to the end of the proof nodes
    // vector. Facilitates checking that there are no redundant proof nodes.
    ++proof_back;
  }

  // Assemble the new root hash and compare with the expected one.
  if (CalculateRootHashFromNodes(new_tree_head_hashes, new_tree_leaf_hash) !=
      new_tree_hash) {
    return false;
  }

  // Fail if there are more proof nodes than necessary.
  if (proof_back != proof.nodes.rend())
    return false;

  return true;
}

}  // namespace net
