// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/test_guest_view_manager.h"

#include "base/path_service.h"
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
    : GuestViewManager(context),
      seen_guest_removed_(false),
      web_contents_(NULL) {
}

TestGuestViewManager::~TestGuestViewManager() {
}

content::WebContents* TestGuestViewManager::WaitForGuestCreated() {
  if (web_contents_)
    return web_contents_;

  created_message_loop_runner_ = new content::MessageLoopRunner;
  created_message_loop_runner_->Run();
  return web_contents_;
}

void TestGuestViewManager::WaitForGuestDeleted() {
  if (seen_guest_removed_)
    return;

  deleted_message_loop_runner_ = new content::MessageLoopRunner;
  deleted_message_loop_runner_->Run();
}

void TestGuestViewManager::AddGuest(int guest_instance_id,
                                    content::WebContents* guest_web_contents) {
  GuestViewManager::AddGuest(guest_instance_id, guest_web_contents);
  web_contents_ = guest_web_contents;
  seen_guest_removed_ = false;

  if (created_message_loop_runner_.get())
    created_message_loop_runner_->Quit();
}

void TestGuestViewManager::RemoveGuest(int guest_instance_id) {
  GuestViewManager::RemoveGuest(guest_instance_id);
  web_contents_ = NULL;
  seen_guest_removed_ = true;

  if (deleted_message_loop_runner_.get())
    deleted_message_loop_runner_->Quit();
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
