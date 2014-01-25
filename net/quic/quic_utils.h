// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic

#ifndef NET_QUIC_QUIC_UTILS_H_
#define NET_QUIC_QUIC_UTILS_H_

#include "net/base/int128.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"

namespace net {

//
// Find*()
//

// Returns a const reference to the value associated with the given key if it
// exists. Crashes otherwise.
//
// This is intended as a replacement for operator[] as an rvalue (for reading)
// when the key is guaranteed to exist.
//
// operator[] for lookup is discouraged for several reasons:
//  * It has a side-effect of inserting missing keys
//  * It is not thread-safe (even when it is not inserting, it can still
//      choose to resize the underlying storage)
//  * It invalidates iterators (when it chooses to resize)
//  * It default constructs a value object even if it doesn't need to
//
// This version assumes the key is printable, and includes it in the fatal log
// message.
template <class Collection>
const typename Collection::value_type::second_type&
FindOrDie(const Collection& collection,
          const typename Collection::value_type::first_type& key) {
  typename Collection::const_iterator it = collection.find(key);
  CHECK(it != collection.end()) << "Map key not found: " << key;
  return it->second;
}

// Same as above, but returns a non-const reference.
template <class Collection>
typename Collection::value_type::second_type&
FindOrDie(Collection& collection,  // NOLINT
          const typename Collection::value_type::first_type& key) {
  typename Collection::iterator it = collection.find(key);
  CHECK(it != collection.end()) << "Map key not found: " << key;
  return it->second;
}

// Returns a pointer to the const value associated with the given key if it
// exists, or NULL otherwise.
template <class Collection>
const typename Collection::value_type::second_type*
FindOrNull(const Collection& collection,
           const typename Collection::value_type::first_type& key) {
  typename Collection::const_iterator it = collection.find(key);
  if (it == collection.end()) {
    return 0;
  }
  return &it->second;
}

// Same as above but returns a pointer to the non-const value.
template <class Collection>
typename Collection::value_type::second_type*
FindOrNull(Collection& collection,  // NOLINT
           const typename Collection::value_type::first_type& key) {
  typename Collection::iterator it = collection.find(key);
  if (it == collection.end()) {
    return 0;
  }
  return &it->second;
}

class NET_EXPORT_PRIVATE QuicUtils {
 public:
  enum Priority {
    LOCAL_PRIORITY,
    PEER_PRIORITY,
  };

  // returns the 64 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static uint64 FNV1a_64_Hash(const char* data, int len);

  // returns the 128 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static uint128 FNV1a_128_Hash(const char* data, int len);

  // FindMutualTag sets |out_result| to the first tag in the priority list that
  // is also in the other list and returns true. If there is no intersection it
  // returns false.
  //
  // Which list has priority is determined by |priority|.
  //
  // If |out_index| is non-NULL and a match is found then the index of that
  // match in |their_tags| is written to |out_index|.
  static bool FindMutualTag(const QuicTagVector& our_tags,
                            const QuicTag* their_tags,
                            size_t num_their_tags,
                            Priority priority,
                            QuicTag* out_result,
                            size_t* out_index);

  // SerializeUint128 writes |v| in little-endian form to |out|.
  static void SerializeUint128(uint128 v, uint8* out);

  // SerializeUint128 writes the first 96 bits of |v| in little-endian form
  // to |out|.
  static void SerializeUint128Short(uint128 v, uint8* out);

  // Returns the name of the QuicRstStreamErrorCode as a char*
  static const char* StreamErrorToString(QuicRstStreamErrorCode error);

  // Returns the name of the QuicErrorCode as a char*
  static const char* ErrorToString(QuicErrorCode error);

  // Returns the level of encryption as a char*
  static const char* EncryptionLevelToString(EncryptionLevel level);

  // TagToString is a utility function for pretty-printing handshake messages
  // that converts a tag to a string. It will try to maintain the human friendly
  // name if possible (i.e. kABCD -> "ABCD"), or will just treat it as a number
  // if not.
  static std::string TagToString(QuicTag tag);

  // Given a binary buffer, return a hex+ASCII dump in the style of
  // tcpdump's -X and -XX options:
  // "0x0000:  0090 69bd 5400 000d 610f 0189 0800 4500  ..i.T...a.....E.\n"
  // "0x0010:  001c fb98 4000 4001 7e18 d8ef 2301 455d  ....@.@.~...#.E]\n"
  // "0x0020:  7fe2 0800 6bcb 0bc6 806e                 ....k....n\n"
  static std::string StringToHexASCIIDump(base::StringPiece in_buffer);

  static char* AsChars(unsigned char* data) {
    return reinterpret_cast<char*>(data);
  }

  static QuicPriority LowestPriority();

  static QuicPriority HighestPriority();
};

// Utility function that returns an IOVector object wrapped around |str|.
inline IOVector MakeIOVector(base::StringPiece str) {
  IOVector iov;
  iov.Append(const_cast<char*>(str.data()), str.size());
  return iov;
}

}  // namespace net

#endif  // NET_QUIC_QUIC_UTILS_H_
