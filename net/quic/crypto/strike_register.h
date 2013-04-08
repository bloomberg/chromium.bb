// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_STRIKE_REGISTER_H_
#define NET_QUIC_CRYPTO_STRIKE_REGISTER_H_

#include <set>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

// A StrikeRegister is critbit tree which stores a set of observed nonces.
// We use a critbit tree because:
//   1) It's immune to algorithmic complexity attacks. If we had used a hash
//      tree, an attacker could send us a series of values which stretch out one
//      of the hash chains, causing us to do much more work than normal.
//   2) We can write it to use a fixed block of memory: avoiding fragmentation
//      issues and so forth. (We might be able to do that with the STL
//      algorithms and a custom allocator, but I don't want to go there.)
//   3) It's simple (compared to balanced binary trees) and doesn't involve
//      bouncing nearly as many cache lines around.
//   4) It allows us to query for the oldest element in log(n) time.
//
// This code is based on djb's public domain critbit tree from qhasm.
//
// A critbit tree has external and internal nodes. External nodes are just the
// nonce values (which are stored without the orbit values included). Internal
// nodes contain the bit number at which the tree is branching and exactly two
// children. The critical bit is stored as a byte number and a byte
// (|otherbits|) which has all the bits set /except/ the one in question.
//
// Internal nodes have exactly two children: an internal node with only a
// single child would be useless.
//
// The branching bit number (considering the MSB to be the 1st bit) is
// monotonically increasing as you go down the tree.
class NET_EXPORT_PRIVATE StrikeRegister {
 public:
  // An external node takes 24 bytes as we don't record the orbit.
  static const uint32 kExternalNodeSize;

  // We address the nodes by their index in the array. This means that 0 is a
  // valid index. Therefore this is our invalid index. It also has a one bit
  // in the LSB position because we tend to store indexes shifted up 8 bits
  // and this distinguishes kNil from (kExternalFlag | 0) << 8.
  static const uint32  kNil;

  // Our pointers from internal nodes can either point to an internal or
  // external node. We flag the 24th bit to mark a pointer as external.
  static const uint32 kExternalFlag;

  // Construct a new set which can hold, at most, |max_entries| (which must be
  // less than 2**23). Once constructed, all nonces created before
  // |current_time| (in seconds) will be rejected. In the future, all nonces
  // that are outside +/- |window_secs| from the current time will be rejected.
  // Additionally, all nonces that have an orbit value other than |orbit| will
  // be rejected.
  //
  // (Note that this code is independent of the actual units of time used, but
  // you should use seconds.)
  StrikeRegister(unsigned max_entries,
                 uint32 current_time,
                 uint32 window_secs,
                 const uint8 orbit[8]);

  ~StrikeRegister();

  void Reset();

  // |Insert| queries to see if |nonce| is
  //   a) for the wrong orbit
  //   b) before the current horizon
  //   c) outside of the valid time window
  //   d) already in the set of observed nonces
  // and returns false if any of these are true. It is also free to return
  // false for other reasons as it's always safe to reject an nonce.
  //
  // nonces are:
  //   4 bytes of timestamp (UNIX epoch seconds)
  //   8 bytes of orbit value (a cluster id)
  //   20 bytes of random data
  //
  // Otherwise, it inserts |nonce| into the observed set and returns true.
  bool Insert(const uint8 nonce[32], const uint32 current_time);

  // This is a debugging aid which checks the tree for sanity.
  void Validate();

 private:
  class InternalNode;

  // TimeFromBytes returns a big-endian uint32 from |d|.
  static uint32 TimeFromBytes(const uint8 d[4]);

  // BestMatch returns either kNil, or an external node index which could
  // possibly match |v|.
  uint32 BestMatch(const uint8 v[24]) const;

  // external_node_next_ptr returns the 'next' pointer embedded in external
  // node |i|. This is used to thread a free list through the external nodes.
  uint32& external_node_next_ptr(unsigned i);

  uint8* external_node(unsigned i);

  uint32 GetFreeExternalNode();

  uint32 GetFreeInternalNode();

  // DropNode removes the oldest node in the tree and updates |horizon_|
  // accordingly.
  void DropNode();

  void FreeExternalNode(uint32 index);

  void FreeInternalNode(uint32 index);

  void ValidateTree(uint32 internal_node,
                    int last_bit,
                    const std::vector<std::pair<unsigned, bool> >& bits,
                    const std::set<uint32>& free_internal_nodes,
                    const std::set<uint32>& free_external_nodes,
                    std::set<uint32>* used_internal_nodes,
                    std::set<uint32>* used_external_nodes);

  const uint32 max_entries_;
  const uint32 window_secs_;
  uint8 orbit_[8];
  uint32 horizon_;

  uint32 internal_node_free_head_;
  uint32 external_node_free_head_;
  uint32 internal_node_head_;
  // internal_nodes_ can't be a scoped_ptr because the type isn't defined in
  // this header.
  InternalNode* internal_nodes_;
  scoped_ptr<uint8[]> external_nodes_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_STRIKE_REGISTER_H_
