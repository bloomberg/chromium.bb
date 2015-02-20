// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_TEST_GUEST_VIEW_MANAGER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_TEST_GUEST_VIEW_MANAGER_H_

#include "base/memory/linked_ptr.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"

namespace extensions {

class TestGuestViewManager : public GuestViewManager {
 public:
  explicit TestGuestViewManager(content::BrowserContext* context);
  ~TestGuestViewManager() override;

  void WaitForAllGuestsDeleted();
  content::WebContents* WaitForSingleGuestCreated();

  content::WebContents* GetLastGuestCreated();

 private:
  // GuestViewManager override:
  void AddGuest(int guest_instance_id,
                content::WebContents* guest_web_contents) override;
  void RemoveGuest(int guest_instance_id) override;

  int GetNumGuests() const;
  void WaitForGuestCreated();

  std::vector<linked_ptr<content::WebContentsDestroyedWatcher>>
      guest_web_contents_watchers_;
  scoped_refptr<content::MessageLoopRunner> created_message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManager);
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory();
  ~TestGuestViewManagerFactory() override;

  GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context) override;

  TestGuestViewManager* GetManager(content::BrowserContext* context);

 private:
  TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManagerFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_TEST_GUEST_VIEW_MANAGER_H_
