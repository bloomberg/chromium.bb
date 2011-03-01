// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/extensions_activity_monitor.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_service.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::ExtensionsActivityMonitor;
namespace keys = extension_manifest_keys;

namespace {

const FilePath::CharType kTestExtensionPath1[] =
#if defined(OS_POSIX)
    FILE_PATH_LITERAL("/testextension1");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("c:\\testextension1");
#endif

const FilePath::CharType kTestExtensionPath2[] =
#if defined(OS_POSIX)
    FILE_PATH_LITERAL("/testextension2");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("c:\\testextension2");
#endif

const char* kTestExtensionVersion = "1.0.0.0";
const char* kTestExtensionName = "foo extension";

template <class FunctionType>
class BookmarkAPIEventTask : public Task {
 public:
  BookmarkAPIEventTask(FunctionType* t, Extension* e, size_t repeats,
                       base::WaitableEvent* done) :
       extension_(e), function_(t), repeats_(repeats), done_(done) {}
   virtual void Run() {
     for (size_t i = 0; i < repeats_; i++) {
       NotificationService::current()->Notify(
           NotificationType::EXTENSION_BOOKMARKS_API_INVOKED,
           Source<Extension>(extension_.get()),
           Details<const BookmarksFunction>(function_.get()));
     }
     done_->Signal();
   }
 private:
  scoped_refptr<Extension> extension_;
  scoped_refptr<FunctionType> function_;
  size_t repeats_;
  base::WaitableEvent* done_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAPIEventTask);
};

class BookmarkAPIEventGenerator {
 public:
  BookmarkAPIEventGenerator() {}
  virtual ~BookmarkAPIEventGenerator() {}
  template <class T>
  void NewEvent(const FilePath::StringType& extension_path,
      T* bookmarks_function, size_t repeats) {
    std::string error;
    DictionaryValue input;
    input.SetString(keys::kVersion, kTestExtensionVersion);
    input.SetString(keys::kName, kTestExtensionName);
    scoped_refptr<Extension> extension(Extension::Create(
        FilePath(extension_path), Extension::INVALID, input, false, &error));
    bookmarks_function->set_name(T::function_name());
    base::WaitableEvent done_event(false, false);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        new BookmarkAPIEventTask<T>(bookmarks_function, extension,
                                    repeats, &done_event));
    done_event.Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAPIEventGenerator);
};
}  // namespace

class DoUIThreadSetupTask : public Task {
 public:
  DoUIThreadSetupTask(NotificationService** service,
                      base::WaitableEvent* done)
      : service_(service), signal_when_done_(done) {}
  virtual ~DoUIThreadSetupTask() {}
  virtual void Run() {
    *service_ = new NotificationService();
    signal_when_done_->Signal();
  }
 private:
  NotificationService** service_;
  base::WaitableEvent* signal_when_done_;
  DISALLOW_COPY_AND_ASSIGN(DoUIThreadSetupTask);
};

class ExtensionsActivityMonitorTest : public testing::Test {
 public:
  ExtensionsActivityMonitorTest() : service_(NULL),
      ui_thread_(BrowserThread::UI) { }
  virtual ~ExtensionsActivityMonitorTest() {}

  virtual void SetUp() {
    ui_thread_.Start();
    base::WaitableEvent service_created(false, false);
    ui_thread_.message_loop()->PostTask(FROM_HERE,
        new DoUIThreadSetupTask(&service_, &service_created));
    service_created.Wait();
  }

  virtual void TearDown() {
    ui_thread_.message_loop()->DeleteSoon(FROM_HERE, service_);
    ui_thread_.Stop();
  }

  MessageLoop* ui_loop() { return ui_thread_.message_loop(); }

  static std::string GetExtensionIdForPath(
      const FilePath::StringType& extension_path) {
    std::string error;
    DictionaryValue input;
    input.SetString(keys::kVersion, kTestExtensionVersion);
    input.SetString(keys::kName, kTestExtensionName);
    scoped_refptr<Extension> extension(Extension::Create(
        FilePath(extension_path), Extension::INVALID, input, false, &error));
    EXPECT_EQ("", error);
    return extension->id();
  }
 private:
  NotificationService* service_;
  BrowserThread ui_thread_;
};

TEST_F(ExtensionsActivityMonitorTest, Basic) {
  ExtensionsActivityMonitor* monitor = new ExtensionsActivityMonitor();
  BookmarkAPIEventGenerator generator;

  generator.NewEvent<RemoveBookmarkFunction>(kTestExtensionPath1,
      new RemoveBookmarkFunction(), 1);
  generator.NewEvent<MoveBookmarkFunction>(kTestExtensionPath1,
      new MoveBookmarkFunction(), 1);
  generator.NewEvent<UpdateBookmarkFunction>(kTestExtensionPath1,
      new UpdateBookmarkFunction(), 2);
  generator.NewEvent<CreateBookmarkFunction>(kTestExtensionPath1,
      new CreateBookmarkFunction(), 3);
  generator.NewEvent<SearchBookmarksFunction>(kTestExtensionPath1,
      new SearchBookmarksFunction(), 5);
  const uint32 writes_by_extension1 = 1 + 1 + 2 + 3;

  generator.NewEvent<RemoveTreeBookmarkFunction>(kTestExtensionPath2,
      new RemoveTreeBookmarkFunction(), 8);
  generator.NewEvent<GetBookmarkTreeFunction>(kTestExtensionPath2,
      new GetBookmarkTreeFunction(), 13);
  generator.NewEvent<GetBookmarkChildrenFunction>(kTestExtensionPath2,
      new GetBookmarkChildrenFunction(), 21);
  generator.NewEvent<GetBookmarksFunction>(kTestExtensionPath2,
      new GetBookmarksFunction(), 33);
  const uint32 writes_by_extension2 = 8;

  ExtensionsActivityMonitor::Records results;
  monitor->GetAndClearRecords(&results);

  std::string id1 = GetExtensionIdForPath(kTestExtensionPath1);
  std::string id2 = GetExtensionIdForPath(kTestExtensionPath2);

  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(results.end() != results.find(id1));
  EXPECT_TRUE(results.end() != results.find(id2));
  EXPECT_EQ(writes_by_extension1, results[id1].bookmark_write_count);
  EXPECT_EQ(writes_by_extension2, results[id2].bookmark_write_count);

  ui_loop()->DeleteSoon(FROM_HERE, monitor);
}

TEST_F(ExtensionsActivityMonitorTest, Put) {
  ExtensionsActivityMonitor* monitor = new ExtensionsActivityMonitor();
  BookmarkAPIEventGenerator generator;
  std::string id1 = GetExtensionIdForPath(kTestExtensionPath1);
  std::string id2 = GetExtensionIdForPath(kTestExtensionPath2);

  generator.NewEvent<CreateBookmarkFunction>(kTestExtensionPath1,
      new CreateBookmarkFunction(), 5);
  generator.NewEvent<MoveBookmarkFunction>(kTestExtensionPath2,
      new MoveBookmarkFunction(), 8);

  ExtensionsActivityMonitor::Records results;
  monitor->GetAndClearRecords(&results);

  EXPECT_EQ(2U, results.size());
  EXPECT_EQ(5U, results[id1].bookmark_write_count);
  EXPECT_EQ(8U, results[id2].bookmark_write_count);

  generator.NewEvent<GetBookmarksFunction>(kTestExtensionPath2,
      new GetBookmarksFunction(), 3);
  generator.NewEvent<UpdateBookmarkFunction>(kTestExtensionPath2,
      new UpdateBookmarkFunction(), 2);

  // Simulate a commit failure, which augments the active record set with the
  // refugee records.
  monitor->PutRecords(results);
  ExtensionsActivityMonitor::Records new_records;
  monitor->GetAndClearRecords(&new_records);

  EXPECT_EQ(2U, results.size());
  EXPECT_EQ(id1, new_records[id1].extension_id);
  EXPECT_EQ(id2, new_records[id2].extension_id);
  EXPECT_EQ(5U, new_records[id1].bookmark_write_count);
  EXPECT_EQ(8U + 2U, new_records[id2].bookmark_write_count);
  ui_loop()->DeleteSoon(FROM_HERE, monitor);
}

TEST_F(ExtensionsActivityMonitorTest, MultiGet) {
  ExtensionsActivityMonitor* monitor = new ExtensionsActivityMonitor();
  BookmarkAPIEventGenerator generator;
  std::string id1 = GetExtensionIdForPath(kTestExtensionPath1);

  generator.NewEvent<CreateBookmarkFunction>(kTestExtensionPath1,
      new CreateBookmarkFunction(), 5);

  ExtensionsActivityMonitor::Records results;
  monitor->GetAndClearRecords(&results);

  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(5U, results[id1].bookmark_write_count);

  monitor->GetAndClearRecords(&results);
  EXPECT_TRUE(results.empty());

  generator.NewEvent<CreateBookmarkFunction>(kTestExtensionPath1,
      new CreateBookmarkFunction(), 3);
  monitor->GetAndClearRecords(&results);

  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(3U, results[id1].bookmark_write_count);

  ui_loop()->DeleteSoon(FROM_HERE, monitor);
}
