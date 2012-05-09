// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_validation_cache.h"

#include "base/metrics/histogram.h"
#include "base/rand_util.h"

namespace {

// For the moment, choose an arbitrary cache size.
const size_t kValidationCacheCacheSize = 200;
// Key size is equal to the block size (not the digest size) of SHA256.
const size_t kValidationCacheKeySize = 64;
// Entry size is equal to the digest size of SHA256.
const size_t kValidationCacheEntrySize = 32;

enum ValidationCacheStatus {
  CACHE_MISS = 0,
  CACHE_HIT,
  CACHE_ERROR,
  CACHE_MAX
};

void LogCacheQuery(ValidationCacheStatus status) {
  UMA_HISTOGRAM_ENUMERATION("NaCl.ValidationCache.Query", status, CACHE_MAX);
}

void LogCacheSet(ValidationCacheStatus status) {
  // Bucket zero is reserved for future use.
  UMA_HISTOGRAM_ENUMERATION("NaCl.ValidationCache.Set", status, CACHE_MAX);
}

}  // namespace

NaClValidationCache::NaClValidationCache()
    : validation_cache_(kValidationCacheCacheSize),
      validation_cache_key_(base::RandBytesAsString(kValidationCacheKeySize)){
}

NaClValidationCache::~NaClValidationCache() {
  // Make clang's style checking happy by adding a destructor.
}

bool NaClValidationCache::QueryKnownToValidate(const std::string& signature) {
  bool result = false;
  if (signature.length() != kValidationCacheEntrySize) {
    // Signature is the wrong size, something is wrong.
    LogCacheQuery(CACHE_ERROR);
  } else {
    ValidationCacheType::iterator iter = validation_cache_.Get(signature);
    if (iter != validation_cache_.end()) {
      result = iter->second;
    }
    LogCacheQuery(result ? CACHE_HIT : CACHE_MISS);
  }
  return result;
}

void NaClValidationCache::SetKnownToValidate(const std::string& signature) {
  if (signature.length() != kValidationCacheEntrySize) {
    // Signature is the wrong size, something is wrong.
    LogCacheSet(CACHE_ERROR);
  } else {
    validation_cache_.Put(signature, true);
    // The number of sets should be equal to the number of cache misses, minus
    // validation failures and successful validations where stubout occurs.
    LogCacheSet(CACHE_HIT);
  }
}
