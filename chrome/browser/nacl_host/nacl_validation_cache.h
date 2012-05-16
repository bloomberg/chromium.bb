// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_VALIDATION_CACHE_H_
#define CHROME_BROWSER_NACL_HOST_NACL_VALIDATION_CACHE_H_
#pragma once

#include "base/memory/mru_cache.h"

class Pickle;

class NaClValidationCache {
 public:
  NaClValidationCache();
  ~NaClValidationCache();

  // Get the key used for HMACing validation signatures.  This should be a
  // string of cryptographically secure random bytes.
  const std::string& GetValidationCacheKey() const {
    return validation_cache_key_;
  }

  // Is the validation signature in the database?
  bool QueryKnownToValidate(const std::string& signature);

  // Put the validation signature in the database.
  void SetKnownToValidate(const std::string& signature);

  // Should the cache be used?
  bool IsEnabled() {
    return enabled_;
  }

 private:
  typedef base::HashingMRUCache<std::string, bool> ValidationCacheType;
  ValidationCacheType validation_cache_;

  std::string validation_cache_key_;

  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(NaClValidationCache);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_VALIDATION_CACHE_H_
