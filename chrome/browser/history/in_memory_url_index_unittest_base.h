// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_UNITTEST_BASE_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_UNITTEST_BASE_H_

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class HistoryDatabase;

// A base class for unit tests that exercise the InMemoryURLIndex and the
// HistoryQuickProvider. Provides initialization of the index using data
// contained in a test file.
//
// The test version of the history url database table ('url') is contained in
// a database file created from a text file as specified by the
// TestDBName() method overridden by subclasses. The only difference between
// the table specified in this test file and a live 'urls' table from a
// profile is that the last_visit_time column in the test table contains a
// number specifying the number of days relative to 'today' to which the
// visit time of the URL will be set during the test setup stage.
//
// The format of the test database text file is of a SQLite .dump file.
// Note that only lines whose first character is an upper-case letter are
// processed when creating the test database.
//
class InMemoryURLIndexTestBase : public testing::Test {
 protected:
  InMemoryURLIndexTestBase();
  virtual ~InMemoryURLIndexTestBase();

  // Specifies the test data file name used by the subclass. The specified file
  // must reside in the path given by chrome::DIR_TEST_DATA.
  virtual FilePath::StringType TestDBName() const = 0;

  // Fills the HistoryBackend with test data from the test data file and creates
  // the InMemoryURLIndex instance, but does not fill it with data. Call
  // LoadIndex() after calling SetUp() To fill the InMemoryURLIndex instance
  // with the test data.
  // NOTE: By default, TestingProfile does not enable the cache database
  //       (InMemoryURLCacheDatabase). If a test relies on the cache database
  //       having been enabled then that test should subclass TestingProfile
  //       and provide an override of InitHistoryService(...) that causes
  //       the cache database to be created and initialized. For an example,
  //       see CacheTestingProfile in in_memory_url_index_unittest.cc.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Loads the InMemoryURLIndex instance with data from the HistoryBackend.
  // Blocks until the load completes. Completion does not imply success.
  void LoadIndex();

  // Blocks the caller until the load sequence for the index has completed.
  // Note that load completion does not imply success.
  void BlockUntilIndexLoaded();

  // Pass-through function to simplify our friendship with InMemoryURLIndex.
  URLIndexPrivateData* GetPrivateData();

  // Pass-through functions to simplify our friendship with URLIndexPrivateData.
  bool UpdateURL(const URLRow& row);
  bool DeleteURL(const GURL& url);
  bool GetCacheDBPath(FilePath* file_path);

  InMemoryURLIndex* url_index_;
  HistoryDatabase* history_database_;

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_ptr<TestingProfile> profile_;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_UNITTEST_BASE_H_
