// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/snapshot_tab_helper.h"

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"

SnapshotTabHelper::SnapshotTabHelper(TabContentsWrapper* wrapper)
    : TabContentsObserver(wrapper->tab_contents()),
      wrapper_(wrapper) {
}

SnapshotTabHelper::~SnapshotTabHelper() {
}

void SnapshotTabHelper::CaptureSnapshot() {
  Send(new ChromeViewMsg_CaptureSnapshot(routing_id()));
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsObserver overrides

bool SnapshotTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SnapshotTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_Snapshot, OnSnapshot)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void SnapshotTabHelper::OnSnapshot(const SkBitmap& bitmap) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN,
      content::Source<TabContentsWrapper>(wrapper_),
      content::Details<const SkBitmap>(&bitmap));
}
