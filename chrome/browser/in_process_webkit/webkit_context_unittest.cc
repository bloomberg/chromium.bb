// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockDOMStorageContext : public DOMStorageContext {
 public:
  explicit MockDOMStorageContext(WebKitContext* webkit_context)
      : DOMStorageContext(webkit_context),
        purge_count_(0) {
  }

  virtual void PurgeMemory() {
    EXPECT_FALSE(ChromeThread::CurrentlyOn(ChromeThread::UI));
    EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
    ++purge_count_;
  }

  int purge_count() const { return purge_count_; }

 private:
  int purge_count_;
};

TEST(WebKitContextTest, Basic) {
  FilePath file_path;
  scoped_refptr<WebKitContext> context1(new WebKitContext(file_path, true));
  EXPECT_TRUE(file_path == context1->data_path());
  EXPECT_TRUE(context1->is_incognito());

  scoped_refptr<WebKitContext> context2(new WebKitContext(file_path, false));
  EXPECT_TRUE(file_path == context2->data_path());
  EXPECT_FALSE(context2->is_incognito());
}

TEST(WebKitContextTest, PurgeMemory) {
  // Start up a WebKit thread for the WebKitContext to call the
  // DOMStorageContext on.
  ChromeThread webkit_thread(ChromeThread::WEBKIT);
  webkit_thread.Start();

  // Create the contexts.
  scoped_refptr<WebKitContext> context(new WebKitContext(FilePath(), false));
  MockDOMStorageContext* mock_context =
      new MockDOMStorageContext(context.get());
  context->set_dom_storage_context(mock_context);  // Takes ownership.

  // Ensure PurgeMemory() calls our mock object on the right thread.
  EXPECT_EQ(0, mock_context->purge_count());
  context->PurgeMemory();
  webkit_thread.Stop();  // Blocks until all tasks are complete.
  EXPECT_EQ(1, mock_context->purge_count());
}
