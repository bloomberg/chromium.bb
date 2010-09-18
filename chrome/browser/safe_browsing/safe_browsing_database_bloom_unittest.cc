// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the SafeBrowsing storage system.

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_temp_dir.h"
#include "base/sha2.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_database_bloom.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_unittest_helper.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;

namespace {

SBPrefix Sha256Prefix(const std::string& str) {
  SBPrefix prefix;
  base::SHA256HashString(str, &prefix, sizeof(prefix));
  return prefix;
}

SBFullHash Sha256Hash(const std::string& str) {
  SBFullHash hash;
  base::SHA256HashString(str, &hash, sizeof(hash));
  return hash;
}

}  // namespace

class SafeBrowsingDatabaseBloomTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    // Setup a database in a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_.reset(new SafeBrowsingDatabaseBloom);
    database_filename_ =
        temp_dir_.path().AppendASCII("SafeBrowsingTestDatabase");
    database_->Init(database_filename_);
  }

  virtual void TearDown() {
    database_.reset();

    PlatformTest::TearDown();
  }

  void GetListsInfo(std::vector<SBListChunkRanges>* lists) {
    lists->clear();
    EXPECT_TRUE(database_->UpdateStarted(lists));
    database_->UpdateFinished(true);
  }

  // Helper function to do an AddDel or SubDel command.
  void DelChunk(const std::string& list,
                int chunk_id,
                bool is_sub_del) {
    std::vector<SBChunkDelete> deletes;
    SBChunkDelete chunk_delete;
    chunk_delete.list_name = list;
    chunk_delete.is_sub_del = is_sub_del;
    chunk_delete.chunk_del.push_back(ChunkRange(chunk_id));
    deletes.push_back(chunk_delete);
    database_->DeleteChunks(deletes);
  }

  void AddDelChunk(const std::string& list, int chunk_id) {
    DelChunk(list, chunk_id, false);
  }

  void SubDelChunk(const std::string& list, int chunk_id) {
    DelChunk(list, chunk_id, true);
  }

  // Utility function for setting up the database for the caching test.
  void PopulateDatabaseForCacheTest();

  scoped_ptr<SafeBrowsingDatabaseBloom> database_;
  FilePath database_filename_;
  ScopedTempDir temp_dir_;
};

// Tests retrieving list name information.
TEST_F(SafeBrowsingDatabaseBloomTest, ListName) {
  SBChunkList chunks;

  // Insert some malware add chunks.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/malware.html"));
  SBChunk chunk;
  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  std::vector<SBListChunkRanges> lists;
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  host.host = Sha256Prefix("www.foo.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.foo.com/malware.html"));
  chunk.chunk_number = 2;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  host.host = Sha256Prefix("www.whatever.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(3);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.whatever.com/malware.html"));
  chunk.chunk_number = 3;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());

  // Insert a malware sub chunk.
  host.host = Sha256Prefix("www.subbed.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(7);
  host.entry->SetChunkIdAtPrefix(0, 19);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.subbed.com/notevil1.html"));
  chunk.chunk_number = 7;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);

  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_EQ(lists[0].subs, "7");
  if (lists.size() == 2) {
    // Old style database won't have the second entry since it creates the lists
    // when it receives an update containing that list. The new bloom filter
    // based database has these values hard coded.
    EXPECT_TRUE(lists[1].name == safe_browsing_util::kPhishingList);
    EXPECT_TRUE(lists[1].adds.empty());
    EXPECT_TRUE(lists[1].subs.empty());
  }

  // Add a phishing add chunk.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(47);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  chunk.chunk_number = 47;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kPhishingList, chunks);

  // Insert some phishing sub chunks.
  host.host = Sha256Prefix("www.phishy.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(200);
  host.entry->SetChunkIdAtPrefix(0, 1999);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.phishy.com/notevil1.html"));
  chunk.chunk_number = 200;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  database_->InsertChunks(safe_browsing_util::kPhishingList, chunks);

  host.host = Sha256Prefix("www.phishy2.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(201);
  host.entry->SetChunkIdAtPrefix(0, 1999);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.phishy2.com/notevil1.html"));
  chunk.chunk_number = 201;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  database_->InsertChunks(safe_browsing_util::kPhishingList, chunks);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_EQ(lists[0].subs, "7");
  EXPECT_TRUE(lists[1].name == safe_browsing_util::kPhishingList);
  EXPECT_EQ(lists[1].adds, "47");
  EXPECT_EQ(lists[1].subs, "200-201");
}

// Checks database reading and writing.
TEST_F(SafeBrowsingDatabaseBloomTest, Database) {
  SBChunkList chunks;

  // Add a simple chunk with one hostkey.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/malware.html"));

  SBChunk chunk;
  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);
  std::vector<SBListChunkRanges> lists;
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  // Add another chunk with two different hostkeys.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/notevil2.html"));

  chunk.chunk_number = 2;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  host.host = Sha256Prefix("www.good.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.good.com/good1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.good.com/good2.html"));

  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);

  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  // and a chunk with an IP-based host
  host.host = Sha256Prefix("192.168.0.1/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(3);
  host.entry->SetPrefixAt(0, Sha256Prefix("192.168.0.1/malware.html"));

  chunk.chunk_number = 3;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  // Make sure they were added correctly.
  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());

  const Time now = Time::Now();
  std::vector<SBFullHashResult> full_hashes;
  std::vector<SBPrefix> prefix_hits;
  std::string matching_list;
  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));
  EXPECT_EQ(prefix_hits[0], Sha256Prefix("www.evil.com/phishing.html"));
  EXPECT_EQ(prefix_hits.size(), 1U);

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.evil.com/notevil1.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.good.com/good1.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.good.com/good2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://192.168.0.1/malware.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.evil.com/"),
                                      &matching_list, &prefix_hits,
                                      &full_hashes, now));
  EXPECT_TRUE(prefix_hits.empty());

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.evil.com/robots.txt"),
                                      &matching_list, &prefix_hits,
                                      &full_hashes, now));



  // Attempt to re-add the first chunk (should be a no-op).
  // see bug: http://code.google.com/p/chromium/issues/detail?id=4522
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/malware.html"));

  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1-3");
  EXPECT_TRUE(lists[0].subs.empty());


  // Test removing a single prefix from the add chunk.
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(2);
  host.entry->SetChunkIdAtPrefix(0, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));

  chunk.is_add = false;
  chunk.chunk_number = 4;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);

  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));
  EXPECT_EQ(prefix_hits[0], Sha256Prefix("www.evil.com/phishing.html"));
  EXPECT_EQ(prefix_hits.size(), 1U);

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.evil.com/notevil1.html"),
                                      &matching_list, &prefix_hits,
                                      &full_hashes, now));
  EXPECT_TRUE(prefix_hits.empty());

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.good.com/good1.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.good.com/good2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].subs, "4");

  // Test the same sub chunk again.  This should be a no-op.
  // see bug: http://code.google.com/p/chromium/issues/detail?id=4522
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(2);
  host.entry->SetChunkIdAtPrefix(0, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/notevil1.html"));

  chunk.is_add = false;
  chunk.chunk_number = 4;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);

  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].subs, "4");

  // Test removing all the prefixes from an add chunk.
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 2);
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.evil.com/notevil2.html"),
                                      &matching_list, &prefix_hits,
                                      &full_hashes, now));

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.good.com/good1.html"),
                                      &matching_list, &prefix_hits,
                                      &full_hashes, now));

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.good.com/good2.html"),
                                      &matching_list, &prefix_hits,
                                      &full_hashes, now));

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1,3");
  EXPECT_EQ(lists[0].subs, "4");

  // The adddel command exposed a bug in the transaction code where any
  // transaction after it would fail.  Add a dummy entry and remove it to
  // make sure the transcation works fine.
  host.host = Sha256Prefix("www.redherring.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.redherring.com/index.html"));

  chunk.is_add = true;
  chunk.chunk_number = 44;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);

  // Now remove the dummy entry.  If there are any problems with the
  // transactions, asserts will fire.
  AddDelChunk(safe_browsing_util::kMalwareList, 44);

  // Test the subdel command.
  SubDelChunk(safe_browsing_util::kMalwareList, 4);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_TRUE(lists[0].name == safe_browsing_util::kMalwareList);
  EXPECT_EQ(lists[0].adds, "1,3");
  EXPECT_EQ(lists[0].subs, "");

  // Test a sub command coming in before the add.
  host.host = Sha256Prefix("www.notevilanymore.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 2);
  host.entry->set_chunk_id(10);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notevilanymore.com/index.html"));
  host.entry->SetChunkIdAtPrefix(0, 10);
  host.entry->SetPrefixAt(1, Sha256Prefix("www.notevilanymore.com/good.html"));
  host.entry->SetChunkIdAtPrefix(1, 10);

  chunk.is_add = false;
  chunk.chunk_number = 5;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsUrl(
      GURL("http://www.notevilanymore.com/index.html"),
      &matching_list, &prefix_hits, &full_hashes, now));

  // Now insert the tardy add chunk.
  host.host = Sha256Prefix("www.notevilanymore.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notevilanymore.com/index.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.notevilanymore.com/good.html"));

  chunk.is_add = true;
  chunk.chunk_number = 10;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsUrl(
      GURL("http://www.notevilanymore.com/index.html"),
      &matching_list, &prefix_hits, &full_hashes, now));

  EXPECT_FALSE(database_->ContainsUrl(
      GURL("http://www.notevilanymore.com/good.html"),
      &matching_list, &prefix_hits, &full_hashes, now));
}


// Test adding zero length chunks to the database.
TEST_F(SafeBrowsingDatabaseBloomTest, ZeroSizeChunk) {
  SBChunkList chunks;

  // Populate with a couple of normal chunks.
  SBChunkHost host;
  host.host = Sha256Prefix("www.test.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.test.com/test1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.test.com/test2.html"));
  host.entry->set_chunk_id(1);

  SBChunk chunk;
  chunk.is_add = true;
  chunk.chunk_number = 1;
  chunk.hosts.push_back(host);

  chunks.clear();
  chunks.push_back(chunk);

  host.host = Sha256Prefix("www.random.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.random.com/random1.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.random.com/random2.html"));
  host.entry->set_chunk_id(10);
  chunk.chunk_number = 10;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.push_back(chunk);

  std::vector<SBListChunkRanges> lists;
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  // Add an empty ADD and SUB chunk.
  GetListsInfo(&lists);
  EXPECT_EQ(lists[0].adds, "1,10");

  SBChunk empty_chunk;
  empty_chunk.chunk_number = 19;
  empty_chunk.is_add = true;
  chunks.clear();
  chunks.push_back(empty_chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  chunks.clear();
  empty_chunk.chunk_number = 7;
  empty_chunk.is_add = false;
  chunks.push_back(empty_chunk);
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_EQ(lists[0].adds, "1,10,19");
  EXPECT_EQ(lists[0].subs, "7");

  // Add an empty chunk along with a couple that contain data. This should
  // result in the chunk range being reduced in size.
  host.host = Sha256Prefix("www.notempty.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notempty.com/full1.html"));
  host.entry->set_chunk_id(20);
  empty_chunk.chunk_number = 20;
  empty_chunk.is_add = true;
  empty_chunk.hosts.clear();
  empty_chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(empty_chunk);

  empty_chunk.chunk_number = 21;
  empty_chunk.is_add = true;
  empty_chunk.hosts.clear();
  chunks.push_back(empty_chunk);

  host.host = Sha256Prefix("www.notempty.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.notempty.com/full2.html"));
  host.entry->set_chunk_id(22);
  empty_chunk.hosts.clear();
  empty_chunk.hosts.push_back(host);
  empty_chunk.chunk_number = 22;
  empty_chunk.is_add = true;
  chunks.push_back(empty_chunk);

  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  const Time now = Time::Now();
  std::vector<SBFullHashResult> full_hashes;
  std::vector<SBPrefix> prefix_hits;
  std::string matching_list;
  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.notempty.com/full1.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));
  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.notempty.com/full2.html"),
                                     &matching_list, &prefix_hits,
                                     &full_hashes, now));

  GetListsInfo(&lists);
  EXPECT_EQ(lists[0].adds, "1,10,19-22");
  EXPECT_EQ(lists[0].subs, "7");

  // Handle AddDel and SubDel commands for empty chunks.
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 21);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_EQ(lists[0].adds, "1,10,19-20,22");
  EXPECT_EQ(lists[0].subs, "7");

  EXPECT_TRUE(database_->UpdateStarted(&lists));
  SubDelChunk(safe_browsing_util::kMalwareList, 7);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  EXPECT_EQ(lists[0].adds, "1,10,19-20,22");
  EXPECT_EQ(lists[0].subs, "");
}

// Utility function for setting up the database for the caching test.
void SafeBrowsingDatabaseBloomTest::PopulateDatabaseForCacheTest() {
  // Add a simple chunk with one hostkey and cache it.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_PREFIX, 2);
  host.entry->set_chunk_id(1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));
  host.entry->SetPrefixAt(1, Sha256Prefix("www.evil.com/malware.html"));

  SBChunk chunk;
  chunk.chunk_number = 1;
  chunk.is_add = true;
  chunk.hosts.push_back(host);

  SBChunkList chunks;
  std::vector<SBListChunkRanges> lists;
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  // Add the GetHash results to the cache.
  SBFullHashResult full_hash;
  full_hash.hash = Sha256Hash("www.evil.com/phishing.html");
  full_hash.list_name = safe_browsing_util::kMalwareList;
  full_hash.add_chunk_id = 1;

  std::vector<SBFullHashResult> results;
  results.push_back(full_hash);

  full_hash.hash = Sha256Hash("www.evil.com/malware.html");
  results.push_back(full_hash);

  std::vector<SBPrefix> prefixes;
  database_->CacheHashResults(prefixes, results);
}

TEST_F(SafeBrowsingDatabaseBloomTest, HashCaching) {
  PopulateDatabaseForCacheTest();

  // We should have both full hashes in the cache.
  SafeBrowsingDatabaseBloom::HashCache* hash_cache = database_->hash_cache();
  EXPECT_EQ(hash_cache->size(), 2U);

  // Test the cache lookup for the first prefix.
  std::string listname;
  std::vector<SBPrefix> prefixes;
  std::vector<SBFullHashResult> full_hashes;
  database_->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                         &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_TRUE(SBFullHashEq(full_hashes[0].hash,
                           Sha256Hash("www.evil.com/phishing.html")));

  prefixes.clear();
  full_hashes.clear();

  // Test the cache lookup for the second prefix.
  database_->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                         &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_TRUE(SBFullHashEq(full_hashes[0].hash,
                           Sha256Hash("www.evil.com/malware.html")));

  prefixes.clear();
  full_hashes.clear();

  // Test removing a prefix via a sub chunk.
  SBChunkHost host;
  host.host = Sha256Prefix("www.evil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_PREFIX, 1);
  host.entry->set_chunk_id(1);
  host.entry->SetChunkIdAtPrefix(0, 1);
  host.entry->SetPrefixAt(0, Sha256Prefix("www.evil.com/phishing.html"));

  SBChunk chunk;
  chunk.chunk_number = 2;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  SBChunkList chunks;
  chunks.push_back(chunk);

  std::vector<SBListChunkRanges> lists;
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  // This prefix should still be there.
  database_->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                         &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_TRUE(SBFullHashEq(full_hashes[0].hash,
                           Sha256Hash("www.evil.com/malware.html")));

  prefixes.clear();
  full_hashes.clear();

  // This prefix should be gone.
  database_->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                         &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_TRUE(full_hashes.empty());

  prefixes.clear();
  full_hashes.clear();

  // Test that an AddDel for the original chunk removes the last cached entry.
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 1);
  database_->UpdateFinished(true);
  database_->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                         &listname, &prefixes, &full_hashes, Time::Now());
  EXPECT_TRUE(full_hashes.empty());
  EXPECT_TRUE(database_->hash_cache()->empty());

  prefixes.clear();
  full_hashes.clear();

  // Test that the cache won't return expired values. First we have to adjust
  // the cached entries' received time to make them older, since the database
  // cache insert uses Time::Now(). First, store some entries.
  PopulateDatabaseForCacheTest();
  hash_cache = database_->hash_cache();
  EXPECT_EQ(hash_cache->size(), 2U);

  // Now adjust one of the entries times to be in the past.
  base::Time expired = base::Time::Now() - base::TimeDelta::FromMinutes(60);
  const SBPrefix key = Sha256Prefix("www.evil.com/malware.html");
  SafeBrowsingDatabaseBloom::HashList& entries = (*hash_cache)[key];
  SafeBrowsingDatabaseBloom::HashCacheEntry entry = entries.front();
  entries.pop_front();
  entry.received = expired;
  entries.push_back(entry);

  database_->ContainsUrl(GURL("http://www.evil.com/malware.html"),
                         &listname, &prefixes, &full_hashes, expired);
  EXPECT_TRUE(full_hashes.empty());

  // This entry should still exist.
  database_->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                         &listname, &prefixes, &full_hashes, expired);
  EXPECT_EQ(full_hashes.size(), 1U);


  // Testing prefix miss caching. First, we clear out the existing database,
  // Since PopulateDatabaseForCacheTest() doesn't handle adding duplicate
  // chunks.
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 1);
  database_->UpdateFinished(true);

  std::vector<SBPrefix> prefix_misses;
  std::vector<SBFullHashResult> empty_full_hash;
  prefix_misses.push_back(Sha256Prefix("http://www.bad.com/malware.html"));
  prefix_misses.push_back(Sha256Prefix("http://www.bad.com/phishing.html"));
  database_->CacheHashResults(prefix_misses, empty_full_hash);

  // Prefixes with no full results are misses.
  EXPECT_EQ(database_->prefix_miss_cache()->size(), 2U);

  // Update the database.
  PopulateDatabaseForCacheTest();

  // Prefix miss cache should be cleared.
  EXPECT_TRUE(database_->prefix_miss_cache()->empty());

  // Cache a GetHash miss for a particular prefix, and even though the prefix is
  // in the database, it is flagged as a miss so looking up the associated URL
  // will not succeed.
  prefixes.clear();
  full_hashes.clear();
  prefix_misses.clear();
  empty_full_hash.clear();
  prefix_misses.push_back(Sha256Prefix("www.evil.com/phishing.html"));
  database_->CacheHashResults(prefix_misses, empty_full_hash);
  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.evil.com/phishing.html"),
                                      &listname, &prefixes,
                                      &full_hashes, Time::Now()));

  prefixes.clear();
  full_hashes.clear();

  // Test receiving a full add chunk.
  host.host = Sha256Prefix("www.fullevil.com/");
  host.entry = SBEntry::Create(SBEntry::ADD_FULL_HASH, 2);
  host.entry->set_chunk_id(20);
  host.entry->SetFullHashAt(0, Sha256Hash("www.fullevil.com/bad1.html"));
  host.entry->SetFullHashAt(1, Sha256Hash("www.fullevil.com/bad2.html"));

  chunk.chunk_number = 20;
  chunk.is_add = true;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.fullevil.com/bad1.html"),
                                     &listname, &prefixes, &full_hashes,
                                     Time::Now()));
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_TRUE(SBFullHashEq(full_hashes[0].hash,
                           Sha256Hash("www.fullevil.com/bad1.html")));
  prefixes.clear();
  full_hashes.clear();

  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.fullevil.com/bad2.html"),
                                     &listname, &prefixes, &full_hashes,
                                     Time::Now()));
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_TRUE(SBFullHashEq(full_hashes[0].hash,
                           Sha256Hash("www.fullevil.com/bad2.html")));
  prefixes.clear();
  full_hashes.clear();

  // Test receiving a full sub chunk, which will remove one of the full adds.
  host.host = Sha256Prefix("www.fullevil.com/");
  host.entry = SBEntry::Create(SBEntry::SUB_FULL_HASH, 1);
  host.entry->set_chunk_id(200);
  host.entry->SetChunkIdAtPrefix(0, 20);
  host.entry->SetFullHashAt(0, Sha256Hash("www.fullevil.com/bad1.html"));

  chunk.chunk_number = 200;
  chunk.is_add = false;
  chunk.hosts.clear();
  chunk.hosts.push_back(host);
  chunks.clear();
  chunks.push_back(chunk);
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks);
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.fullevil.com/bad1.html"),
                                      &listname, &prefixes, &full_hashes,
                                      Time::Now()));
  EXPECT_TRUE(full_hashes.empty());

  // There should be one remaining full add.
  EXPECT_TRUE(database_->ContainsUrl(GURL("http://www.fullevil.com/bad2.html"),
                                     &listname, &prefixes, &full_hashes,
                                     Time::Now()));
  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_TRUE(SBFullHashEq(full_hashes[0].hash,
                           Sha256Hash("www.fullevil.com/bad2.html")));
  prefixes.clear();
  full_hashes.clear();

  // Now test an AddDel for the remaining full add.
  EXPECT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 20);
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.fullevil.com/bad1.html"),
                                      &listname, &prefixes, &full_hashes,
                                      Time::Now()));
  EXPECT_FALSE(database_->ContainsUrl(GURL("http://www.fullevil.com/bad2.html"),
                                      &listname, &prefixes, &full_hashes,
                                      Time::Now()));
}

namespace {

void PrintStat(const char* name) {
  int value = StatsTable::current()->GetCounterValue(name);
  SB_DLOG(INFO) << StringPrintf("%s %d", name, value);
}

FilePath GetFullSBDataPath(const FilePath& path) {
  FilePath full_path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &full_path)) {
    ADD_FAILURE() << "Unable to find test DIR_SOURCE_ROOT for test data.";
    return FilePath();
  }
  full_path = full_path.AppendASCII("chrome");
  full_path = full_path.AppendASCII("test");
  full_path = full_path.AppendASCII("data");
  full_path = full_path.AppendASCII("safe_browsing");
  full_path = full_path.Append(path);
  return full_path;
}

// TODO(shess): The clients of this structure manually manage
// |chunks|.  Improve this code to apply the RAII idiom to manage
// |chunks|.
struct ChunksInfo {
  SBChunkList* chunks;  // weak
  std::string listname;
};

// TODO(shess): Move this into SafeBrowsingDatabaseTest.
void PerformUpdate(SafeBrowsingDatabaseBloom* database,
                   const FilePath& database_filename,
                   const FilePath& initial_db,
                   const std::vector<ChunksInfo>& chunks,
                   const std::vector<SBChunkDelete>& deletes) {
  base::IoCounters before, after;

  if (!initial_db.empty()) {
    FilePath full_initial_db = GetFullSBDataPath(initial_db);
    ASSERT_FALSE(full_initial_db.empty());
    ASSERT_TRUE(file_util::PathExists(full_initial_db));
    ASSERT_TRUE(file_util::CopyFile(full_initial_db, database_filename));
  }

  Time before_time = Time::Now();
  base::ProcessHandle handle = base::Process::Current().handle();
  scoped_ptr<base::ProcessMetrics> metric(
#if !defined(OS_MACOSX)
      base::ProcessMetrics::CreateProcessMetrics(handle));
#else
      // Getting stats only for the current process is enough, so NULL is fine.
      base::ProcessMetrics::CreateProcessMetrics(handle, NULL));
#endif
  // Get IO stats.  These are currently not supported on Mac, and may not be
  // available for Linux, so we check the result and only show IO stats if
  // they are available.
  bool gotIOCounters = metric->GetIOCounters(&before);

  std::vector<SBListChunkRanges> lists;
  EXPECT_TRUE(database->UpdateStarted(&lists));
  database->DeleteChunks(deletes);
  for (size_t i = 0; i < chunks.size(); ++i)
    database->InsertChunks(chunks[i].listname, *chunks[i].chunks);

  database->UpdateFinished(true);
  gotIOCounters = gotIOCounters && metric->GetIOCounters(&after);

  if (gotIOCounters) {
    SB_DLOG(INFO) << StringPrintf("I/O Read Bytes: %" PRIu64,
        after.ReadTransferCount - before.ReadTransferCount);
    SB_DLOG(INFO) << StringPrintf("I/O Write Bytes: %" PRIu64,
        after.WriteTransferCount - before.WriteTransferCount);
    SB_DLOG(INFO) << StringPrintf("I/O Reads: %" PRIu64,
        after.ReadOperationCount - before.ReadOperationCount);
    SB_DLOG(INFO) << StringPrintf("I/O Writes: %" PRIu64,
        after.WriteOperationCount - before.WriteOperationCount);
  }
  SB_DLOG(INFO) << StringPrintf("Finished in %" PRId64 " ms",
      (Time::Now() - before_time).InMilliseconds());

  PrintStat("c:SB.HostSelect");
  PrintStat("c:SB.HostSelectForBloomFilter");
  PrintStat("c:SB.HostReplace");
  PrintStat("c:SB.HostInsert");
  PrintStat("c:SB.HostDelete");
  PrintStat("c:SB.ChunkSelect");
  PrintStat("c:SB.ChunkInsert");
  PrintStat("c:SB.ChunkDelete");
  PrintStat("c:SB.TransactionCommit");
}

void UpdateDatabase(SafeBrowsingDatabaseBloom* database,
                    const FilePath& database_filename,
                    const FilePath& initial_db,
                    const FilePath& response_path,
                    const FilePath& updates_path) {
  // First we read the chunks from disk, so that this isn't counted in IO bytes.
  std::vector<ChunksInfo> chunks;

  SafeBrowsingProtocolParser parser;
  if (!updates_path.empty()) {
    FilePath data_dir = GetFullSBDataPath(updates_path);
    ASSERT_FALSE(data_dir.empty());
    ASSERT_TRUE(file_util::PathExists(data_dir));
    file_util::FileEnumerator file_enum(data_dir, false,
        file_util::FileEnumerator::FILES);
    while (true) {
      FilePath file = file_enum.Next();
      if (file.empty())
        break;

      int64 size64;
      bool result = file_util::GetFileSize(file, &size64);
      ASSERT_TRUE(result);

      int size = static_cast<int>(size64);
      scoped_array<char> data(new char[size]);
      file_util::ReadFile(file, data.get(), size);

      ChunksInfo info;
      info.chunks = new SBChunkList;

      bool re_key;
      result = parser.ParseChunk(data.get(), size, "", "",
                                 &re_key, info.chunks);
      ASSERT_TRUE(result);

      info.listname = WideToASCII(file.BaseName().ToWStringHack());
      size_t index = info.listname.find('_');  // Get rid fo the _s or _a.
      info.listname.resize(index);
      info.listname.erase(0, 3);  // Get rid of the 000 etc.

      chunks.push_back(info);
    }
  }

  std::vector<SBChunkDelete> deletes;
  if (!response_path.empty()) {
    std::string update;
    FilePath full_response_path = GetFullSBDataPath(response_path);
    ASSERT_FALSE(full_response_path.empty());
    ASSERT_TRUE(file_util::PathExists(full_response_path));
    if (file_util::ReadFileToString(full_response_path, &update)) {
      int next_update;
      bool result, rekey, reset;
      std::vector<ChunkUrl> urls;
      result = parser.ParseUpdate(update.c_str(),
                                  static_cast<int>(update.length()),
                                  "",
                                  &next_update,
                                  &rekey,
                                  &reset,
                                  &deletes,
                                  &urls);
      ASSERT_TRUE(result);
      if (!updates_path.empty())
        ASSERT_EQ(urls.size(), chunks.size());
    }
  }

  PerformUpdate(database, database_filename, initial_db, chunks, deletes);

  // TODO(shess): Make ChunksInfo handle this via scoping.
  for (std::vector<ChunksInfo>::iterator iter = chunks.begin();
       iter != chunks.end(); ++iter) {
    delete iter->chunks;
    iter->chunks = NULL;
  }
}

// Construct the shared base path used by the GetOld* functions.
FilePath BasePath() {
  return FilePath(FILE_PATH_LITERAL("old"));
}

FilePath GetOldSafeBrowsingPath() {
  return BasePath().AppendASCII("SafeBrowsing");
}

FilePath GetOldResponsePath() {
  return BasePath().AppendASCII("response");
}

FilePath GetOldUpdatesPath() {
  return BasePath().AppendASCII("updates");
}

}  // namespace

// Counts the IO needed for the initial update of a database.
// test\data\safe_browsing\download_update.py was used to fetch the add/sub
// chunks that are read, in order to get repeatable runs.
TEST_F(SafeBrowsingDatabaseBloomTest, DatabaseInitialIO) {
  UpdateDatabase(database_.get(), database_filename_,
                 FilePath(), FilePath(), FilePath().AppendASCII("initial"));
}

// Counts the IO needed to update a month old database.
// The data files were generated by running "..\download_update.py postdata"
// in the "safe_browsing\old" directory.
TEST_F(SafeBrowsingDatabaseBloomTest, DatabaseOldIO) {
  UpdateDatabase(database_.get(), database_filename_, GetOldSafeBrowsingPath(),
                 GetOldResponsePath(), GetOldUpdatesPath());
}

// Like DatabaseOldIO but only the deletes.
TEST_F(SafeBrowsingDatabaseBloomTest, DatabaseOldDeletesIO) {
  UpdateDatabase(database_.get(), database_filename_,
                 GetOldSafeBrowsingPath(), GetOldResponsePath(), FilePath());
}

// Like DatabaseOldIO but only the updates.
TEST_F(SafeBrowsingDatabaseBloomTest, DatabaseOldUpdatesIO) {
  UpdateDatabase(database_.get(), database_filename_,
                 GetOldSafeBrowsingPath(), FilePath(), GetOldUpdatesPath());
}

// Does a a lot of addel's on very large chunks.
TEST_F(SafeBrowsingDatabaseBloomTest, DatabaseOldLotsofDeletesIO) {
  std::vector<ChunksInfo> chunks;
  std::vector<SBChunkDelete> deletes;
  SBChunkDelete del;
  del.is_sub_del = false;
  del.list_name = safe_browsing_util::kMalwareList;
  del.chunk_del.push_back(ChunkRange(3539, 3579));
  deletes.push_back(del);
  PerformUpdate(database_.get(), database_filename_,
                GetOldSafeBrowsingPath(), chunks, deletes);
}
