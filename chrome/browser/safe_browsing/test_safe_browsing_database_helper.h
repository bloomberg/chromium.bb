// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_DATABASE_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_DATABASE_HELPER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/safe_browsing_db/util.h"

namespace safe_browsing {
class ListIdentifier;
class TestSafeBrowsingServiceFactory;
class TestV4GetHashProtocolManagerFactory;
}  // namespace safe_browsing

class InsertingDatabaseFactory;
class GURL;

// This class wraps a couple of safe browsing utilities that enable updating
// underlying SafeBrowsing lists to match URLs. It optionally takes a list of
// ListIdentifiers to add lists which are not normally enabled. This is used for
// e.g. the SubresourceFilter list to allow browser tests to test the list
// without building a chrome branded build.
class TestSafeBrowsingDatabaseHelper {
 public:
  TestSafeBrowsingDatabaseHelper();
  TestSafeBrowsingDatabaseHelper(
      std::vector<safe_browsing::ListIdentifier> lists_to_insert);
  ~TestSafeBrowsingDatabaseHelper();

  void MarkUrlAsMatchingListWithId(
      const GURL& bad_url,
      const safe_browsing::ListIdentifier& list_id,
      safe_browsing::ThreatPatternType threat_pattern_type);

  bool HasListSynced(const safe_browsing::ListIdentifier& list_id);

 private:
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory> sb_factory_;
  // Owned by the V4Database.
  InsertingDatabaseFactory* v4_db_factory_;
  // Owned by the V4GetHashProtocolManager.
  safe_browsing::TestV4GetHashProtocolManagerFactory* v4_get_hash_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestSafeBrowsingDatabaseHelper);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_DATABASE_HELPER_H_
