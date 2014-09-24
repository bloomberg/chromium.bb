// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_TEST_GUEST_VIEW_MANAGER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_TEST_GUEST_VIEW_MANAGER_H_

#include "content/public/test/test_utils.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"

namespace extensions {

class TestGuestViewManager : public GuestViewManager {
 public:
  explicit TestGuestViewManager(content::BrowserContext* context);
  virtual ~TestGuestViewManager();

  content::WebContents* WaitForGuestCreated();
  void WaitForGuestDeleted();

 private:
  // GuestViewManager override:
  virtual void AddGuest(int guest_instance_id,
                        content::WebContents* guest_web_contents) OVERRIDE;
  virtual void RemoveGuest(int guest_instance_id) OVERRIDE;

  bool seen_guest_removed_;
  content::WebContents* web_contents_;
  scoped_refptr<content::MessageLoopRunner> created_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> deleted_message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManager);
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory();
  virtual ~TestGuestViewManagerFactory();

  virtual GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context) OVERRIDE;

  TestGuestViewManager* GetManager(content::BrowserContext* context);

 private:
  TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManagerFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_TEST_GUEST_VIEW_MANAGER_H_
