// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/hashed_ad_network_database.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// We use a hash size of 8 for these for three reasons.
// 1. It saves us a bit on space, and, since we have to store these in memory
//    (reading from disk would be far too slow because these checks are
//    performed synchronously), that space is important.
// 2. Since we don't store full hashes, reconstructing the list is more
//    difficult. This may mean we get a few incorrect hits, but the security is
//    worth the (very small) amount of noise.
// 3. It fits nicely into a int64.
const size_t kUrlHashSize = 8u;
COMPILE_ASSERT(kUrlHashSize <= sizeof(int64), url_hashes_must_fit_into_a_int64);

const size_t kChecksumHashSize = 32u;

}  // namespace

HashedAdNetworkDatabase::HashedAdNetworkDatabase(
    scoped_refptr<base::RefCountedStaticMemory> entries_memory)
    : is_valid_(false) {
  // This can legitimately happen in unit tests.
  if (!entries_memory)
    return;

  const size_t size = entries_memory->size();
  const unsigned char* const front = entries_memory->front();
  if (size < kChecksumHashSize ||
      (size - kChecksumHashSize) % kUrlHashSize != 0) {
    NOTREACHED();
    return;
  }

  // The format of the data resource is fairly straight-forward:
  // <32-bit checksum><list of 64-bit hashes of hosts>, with no linebreaks or
  // other separations.
  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hash->Update(front + kChecksumHashSize, size - kChecksumHashSize);
  char hash_value[kChecksumHashSize];
  hash->Finish(hash_value, kChecksumHashSize);
  // If the checksum doesn't match, abort.
  if (memcmp(hash_value, front, kChecksumHashSize) != 0) {
    NOTREACHED();
    return;
  }

  // Construct and insert all hashes.
  for (const unsigned char* index = front + kChecksumHashSize;
       index < front + size;
       index += kUrlHashSize) {
    int64 value = 0;
    memcpy(&value, index, kUrlHashSize);
    entries_.insert(value);
  }

  is_valid_ = true;
}

HashedAdNetworkDatabase::~HashedAdNetworkDatabase() {}

bool HashedAdNetworkDatabase::IsAdNetwork(const GURL& url) const {
  int64 hash = 0;
  crypto::SHA256HashString(url.host(), &hash, sizeof(hash));
  // If initialization failed (most likely because this is a unittest), then
  // |entries_| is never populated and we are guaranteed to return false - which
  // is desired default behavior.
  return entries_.count(hash) != 0;
}

}  // namespace extensions
