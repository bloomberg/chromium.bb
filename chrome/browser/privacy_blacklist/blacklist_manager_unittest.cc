// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/thread.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MyTestingProfile : public TestingProfile {
 public:
  MyTestingProfile() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.path();
  }

 private:
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(MyTestingProfile);
};

class TestBlacklistPathProvider : public BlacklistPathProvider {
 public:
  explicit TestBlacklistPathProvider(Profile* profile) : profile_(profile) {
  }

  virtual std::vector<FilePath> GetPersistentBlacklistPaths() {
    return persistent_paths_;
  }

  virtual std::vector<FilePath> GetTransientBlacklistPaths() {
    return transient_paths_;
  }

  void AddPersistentPath(const FilePath& path) {
    persistent_paths_.push_back(path);
    SendUpdateNotification();
  }

  void AddTransientPath(const FilePath& path) {
    transient_paths_.push_back(path);
    SendUpdateNotification();
  }

  void clear() {
    persistent_paths_.clear();
    transient_paths_.clear();
    SendUpdateNotification();
  }

 private:
  void SendUpdateNotification() {
#if defined(OS_WIN)
    FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
    FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
    Extension extension(path);
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_LOADED,
        Source<Profile>(profile_),
        Details<Extension>(&extension));
  }

  Profile* profile_;

  std::vector<FilePath> persistent_paths_;
  std::vector<FilePath> transient_paths_;

  DISALLOW_COPY_AND_ASSIGN(TestBlacklistPathProvider);
};

class BlacklistManagerTest : public testing::Test, public NotificationObserver {
 public:
  BlacklistManagerTest()
      : path_provider_(&profile_),
        mock_ui_thread_(ChromeThread::UI, MessageLoop::current()),
        mock_file_thread_(ChromeThread::FILE) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("blacklist_samples");
    ASSERT_TRUE(mock_file_thread_.Start());
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    MessageLoop::current()->Quit();
  }

 protected:
  void WaitForBlacklistError() {
    NotificationRegistrar registrar;
    registrar.Add(this,
                  NotificationType::BLACKLIST_MANAGER_ERROR,
                  Source<Profile>(&profile_));
    MessageLoop::current()->Run();
  }

  void WaitForBlacklistUpdate() {
    NotificationRegistrar registrar;
    registrar.Add(this,
                  NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED,
                  Source<Profile>(&profile_));
    MessageLoop::current()->Run();
  }

  FilePath test_data_dir_;

  MyTestingProfile profile_;

  TestBlacklistPathProvider path_provider_;

 private:
  MessageLoop loop_;

  ChromeThread mock_ui_thread_;
  ChromeThread mock_file_thread_;
};

// Returns true if |blacklist| contains a match for |url|.
bool BlacklistHasMatch(const Blacklist* blacklist, const char* url) {
  Blacklist::Match* match = blacklist->findMatch(GURL(url));

  if (!match)
    return false;

  delete match;
  return true;
}

TEST_F(BlacklistManagerTest, Basic) {
  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, &path_provider_));
  WaitForBlacklistUpdate();

  const Blacklist* blacklist = manager->GetCompiledBlacklist();
  EXPECT_TRUE(blacklist);

  // Repeated invocations of GetCompiledBlacklist should return the same object.
  EXPECT_EQ(blacklist, manager->GetCompiledBlacklist());
}

TEST_F(BlacklistManagerTest, BlacklistPathProvider) {
  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, &path_provider_));
  WaitForBlacklistUpdate();

  const Blacklist* blacklist1 = manager->GetCompiledBlacklist();
  EXPECT_FALSE(BlacklistHasMatch(blacklist1,
                                 "http://host/annoying_ads/ad.jpg"));

  path_provider_.AddPersistentPath(
      test_data_dir_.AppendASCII("annoying_ads.pbl"));
  WaitForBlacklistUpdate();

  const Blacklist* blacklist2 = manager->GetCompiledBlacklist();

  // Added a real blacklist, the manager should recompile.
  EXPECT_NE(blacklist1, blacklist2);
  EXPECT_TRUE(BlacklistHasMatch(blacklist2, "http://host/annoying_ads/ad.jpg"));
  EXPECT_FALSE(BlacklistHasMatch(blacklist2, "http://host/other_ads/ad.jpg"));

  path_provider_.AddTransientPath(test_data_dir_.AppendASCII("other_ads.pbl"));
  WaitForBlacklistUpdate();

  const Blacklist* blacklist3 = manager->GetCompiledBlacklist();

  // In theory blacklist2 and blacklist3 could be the same object, so we're
  // not checking for inequality.
  EXPECT_TRUE(BlacklistHasMatch(blacklist3, "http://host/annoying_ads/ad.jpg"));
  EXPECT_TRUE(BlacklistHasMatch(blacklist3, "http://host/other_ads/ad.jpg"));

  // Now make sure that transient blacklists don't survive after re-creating
  // the BlacklistManager.
  manager = NULL;
  path_provider_.clear();
  path_provider_.AddPersistentPath(
      test_data_dir_.AppendASCII("annoying_ads.pbl"));
  manager = new BlacklistManager(&profile_, &path_provider_);
  WaitForBlacklistUpdate();

  const Blacklist* blacklist4 = manager->GetCompiledBlacklist();

  EXPECT_TRUE(BlacklistHasMatch(blacklist4, "http://host/annoying_ads/ad.jpg"));
  EXPECT_FALSE(BlacklistHasMatch(blacklist4, "http://host/other_ads/ad.jpg"));
}

TEST_F(BlacklistManagerTest, BlacklistPathReadError) {
  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, &path_provider_));
  WaitForBlacklistUpdate();

  FilePath bogus_path(test_data_dir_.AppendASCII("does_not_exist_randomness"));
  ASSERT_FALSE(file_util::PathExists(bogus_path));
  path_provider_.AddPersistentPath(bogus_path);
  WaitForBlacklistError();

  const Blacklist* blacklist = manager->GetCompiledBlacklist();
  EXPECT_TRUE(blacklist);
}

TEST_F(BlacklistManagerTest, CompiledBlacklistReadError) {
  FilePath compiled_blacklist_path;

  {
    scoped_refptr<BlacklistManager> manager(
        new BlacklistManager(&profile_, &path_provider_));
    WaitForBlacklistUpdate();

    path_provider_.AddPersistentPath(
        test_data_dir_.AppendASCII("annoying_ads.pbl"));
    WaitForBlacklistUpdate();
    const Blacklist* blacklist = manager->GetCompiledBlacklist();
    EXPECT_TRUE(BlacklistHasMatch(blacklist,
                                  "http://host/annoying_ads/ad.jpg"));

    compiled_blacklist_path = manager->compiled_blacklist_path();
  }

  ASSERT_TRUE(file_util::PathExists(compiled_blacklist_path));
  ASSERT_TRUE(file_util::Delete(compiled_blacklist_path, false));

  {
    scoped_refptr<BlacklistManager> manager(
        new BlacklistManager(&profile_, &path_provider_));
    WaitForBlacklistUpdate();

    // The manager should recompile the blacklist.
    const Blacklist* blacklist = manager->GetCompiledBlacklist();
    EXPECT_TRUE(BlacklistHasMatch(blacklist,
                                  "http://host/annoying_ads/ad.jpg"));
  }
}

}  // namespace
