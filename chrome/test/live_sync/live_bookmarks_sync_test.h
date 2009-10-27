// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "googleurl/src/gurl.h"

class BookmarkModel;
class BookmarkNode;
class Profile;

namespace switches {
extern const wchar_t kSyncUserForTest[];
extern const wchar_t kSyncPasswordForTest[];
}

// TODO(timsteele): This should be moved out of personalization_unit_tests into
// its own project that doesn't get run by default on the standard buildbot
// without a valid sync server set up.
class LiveBookmarksSyncTest : public InProcessBrowserTest {
 public:
  LiveBookmarksSyncTest() { }
  ~LiveBookmarksSyncTest() { }

  virtual void SetUp() {
    // At this point, the browser hasn't been launched, and no services are
    // available.  But we can verify our command line parameters and fail
    // early.
    const CommandLine* cl = CommandLine::ForCurrentProcess();
    username_ = WideToUTF8(cl->GetSwitchValue(switches::kSyncUserForTest));
    password_ = WideToUTF8(cl->GetSwitchValue(switches::kSyncPasswordForTest));
    ASSERT_FALSE(username_.empty()) << "Can't run live server test "
        << "without specifying --" << switches::kSyncUserForTest;
    ASSERT_FALSE(password_.empty()) << "Can't run live server test "
        << "without specifying --" << switches::kSyncPasswordForTest;

    // Yield control back to the InProcessBrowserTest framework.
    InProcessBrowserTest::SetUp();
  }

  // Append command line flag to enable sync.
  virtual void SetUpCommandLine(CommandLine* command_line) {
  }

  // Helper to get a handle on a bookmark in |m| when the url is known to be
  // unique.
  static const BookmarkNode* GetByUniqueURL(BookmarkModel* m, const GURL& url);

  // Helper to ProfileManager::CreateProfile that handles path creation.
  static Profile* MakeProfile(const string16& name);

  // Utility to block (by running the current MessageLoop) until the model has
  // loaded.  Note this is required instead of using m->BlockTillLoaded, as that
  // cannot be called from the main thread (deadlock will occur).
  static void BlockUntilLoaded(BookmarkModel* m);

 protected:
  std::string username_;
  std::string password_;

  virtual void SetUpInProcessBrowserTestFixture();
  virtual void TearDownInProcessBrowserTestFixture();

 private:
  // LiveBookmarksSyncTests need to make live DNS requests for access to
  // GAIA and sync server URLs under google.com.  We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  DISALLOW_COPY_AND_ASSIGN(LiveBookmarksSyncTest);
};

#endif  // CHROME_TEST_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_
