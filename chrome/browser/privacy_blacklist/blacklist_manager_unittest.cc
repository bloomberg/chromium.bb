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

  virtual std::vector<FilePath> GetBlacklistPaths() {
    return paths_;
  }

  void AddPath(const FilePath& path) {
    paths_.push_back(path);
    NotificationService::current()->Notify(
        NotificationType::PRIVACY_BLACKLIST_PATH_PROVIDER_UPDATED,
        Source<Profile>(profile_),
        Details<BlacklistPathProvider>(this));
  }

 private:
  Profile* profile_;

  std::vector<FilePath> paths_;

  DISALLOW_COPY_AND_ASSIGN(TestBlacklistPathProvider);
};

class BlacklistManagerTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("blacklist_samples");
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

 protected:
  FilePath test_data_dir_;

  MyTestingProfile profile_;

 private:
  MessageLoop loop_;
};

TEST_F(BlacklistManagerTest, Basic) {
  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, NULL));

  const Blacklist* blacklist = manager->GetCompiledBlacklist();

  // We should get an empty, but valid object.
  ASSERT_TRUE(blacklist);
  EXPECT_TRUE(blacklist->is_good());

  // Repeated invocations of GetCompiledBlacklist should return the same object.
  EXPECT_EQ(blacklist, manager->GetCompiledBlacklist());
}

TEST_F(BlacklistManagerTest, BlacklistPathProvider) {
  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, NULL));

  const Blacklist* blacklist1 = manager->GetCompiledBlacklist();
  EXPECT_FALSE(blacklist1->findMatch(GURL("http://host/annoying_ads/ad.jpg")));

  TestBlacklistPathProvider provider(&profile_);
  manager->RegisterBlacklistPathProvider(&provider);

  // Blacklist should not get recompiled.
  EXPECT_EQ(blacklist1, manager->GetCompiledBlacklist());

  provider.AddPath(test_data_dir_.AppendASCII("annoying_ads.pbl"));

  const Blacklist* blacklist2 = manager->GetCompiledBlacklist();

  // Added a real blacklist, the manager should recompile.
  EXPECT_NE(blacklist1, blacklist2);
  EXPECT_TRUE(blacklist2->findMatch(GURL("http://host/annoying_ads/ad.jpg")));

  manager->UnregisterBlacklistPathProvider(&provider);

  // Just unregistering the provider doesn't remove the blacklist paths
  // from the manager.
  EXPECT_EQ(blacklist2, manager->GetCompiledBlacklist());
}

TEST_F(BlacklistManagerTest, RealThread) {
  base::Thread backend_thread("backend_thread");
  backend_thread.Start();

  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, &backend_thread));

  // Make sure all pending tasks run.
  backend_thread.Stop();
  backend_thread.Start();

  const Blacklist* blacklist1 = manager->GetCompiledBlacklist();
  EXPECT_FALSE(blacklist1->findMatch(GURL("http://host/annoying_ads/ad.jpg")));

  TestBlacklistPathProvider provider(&profile_);
  manager->RegisterBlacklistPathProvider(&provider);

  // Make sure all pending tasks run.
  backend_thread.Stop();
  backend_thread.Start();

  // Blacklist should not get recompiled.
  EXPECT_EQ(blacklist1, manager->GetCompiledBlacklist());

  provider.AddPath(test_data_dir_.AppendASCII("annoying_ads.pbl"));

  // Make sure all pending tasks run.
  backend_thread.Stop();
  backend_thread.Start();

  const Blacklist* blacklist2 = manager->GetCompiledBlacklist();

  // Added a real blacklist, the manager should recompile.
  EXPECT_NE(blacklist1, blacklist2);
  EXPECT_TRUE(blacklist2->findMatch(GURL("http://host/annoying_ads/ad.jpg")));

  manager->UnregisterBlacklistPathProvider(&provider);

  // Make sure all pending tasks run.
  backend_thread.Stop();
  backend_thread.Start();

  // Just unregistering the provider doesn't remove the blacklist paths
  // from the manager.
  EXPECT_EQ(blacklist2, manager->GetCompiledBlacklist());
}

TEST_F(BlacklistManagerTest, CompiledBlacklistStaysOnDisk) {
  {
    scoped_refptr<BlacklistManager> manager(
        new BlacklistManager(&profile_, NULL));

    TestBlacklistPathProvider provider(&profile_);
    manager->RegisterBlacklistPathProvider(&provider);
    provider.AddPath(test_data_dir_.AppendASCII("annoying_ads.pbl"));
    const Blacklist* blacklist = manager->GetCompiledBlacklist();
    EXPECT_TRUE(blacklist->findMatch(GURL("http://host/annoying_ads/ad.jpg")));
    manager->UnregisterBlacklistPathProvider(&provider);
  }

  {
    scoped_refptr<BlacklistManager> manager(
        new BlacklistManager(&profile_, NULL));

    // Make sure we read the compiled blacklist from disk and don't even touch
    // the paths providers.
    const Blacklist* blacklist = manager->GetCompiledBlacklist();
    EXPECT_TRUE(blacklist->findMatch(GURL("http://host/annoying_ads/ad.jpg")));
  }
}

TEST_F(BlacklistManagerTest, BlacklistPathReadError) {
  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, NULL));

  TestBlacklistPathProvider provider(&profile_);
  manager->RegisterBlacklistPathProvider(&provider);

  FilePath bogus_path(test_data_dir_.AppendASCII("does_not_exist_randomness"));
  ASSERT_FALSE(file_util::PathExists(bogus_path));
  provider.AddPath(bogus_path);

  const Blacklist* blacklist = manager->GetCompiledBlacklist();

  // We should get an empty, but valid object.
  ASSERT_TRUE(blacklist);
  EXPECT_TRUE(blacklist->is_good());

  manager->UnregisterBlacklistPathProvider(&provider);
}

TEST_F(BlacklistManagerTest, CompiledBlacklistReadError) {
  FilePath compiled_blacklist_path;

  {
    scoped_refptr<BlacklistManager> manager(
        new BlacklistManager(&profile_, NULL));

    TestBlacklistPathProvider provider(&profile_);
    manager->RegisterBlacklistPathProvider(&provider);
    provider.AddPath(test_data_dir_.AppendASCII("annoying_ads.pbl"));
    const Blacklist* blacklist = manager->GetCompiledBlacklist();
    EXPECT_TRUE(blacklist->findMatch(GURL("http://host/annoying_ads/ad.jpg")));
    manager->UnregisterBlacklistPathProvider(&provider);

    compiled_blacklist_path = manager->compiled_blacklist_path();
  }

  ASSERT_TRUE(file_util::PathExists(compiled_blacklist_path));
  ASSERT_TRUE(file_util::Delete(compiled_blacklist_path, false));

  {
    scoped_refptr<BlacklistManager> manager(
        new BlacklistManager(&profile_, NULL));

    // Now we don't have any providers, and no compiled blacklist file. We
    // shouldn't match any URLs.
    const Blacklist* blacklist = manager->GetCompiledBlacklist();
    EXPECT_FALSE(blacklist->findMatch(GURL("http://host/annoying_ads/ad.jpg")));
  }
}

TEST_F(BlacklistManagerTest, MultipleProviders) {
  scoped_refptr<BlacklistManager> manager(
      new BlacklistManager(&profile_, NULL));

  TestBlacklistPathProvider provider1(&profile_);
  TestBlacklistPathProvider provider2(&profile_);
  manager->RegisterBlacklistPathProvider(&provider1);
  manager->RegisterBlacklistPathProvider(&provider2);

  const Blacklist* blacklist1 = manager->GetCompiledBlacklist();
  EXPECT_FALSE(blacklist1->findMatch(GURL("http://sample/annoying_ads/a.jpg")));
  EXPECT_FALSE(blacklist1->findMatch(GURL("http://sample/other_ads/a.jpg")));
  EXPECT_FALSE(blacklist1->findMatch(GURL("http://host/something.doc")));

  provider1.AddPath(test_data_dir_.AppendASCII("annoying_ads.pbl"));
  const Blacklist* blacklist2 = manager->GetCompiledBlacklist();
  EXPECT_NE(blacklist1, blacklist2);

  provider2.AddPath(test_data_dir_.AppendASCII("host.pbl"));
  const Blacklist* blacklist3 = manager->GetCompiledBlacklist();
  EXPECT_NE(blacklist2, blacklist3);

  EXPECT_TRUE(blacklist3->findMatch(GURL("http://sample/annoying_ads/a.jpg")));
  EXPECT_FALSE(blacklist3->findMatch(GURL("http://sample/other_ads/a.jpg")));
  EXPECT_TRUE(blacklist3->findMatch(GURL("http://host/something.doc")));

  provider1.AddPath(test_data_dir_.AppendASCII("other_ads.pbl"));

  const Blacklist* blacklist4 = manager->GetCompiledBlacklist();

  EXPECT_NE(blacklist3, blacklist4);
  EXPECT_TRUE(blacklist4->findMatch(GURL("http://sample/annoying_ads/a.jpg")));
  EXPECT_TRUE(blacklist4->findMatch(GURL("http://sample/other_ads/a.jpg")));
  EXPECT_TRUE(blacklist4->findMatch(GURL("http://host/something.doc")));

  manager->UnregisterBlacklistPathProvider(&provider1);
  manager->UnregisterBlacklistPathProvider(&provider2);
}

}  // namespace
