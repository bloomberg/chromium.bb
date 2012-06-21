// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_extensions_activity_monitor.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_extension_api.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace browser_sync {

namespace {

using content::BrowserThread;
namespace keys = extension_manifest_keys;

// Create and return an extension with the given path.
scoped_refptr<Extension> MakeExtension(const std::string& name) {
  FilePath path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII(name);

  DictionaryValue value;
  value.SetString(keys::kVersion, "1.0.0.0");
  value.SetString(keys::kName, name);
  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      path, Extension::INVALID, value, Extension::NO_FLAGS, &error));
  EXPECT_TRUE(error.empty());
  return extension;
}

// Fire a bookmarks API event from the given extension the given
// number of times.
template <class T>
void FireBookmarksApiEvent(
    const scoped_refptr<Extension>& extension, int repeats) {
  scoped_refptr<T> bookmarks_function(new T());
  bookmarks_function->set_name(T::function_name());
  for (int i = 0; i < repeats; i++) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,
        content::Source<Extension>(extension.get()),
        content::Details<const BookmarksFunction>(bookmarks_function.get()));
  }
}

class SyncChromeExtensionsActivityMonitorTest : public testing::Test {
 public:
  SyncChromeExtensionsActivityMonitorTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        extension1_(MakeExtension("extension1")),
        extension2_(MakeExtension("extension2")),
        id1_(extension1_->id()),
        id2_(extension2_->id()) {}
  virtual ~SyncChromeExtensionsActivityMonitorTest() {}

 private:
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;

 protected:
  ChromeExtensionsActivityMonitor monitor_;
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  // IDs of |extension{1,2}_|.
  const std::string& id1_;
  const std::string& id2_;
};

// Fire some mutating bookmark API events with extension 1, then fire
// some mutating and non-mutating bookmark API events with extension
// 2.  Only the mutating events should be recorded by the
// csync::ExtensionsActivityMonitor.
TEST_F(SyncChromeExtensionsActivityMonitorTest, Basic) {
  FireBookmarksApiEvent<RemoveBookmarkFunction>(extension1_, 1);
  FireBookmarksApiEvent<MoveBookmarkFunction>(extension1_, 1);
  FireBookmarksApiEvent<UpdateBookmarkFunction>(extension1_, 2);
  FireBookmarksApiEvent<CreateBookmarkFunction>(extension1_, 3);
  FireBookmarksApiEvent<SearchBookmarksFunction>(extension1_, 5);
  const uint32 writes_by_extension1 = 1 + 1 + 2 + 3;

  FireBookmarksApiEvent<RemoveTreeBookmarkFunction>(extension2_, 8);
  FireBookmarksApiEvent<GetBookmarkTreeFunction>(extension2_, 13);
  FireBookmarksApiEvent<GetBookmarkChildrenFunction>(extension2_, 21);
  FireBookmarksApiEvent<GetBookmarksFunction>(extension2_, 33);
  const uint32 writes_by_extension2 = 8;

  csync::ExtensionsActivityMonitor::Records results;
  monitor_.GetAndClearRecords(&results);

  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(results.find(id1_) != results.end());
  EXPECT_TRUE(results.find(id2_) != results.end());
  EXPECT_EQ(writes_by_extension1, results[id1_].bookmark_write_count);
  EXPECT_EQ(writes_by_extension2, results[id2_].bookmark_write_count);
}

// Fire some mutating bookmark API events with both extensions.  Then
// get the records, fire some more mutating and non-mutating events,
// and put the old records back.  Those should be merged with the new
// records correctly.
TEST_F(SyncChromeExtensionsActivityMonitorTest, Put) {
  FireBookmarksApiEvent<CreateBookmarkFunction>(extension1_, 5);
  FireBookmarksApiEvent<MoveBookmarkFunction>(extension2_, 8);

  csync::ExtensionsActivityMonitor::Records results;
  monitor_.GetAndClearRecords(&results);

  EXPECT_EQ(2U, results.size());
  EXPECT_EQ(5U, results[id1_].bookmark_write_count);
  EXPECT_EQ(8U, results[id2_].bookmark_write_count);

  FireBookmarksApiEvent<GetBookmarksFunction>(extension2_, 3);
  FireBookmarksApiEvent<UpdateBookmarkFunction>(extension2_, 2);

  // Simulate a commit failure, which augments the active record set with the
  // refugee records.
  monitor_.PutRecords(results);
  csync::ExtensionsActivityMonitor::Records new_records;
  monitor_.GetAndClearRecords(&new_records);

  EXPECT_EQ(2U, results.size());
  EXPECT_EQ(id1_, new_records[id1_].extension_id);
  EXPECT_EQ(id2_, new_records[id2_].extension_id);
  EXPECT_EQ(5U, new_records[id1_].bookmark_write_count);
  EXPECT_EQ(8U + 2U, new_records[id2_].bookmark_write_count);
}

// Fire some mutating bookmark API events and get the records multiple
// times.  The mintor should correctly clear its records every time
// they're returned.
TEST_F(SyncChromeExtensionsActivityMonitorTest, MultiGet) {
  FireBookmarksApiEvent<CreateBookmarkFunction>(extension1_, 5);

  csync::ExtensionsActivityMonitor::Records results;
  monitor_.GetAndClearRecords(&results);

  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(5U, results[id1_].bookmark_write_count);

  monitor_.GetAndClearRecords(&results);
  EXPECT_TRUE(results.empty());

  FireBookmarksApiEvent<CreateBookmarkFunction>(extension1_, 3);
  monitor_.GetAndClearRecords(&results);

  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(3U, results[id1_].bookmark_write_count);
}

}  // namespace

}  // namespace browser_sync
