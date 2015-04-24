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
  TestGuestViewManager(
      content::BrowserContext* context,
      scoped_ptr<guestview::GuestViewManagerDelegate> delegate);
  ~TestGuestViewManager() override;

  void WaitForAllGuestsDeleted();
  content::WebContents* WaitForSingleGuestCreated();

  content::WebContents* GetLastGuestCreated();

  // Returns the number of guests currently still alive at the time of calling
  // this method.
  int GetNumGuestsActive() const;

  // Returns the size of the set of removed instance IDs.
  int GetNumRemovedInstanceIDs() const;

  // Returns the number of guests that have been created since the creation of
  // this GuestViewManager.
  int num_guests_created() const { return num_guests_created_; }

  // Returns the last guest instance ID removed from the manager.
  int last_instance_id_removed() const { return last_instance_id_removed_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(GuestViewManagerTest, AddRemove);

  // GuestViewManager override:
  void AddGuest(int guest_instance_id,
                content::WebContents* guest_web_contents) override;
  void RemoveGuest(int guest_instance_id) override;

  void WaitForGuestCreated();

  using GuestViewManager::last_instance_id_removed_;
  using GuestViewManager::removed_instance_ids_;

  int num_guests_created_;

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
      content::BrowserContext* context,
      scoped_ptr<guestview::GuestViewManagerDelegate> delegate) override;

 private:
  TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManagerFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_TEST_GUEST_VIEW_MANAGER_H_
