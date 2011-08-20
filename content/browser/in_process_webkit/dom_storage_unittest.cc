// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_browser_process_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "webkit/quota/mock_special_storage_policy.h"

class DOMStorageTest : public TestingBrowserProcessTest {
 public:
  DOMStorageTest()
      : message_loop_(MessageLoop::TYPE_IO),
        webkit_thread_(BrowserThread::WEBKIT, &message_loop_) {
  }
 protected:
  MessageLoop message_loop_;

 private:
  BrowserThread webkit_thread_;
};

TEST_F(DOMStorageTest, SessionOnly) {
  GURL session_only_origin("http://www.sessiononly.com");
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
      new quota::MockSpecialStoragePolicy;
  special_storage_policy->AddSessionOnly(session_only_origin);

  // A scope for deleting TestingProfile early enough.
  {
    TestingProfile profile;

    // Create databases for permanent and session-only origins.
    FilePath domstorage_dir = profile.GetPath().Append(
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
    profile.GetWebKitContext()->dom_storage_context()->special_storage_policy_ =
        special_storage_policy;

    // Tell the WebKitContext explicitly do clean up the session-only
    // data. TestingProfile (unlike ProfileImpl) doesn't do it automatically
    // when destructed. The deletion is done immediately since we're on the
    // WEBKIT thread (created by the test).
    profile.GetWebKitContext()->DeleteSessionOnlyData();

    // Expected result: the database file for the permanent storage remains but
    // the database for the session only storage was deleted.
    EXPECT_FALSE(file_util::PathExists(session_only_database_path));
    EXPECT_TRUE(file_util::PathExists(permanent_database_path));
  }
  // Run the message loop to ensure that DOMStorageContext gets destroyed.
  message_loop_.RunAllPending();
}
