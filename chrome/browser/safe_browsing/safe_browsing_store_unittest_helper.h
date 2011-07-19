// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_UNITTEST_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_UNITTEST_HELPER_H_
#pragma once

#include "chrome/browser/safe_browsing/safe_browsing_store.h"

#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

// Helper code for testing that a SafeBrowsingStore implementation
// works to spec.

// Helper to make it easy to initialize SBFullHash constants.
inline const SBFullHash SBFullHashFromString(const char* str) {
  SBFullHash h;
  crypto::SHA256HashString(str, &h.full_hash, sizeof(h.full_hash));
  return h;
}

// TODO(shess): There's an == operator defined in
// safe_browsing_utils.h, but using it gives me the heebie-jeebies.
inline bool SBFullHashEq(const SBFullHash& a, const SBFullHash& b) {
  return !memcmp(a.full_hash, b.full_hash, sizeof(a.full_hash));
}

// Test that the empty store looks empty.
void SafeBrowsingStoreTestEmpty(SafeBrowsingStore* store);

// Write some prefix data to the store and verify that it looks like
// it is still there after the transaction completes.
void SafeBrowsingStoreTestStorePrefix(SafeBrowsingStore* store);

// Test that subs knockout adds.
void SafeBrowsingStoreTestSubKnockout(SafeBrowsingStore* store);

// Test that deletes delete the chunk's data.
void SafeBrowsingStoreTestDeleteChunks(SafeBrowsingStore* store);

// Test that deleting the store deletes the store.
void SafeBrowsingStoreTestDelete(SafeBrowsingStore* store,
                                 const FilePath& filename);

// Wrap all the tests up for implementation subclasses.
// |test_fixture| is the class that would be passed to TEST_F(),
// |instance_name| is the name of the SafeBrowsingStore instance
// within the class, as a pointer, and |filename| is that store's
// filename, for the Delete() test.
#define TEST_STORE(test_fixture, instance_name, filename)        \
  TEST_F(test_fixture, Empty) { \
    SafeBrowsingStoreTestEmpty(instance_name); \
  } \
  TEST_F(test_fixture, StorePrefix) { \
    SafeBrowsingStoreTestStorePrefix(instance_name); \
  } \
  TEST_F(test_fixture, SubKnockout) { \
    SafeBrowsingStoreTestSubKnockout(instance_name); \
  } \
  TEST_F(test_fixture, DeleteChunks) { \
    SafeBrowsingStoreTestDeleteChunks(instance_name); \
  } \
  TEST_F(test_fixture, Delete) { \
    SafeBrowsingStoreTestDelete(instance_name, filename);        \
  }

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_UNITTEST_HELPER_H_
