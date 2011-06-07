// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_remover.h"

#include "base/message_loop.h"
#include "base/platform_file.h"
#include "chrome/browser/history/history.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

namespace {

const char kTestkOrigin1[] = "http://host1:1/";
const char kTestkOrigin2[] = "http://host2:1/";
const char kTestkOrigin3[] = "http://host3:1/";

const GURL kOrigin1(kTestkOrigin1);
const GURL kOrigin2(kTestkOrigin2);
const GURL kOrigin3(kTestkOrigin3);

class BrowsingDataRemoverTester : public BrowsingDataRemover::Observer {
 public:
  BrowsingDataRemoverTester() {}
  virtual ~BrowsingDataRemoverTester() {}

  void BlockUntilNotified() {
    MessageLoop::current()->Run();
  }

 protected:
  // BrowsingDataRemover::Observer implementation.
  virtual void OnBrowsingDataRemoverDone() {
    Notify();
  }

  void Notify() {
    MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTester);
};

class RemoveHistoryTester : public BrowsingDataRemoverTester {
 public:
  explicit RemoveHistoryTester(TestingProfile* profile)
      : query_url_success_(false) {
    profile->CreateHistoryService(true, false);
    history_service_ = profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    history_service_->QueryURL(
        url,
        true,
        &consumer_,
        NewCallback(this, &RemoveHistoryTester::SaveResultAndQuit));
    BlockUntilNotified();
    return query_url_success_;
  }

  void AddHistory(const GURL& url, base::Time time) {
    history_service_->AddPage(url, time, NULL, 0, GURL(), PageTransition::LINK,
        history::RedirectList(), history::SOURCE_BROWSED, false);
  }

 private:
  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(HistoryService::Handle,
                         bool success,
                         const history::URLRow*,
                         history::VisitVector*) {
    query_url_success_ = success;
    Notify();
  }


  // For History requests.
  CancelableRequestConsumer consumer_;
  bool query_url_success_;

  // TestingProfile owns the history service; we shouldn't delete it.
  HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryTester);
};

class RemoveFileSystemTester : public BrowsingDataRemoverTester {
 public:
  explicit RemoveFileSystemTester() {}

  void FindFileSystemPathCallback(bool success,
                                  const FilePath& path,
                                  const std::string& name) {
    found_file_system_ = success;
    Notify();
  }

  bool FileSystemContainsOriginAndType(const GURL& origin,
                                       fileapi::FileSystemType type) {
    sandbox_->ValidateFileSystemRootAndGetURL(
        origin, type, false, NewCallback(this,
            &RemoveFileSystemTester::FindFileSystemPathCallback));
    BlockUntilNotified();
    return found_file_system_;
  }

  virtual void PopulateTestFileSystemData(TestingProfile* profile) {
    // Set up kOrigin1 with a temporary file system, kOrigin2 with a persistent
    // file system, and kOrigin3 with both.
    sandbox_ = profile->GetFileSystemContext()->path_manager()->
        sandbox_provider();

    CreateDirectoryForOriginAndType(kOrigin1,
        fileapi::kFileSystemTypeTemporary);
    CreateDirectoryForOriginAndType(kOrigin2,
        fileapi::kFileSystemTypePersistent);
    CreateDirectoryForOriginAndType(kOrigin3,
        fileapi::kFileSystemTypeTemporary);
    CreateDirectoryForOriginAndType(kOrigin3,
        fileapi::kFileSystemTypePersistent);

    EXPECT_FALSE(FileSystemContainsOriginAndType(kOrigin1,
        fileapi::kFileSystemTypePersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin1,
        fileapi::kFileSystemTypeTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin2,
        fileapi::kFileSystemTypePersistent));
    EXPECT_FALSE(FileSystemContainsOriginAndType(kOrigin2,
        fileapi::kFileSystemTypeTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin3,
        fileapi::kFileSystemTypePersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin3,
        fileapi::kFileSystemTypeTemporary));
  }

  void CreateDirectoryForOriginAndType(const GURL& origin,
                                       fileapi::FileSystemType type) {
    FilePath target = sandbox_->ValidateFileSystemRootAndGetPathOnFileThread(
        origin, type, FilePath(), true);
    EXPECT_TRUE(file_util::DirectoryExists(target));
  }

 private:
  fileapi::SandboxMountPointProvider* sandbox_;
  bool found_file_system_;

  DISALLOW_COPY_AND_ASSIGN(RemoveFileSystemTester);
};

class BrowsingDataRemoverTest : public testing::Test {
 public:
  BrowsingDataRemoverTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        webkit_thread_(BrowserThread::WEBKIT, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        profile_(new TestingProfile()) {
  }

  virtual ~BrowsingDataRemoverTest() {
  }

  void TearDown() {
    // TestingProfile contains a WebKitContext.  WebKitContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    message_loop_.RunAllPending();
  }

  void BlockUntilBrowsingDataRemoved(BrowsingDataRemover::TimePeriod period,
                                     base::Time delete_end,
                                     int remove_mask,
                                     BrowsingDataRemoverTester* tester) {
    BrowsingDataRemover* remover = new BrowsingDataRemover(profile_.get(),
        period, delete_end);
    remover->AddObserver(tester);

    // BrowsingDataRemover deletes itself when it completes.
    remover->Remove(remove_mask);
    tester->BlockUntilNotified();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

 private:
  // message_loop_, as well as all the threads associated with it must be
  // defined before profile_ to prevent explosions. Oh how I love C++.
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  BrowserThread webkit_thread_;
  BrowserThread file_thread_;
  BrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTest);
};

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForever) {
  scoped_ptr<RemoveHistoryTester> tester(
      new RemoveHistoryTester(GetProfile()));

  tester->AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(tester->HistoryContainsURL(kOrigin1));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      base::Time::Now(), BrowsingDataRemover::REMOVE_HISTORY, tester.get());

  EXPECT_FALSE(tester->HistoryContainsURL(kOrigin1));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForLastHour) {
  scoped_ptr<RemoveHistoryTester> tester(
      new RemoveHistoryTester(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester->AddHistory(kOrigin1, base::Time::Now());
  tester->AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester->HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester->HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      base::Time::Now(), BrowsingDataRemover::REMOVE_HISTORY, tester.get());

  EXPECT_FALSE(tester->HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester->HistoryContainsURL(kOrigin2));
}

TEST_F(BrowsingDataRemoverTest, RemoveFileSystemsForever) {
  scoped_ptr<RemoveFileSystemTester> tester(new RemoveFileSystemTester());

  tester->PopulateTestFileSystemData(GetProfile());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      base::Time::Now(), BrowsingDataRemover::REMOVE_COOKIES, tester.get());

  EXPECT_FALSE(tester->FileSystemContainsOriginAndType(kOrigin1,
      fileapi::kFileSystemTypePersistent));
  EXPECT_FALSE(tester->FileSystemContainsOriginAndType(kOrigin1,
      fileapi::kFileSystemTypeTemporary));
  EXPECT_FALSE(tester->FileSystemContainsOriginAndType(kOrigin2,
      fileapi::kFileSystemTypePersistent));
  EXPECT_FALSE(tester->FileSystemContainsOriginAndType(kOrigin2,
      fileapi::kFileSystemTypeTemporary));
  EXPECT_FALSE(tester->FileSystemContainsOriginAndType(kOrigin3,
      fileapi::kFileSystemTypePersistent));
  EXPECT_FALSE(tester->FileSystemContainsOriginAndType(kOrigin3,
      fileapi::kFileSystemTypeTemporary));
}

}  // namespace
