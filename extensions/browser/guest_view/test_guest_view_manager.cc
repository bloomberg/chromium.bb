// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/test_guest_view_manager.h"

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

TestGuestViewManager::TestGuestViewManager(content::BrowserContext* context)
    : GuestViewManager(context) {
}

TestGuestViewManager::~TestGuestViewManager() {
}

int TestGuestViewManager::GetNumGuests() const {
  return guest_web_contents_by_instance_id_.size();
}

content::WebContents* TestGuestViewManager::GetLastGuestCreated() {
  content::WebContents* web_contents = nullptr;
  for (int i = current_instance_id_; i >= 0; i--) {
    web_contents = GetGuestByInstanceID(i);
    if (web_contents) {
      break;
    }
  }
  return web_contents;
}

void TestGuestViewManager::WaitForAllGuestsDeleted() {
  // Make sure that every guest that was created have been removed.
  for (auto& watcher : guest_web_contents_watchers_)
    watcher->Wait();
}

void TestGuestViewManager::WaitForGuestCreated() {
  created_message_loop_runner_ = new content::MessageLoopRunner;
  created_message_loop_runner_->Run();
}

content::WebContents* TestGuestViewManager::WaitForSingleGuestCreated() {
  if (GetNumGuests() == 0)
    WaitForGuestCreated();

  return GetLastGuestCreated();
}

void TestGuestViewManager::AddGuest(int guest_instance_id,
                                    content::WebContents* guest_web_contents) {
  GuestViewManager::AddGuest(guest_instance_id, guest_web_contents);

  guest_web_contents_watchers_.push_back(
      linked_ptr<content::WebContentsDestroyedWatcher>(
          new content::WebContentsDestroyedWatcher(guest_web_contents)));

  if (created_message_loop_runner_.get())
    created_message_loop_runner_->Quit();
}

void TestGuestViewManager::RemoveGuest(int guest_instance_id) {
  GuestViewManager::RemoveGuest(guest_instance_id);
}

// Test factory for creating test instances of GuestViewManager.
TestGuestViewManagerFactory::TestGuestViewManagerFactory()
    : test_guest_view_manager_(NULL) {
}

TestGuestViewManagerFactory::~TestGuestViewManagerFactory() {
}

GuestViewManager* TestGuestViewManagerFactory::CreateGuestViewManager(
    content::BrowserContext* context) {
  return GetManager(context);
}

// This function gets called from GuestViewManager::FromBrowserContext(),
// where test_guest_view_manager_ is assigned to a linked_ptr that takes care
// of deleting it.
TestGuestViewManager* TestGuestViewManagerFactory::GetManager(
    content::BrowserContext* context) {
  DCHECK(!test_guest_view_manager_);
  test_guest_view_manager_ = new TestGuestViewManager(context);
  return test_guest_view_manager_;
}

}  // namespace extensions
