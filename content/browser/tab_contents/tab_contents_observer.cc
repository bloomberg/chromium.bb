// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_observer.h"

#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"

TabContentsObserver::TabContentsObserver(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      routing_id_(tab_contents->render_view_host()->routing_id()) {
  tab_contents_->AddObserver(this);
}

TabContentsObserver::~TabContentsObserver() {
  if (tab_contents_)
    tab_contents_->RemoveObserver(this);
}

bool TabContentsObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool TabContentsObserver::Send(IPC::Message* message) {
  if (!tab_contents_->render_view_host()) {
    delete message;
    return false;
  }

  return tab_contents_->render_view_host()->Send(message);
}
