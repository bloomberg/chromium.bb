// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

TestTabStripModelDelegate::TestTabStripModelDelegate() {
}

TestTabStripModelDelegate::~TestTabStripModelDelegate() {
}

void TestTabStripModelDelegate::AddBlankTabAt(int index, bool foreground) {
}

Browser* TestTabStripModelDelegate::CreateNewStripWithContents(
    const std::vector<NewStripContents>& contentses,
    const gfx::Rect& window_bounds,
    const DockInfo& dock_info,
    bool maximize) {
  return NULL;
}

void TestTabStripModelDelegate::WillAddWebContents(
    content::WebContents* contents) {
  // TEMPORARY: Until TabStripModel is fully de-TabContents-ed, it requires all
  // items in it to be TabContentses.
  if (!TabContents::FromWebContents(contents))
    TabContents::Factory::CreateTabContents(contents);

  // Required to determine reloadability of tabs.
  CoreTabHelper::CreateForWebContents(contents);
  // Required to determine if tabs are app tabs.
  extensions::TabHelper::CreateForWebContents(contents);
}

int TestTabStripModelDelegate::GetDragActions() const {
  return 0;
}

bool TestTabStripModelDelegate::CanDuplicateContentsAt(int index) {
  return false;
}

void TestTabStripModelDelegate::DuplicateContentsAt(int index) {
}

void TestTabStripModelDelegate::CloseFrameAfterDragSession() {
}

void TestTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
}

bool TestTabStripModelDelegate::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return true;
}

bool TestTabStripModelDelegate::CanRestoreTab() {
  return false;
}

void TestTabStripModelDelegate::RestoreTab() {
}

bool TestTabStripModelDelegate::CanBookmarkAllTabs() const {
  return true;
}

void TestTabStripModelDelegate::BookmarkAllTabs() {
}
