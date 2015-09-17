// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_live_tab.h"

namespace {
const char kContentLiveTabWebContentsUserDataKey[] = "content_live_tab";
}

namespace sessions {

// static
void ContentLiveTab::CreateForWebContents(content::WebContents* contents) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(
      kContentLiveTabWebContentsUserDataKey,
      new ContentLiveTab(contents));
}

// static
ContentLiveTab* ContentLiveTab::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentLiveTab*>(contents->GetUserData(
      kContentLiveTabWebContentsUserDataKey));
}

ContentLiveTab::ContentLiveTab(content::WebContents* contents)
    : web_contents_(contents) {}

ContentLiveTab::~ContentLiveTab() {}

int ContentLiveTab::GetCurrentEntryIndex() {
  return navigation_controller().GetCurrentEntryIndex();
}

int ContentLiveTab::GetPendingEntryIndex() {
  return navigation_controller().GetPendingEntryIndex();
}

sessions::SerializedNavigationEntry ContentLiveTab::GetEntryAtIndex(int index) {
  return sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
      index, *navigation_controller().GetEntryAtIndex(index));
}

sessions::SerializedNavigationEntry ContentLiveTab::GetPendingEntry() {
  return sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
      GetPendingEntryIndex(), *navigation_controller().GetPendingEntry());
}

int ContentLiveTab::GetEntryCount() {
  return navigation_controller().GetEntryCount();
}

void ContentLiveTab::LoadIfNecessary() {
  navigation_controller().LoadIfNecessary();
}

const std::string& ContentLiveTab::GetUserAgentOverride() const {
  return web_contents()->GetUserAgentOverride();
}

}  // namespace sessions
