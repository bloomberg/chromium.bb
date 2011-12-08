// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/mock_special_storage_policy.h"

using content::BrowserThread;
using content::BrowserThreadImpl;

class DOMStorageTest : public testing::Test {
 public:
  DOMStorageTest()
      : message_loop_(MessageLoop::TYPE_IO),
        webkit_thread_(BrowserThread::WEBKIT, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;

 private:
  BrowserThreadImpl webkit_thread_;
};

TEST_F(DOMStorageTest, SessionOnly) {
  GURL session_only_origin("http://www.sessiononly.com");
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
      new quota::MockSpecialStoragePolicy;
  special_storage_policy->AddSessionOnly(session_only_origin);

  TestBrowserContext browser_context;

  // Create databases for permanent and session-only origins.
  FilePath domstorage_dir = browser_context.GetPath().Append(
      DOMStorageContext::kLocalStorageDirectory);
  FilePath::StringType session_only_database(
      FILE_PATH_LITERAL("http_www.sessiononly.com_0"));
  FilePath::StringType permanent_database(
      FILE_PATH_LITERAL("http_www.permanent.com_0"));
  session_only_database.append(DOMStorageContext::kLocalStorageExtension);
  permanent_database.append(DOMStorageContext::kLocalStorageExtension);
  FilePath session_only_database_path =
      domstorage_dir.Append(session_only_database);
  FilePath permanent_database_path =
      domstorage_dir.Append(permanent_database);

  ASSERT_TRUE(file_util::CreateDirectory(domstorage_dir));

  ASSERT_EQ(1, file_util::WriteFile(session_only_database_path, ".", 1));
  ASSERT_EQ(1, file_util::WriteFile(permanent_database_path, ".", 1));

  // Inject MockSpecialStoragePolicy into DOMStorageContext.
  browser_context.GetWebKitContext()->dom_storage_context()->
       special_storage_policy_ = special_storage_policy;

  // Delete the WebKitContext before destroying TestBrowserContext. This way the
  // temporary data directory stays alive long enough to conduct the test.
  browser_context.webkit_context_ = NULL;
  // Run the message loop to ensure that DOMStorageContext gets destroyed.
  message_loop_.RunAllPending();

  // Expected result: the database file for the permanent storage remains but
  // the database for the session only storage was deleted.
  EXPECT_FALSE(file_util::PathExists(session_only_database_path));
  EXPECT_TRUE(file_util::PathExists(permanent_database_path));
}

TEST_F(DOMStorageTest, SaveSessionState) {
  GURL session_only_origin("http://www.sessiononly.com");
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
      new quota::MockSpecialStoragePolicy;
  special_storage_policy->AddSessionOnly(session_only_origin);

  TestBrowserContext browser_context;

  // Create databases for permanent and session-only origins.
  FilePath domstorage_dir = browser_context.GetPath().Append(
      DOMStorageContext::kLocalStorageDirectory);
  FilePath::StringType session_only_database(
      FILE_PATH_LITERAL("http_www.sessiononly.com_0"));
  FilePath::StringType permanent_database(
      FILE_PATH_LITERAL("http_www.permanent.com_0"));
  session_only_database.append(DOMStorageContext::kLocalStorageExtension);
  permanent_database.append(DOMStorageContext::kLocalStorageExtension);
  FilePath session_only_database_path =
      domstorage_dir.Append(session_only_database);
  FilePath permanent_database_path =
      domstorage_dir.Append(permanent_database);

  ASSERT_TRUE(file_util::CreateDirectory(domstorage_dir));

  ASSERT_EQ(1, file_util::WriteFile(session_only_database_path, ".", 1));
  ASSERT_EQ(1, file_util::WriteFile(permanent_database_path, ".", 1));

  // Inject MockSpecialStoragePolicy into DOMStorageContext.
  DOMStorageContext* dom_storage_context =
    browser_context.GetWebKitContext()->dom_storage_context();
  dom_storage_context->special_storage_policy_ = special_storage_policy;

  dom_storage_context->set_clear_local_state_on_exit_(true);

  // Save session state. This should bypass the destruction-time deletion.
  dom_storage_context->SaveSessionState();

  // Delete the WebKitContext before destroying TestBrowserContext. This way the
  // temporary data directory stays alive long enough to conduct the test.
  browser_context.webkit_context_ = NULL;
  // Run the message loop to ensure that DOMStorageContext gets destroyed.
  message_loop_.RunAllPending();

  // Expected result: no database files were deleted because of
  // SaveSessionState.
  EXPECT_TRUE(file_util::PathExists(session_only_database_path));
  EXPECT_TRUE(file_util::PathExists(permanent_database_path));
}
