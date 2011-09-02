// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread.h"
#include "content/browser/in_process_webkit/dom_storage_context.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockDOMStorageContext : public DOMStorageContext {
 public:
  MockDOMStorageContext(WebKitContext* webkit_context,
                        quota::SpecialStoragePolicy* special_storage_policy)
      : DOMStorageContext(webkit_context, special_storage_policy),
        purge_count_(0) {
  }

  virtual void PurgeMemory() {
    EXPECT_FALSE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
    ++purge_count_;
  }

  int purge_count() const { return purge_count_; }

 private:
  int purge_count_;
};

TEST(WebKitContextTest, Basic) {
  TestBrowserContext browser_context;
  scoped_refptr<WebKitContext> context1(new WebKitContext(
          browser_context.IsOffTheRecord(), browser_context.GetPath(),
          NULL, false, NULL, NULL));
  EXPECT_TRUE(browser_context.GetPath() == context1->data_path());
  EXPECT_TRUE(browser_context.IsOffTheRecord() == context1->is_incognito());

  scoped_refptr<WebKitContext> context2(new WebKitContext(
          browser_context.IsOffTheRecord(), browser_context.GetPath(),
          NULL, false, NULL, NULL));
  EXPECT_TRUE(context1->data_path() == context2->data_path());
  EXPECT_TRUE(context1->is_incognito() == context2->is_incognito());
}

TEST(WebKitContextTest, PurgeMemory) {
  // Start up a WebKit thread for the WebKitContext to call the
  // DOMStorageContext on.
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  BrowserThread webkit_thread(BrowserThread::WEBKIT, &message_loop);

  {
    // Create the contexts.
    TestBrowserContext browser_context;
    scoped_refptr<WebKitContext> context(new WebKitContext(
            browser_context.IsOffTheRecord(), browser_context.GetPath(),
            NULL, false, NULL, NULL));
    MockDOMStorageContext* mock_context = new MockDOMStorageContext(
        context.get(), NULL);
    context->set_dom_storage_context(mock_context);  // Takes ownership.

    // Ensure PurgeMemory() calls our mock object on the right thread.
    EXPECT_EQ(0, mock_context->purge_count());
    context->PurgeMemory();
    MessageLoop::current()->RunAllPending();
    EXPECT_EQ(1, mock_context->purge_count());
  }
  // WebKitContext's destructor posts stuff to the webkit thread.  Let
  // WebKitContext go out of scope here before processing WebKitContext's
  // clean-up tasks.
  MessageLoop::current()->RunAllPending();
}
