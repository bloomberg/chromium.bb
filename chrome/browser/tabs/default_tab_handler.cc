// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/nacl_histogram.h"
#include "chrome/browser/tabs/default_tab_handler.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"

////////////////////////////////////////////////////////////////////////////////
// DefaultTabHandler, public:

DefaultTabHandler::DefaultTabHandler(TabHandlerDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          model_(new TabStripModel(this, delegate->GetProfile()))) {
  UmaNaclHistogramEnumeration(FIRST_TAB_NACL_BASELINE);
  model_->AddObserver(this);
}

DefaultTabHandler::~DefaultTabHandler() {
  // The tab strip should not have any tabs at this point.
  DCHECK(model_->empty());
  model_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// DefaultTabHandler, TabHandler implementation:

TabStripModel* DefaultTabHandler::GetTabStripModel() const {
  return model_.get();
}

////////////////////////////////////////////////////////////////////////////////
// DefaultTabHandler, TabStripModelDelegate implementation:

TabContentsWrapper* DefaultTabHandler::AddBlankTab(bool foreground) {
  UmaNaclHistogramEnumeration(NEW_TAB_NACL_BASELINE);
  return delegate_->AsBrowser()->AddBlankTab(foreground);
}

TabContentsWrapper* DefaultTabHandler::AddBlankTabAt(int index,
                                                     bool foreground) {
  return delegate_->AsBrowser()->AddBlankTabAt(index, foreground);
}

Browser* DefaultTabHandler::CreateNewStripWithContents(
    TabContentsWrapper* detached_contents,
    const gfx::Rect& window_bounds,
    const DockInfo& dock_info,
    bool maximize) {
  return delegate_->AsBrowser()->CreateNewStripWithContents(detached_contents,
                                                            window_bounds,
                                                            dock_info,
                                                            maximize);
}

int DefaultTabHandler::GetDragActions() const {
  return delegate_->AsBrowser()->GetDragActions();
}

TabContentsWrapper* DefaultTabHandler::CreateTabContentsForURL(
    const GURL& url,
    const GURL& referrer,
    Profile* profile,
    PageTransition::Type transition,
    bool defer_load,
    SiteInstance* instance) const {
  return delegate_->AsBrowser()->CreateTabContentsForURL(url,
                                                         referrer,
                                                         profile,
                                                         transition,
                                                         defer_load,
                                                         instance);
}

bool DefaultTabHandler::CanDuplicateContentsAt(int index) {
  return delegate_->AsBrowser()->CanDuplicateContentsAt(index);
}

void DefaultTabHandler::DuplicateContentsAt(int index) {
  delegate_->AsBrowser()->DuplicateContentsAt(index);
}

void DefaultTabHandler::CloseFrameAfterDragSession() {
  delegate_->AsBrowser()->CloseFrameAfterDragSession();
}

void DefaultTabHandler::CreateHistoricalTab(TabContentsWrapper* contents) {
  delegate_->AsBrowser()->CreateHistoricalTab(contents);
}

bool DefaultTabHandler::RunUnloadListenerBeforeClosing(
    TabContentsWrapper* contents) {
  return delegate_->AsBrowser()->RunUnloadListenerBeforeClosing(contents);
}

bool DefaultTabHandler::CanCloseContentsAt(int index) {
  return delegate_->AsBrowser()->CanCloseContentsAt(index);
}

bool DefaultTabHandler::CanBookmarkAllTabs() const {
  return delegate_->AsBrowser()->CanBookmarkAllTabs();
}

void DefaultTabHandler::BookmarkAllTabs() {
  delegate_->AsBrowser()->BookmarkAllTabs();
}

bool DefaultTabHandler::CanCloseTab() const {
  return delegate_->AsBrowser()->CanCloseTab();
}

void DefaultTabHandler::ToggleUseVerticalTabs() {
  delegate_->AsBrowser()->ToggleUseVerticalTabs();
}

bool DefaultTabHandler::CanRestoreTab() {
  return delegate_->AsBrowser()->CanRestoreTab();
}

void DefaultTabHandler::RestoreTab() {
  delegate_->AsBrowser()->RestoreTab();
}

bool DefaultTabHandler::LargeIconsPermitted() const {
  return delegate_->AsBrowser()->LargeIconsPermitted();
}

bool DefaultTabHandler::UseVerticalTabs() const {
  return delegate_->AsBrowser()->UseVerticalTabs();
}

////////////////////////////////////////////////////////////////////////////////
// DefaultTabHandler, TabStripModelObserver implementation:

void DefaultTabHandler::TabInsertedAt(TabContentsWrapper* contents,
                                      int index,
                                      bool foreground) {
  delegate_->AsBrowser()->TabInsertedAt(contents, index, foreground);
}

void DefaultTabHandler::TabClosingAt(TabStripModel* tab_strip_model,
                                     TabContentsWrapper* contents,
                                     int index) {
  delegate_->AsBrowser()->TabClosingAt(tab_strip_model, contents, index);
}

void DefaultTabHandler::TabDetachedAt(TabContentsWrapper* contents, int index) {
  delegate_->AsBrowser()->TabDetachedAt(contents, index);
}

void DefaultTabHandler::TabDeselected(TabContentsWrapper* contents) {
  delegate_->AsBrowser()->TabDeselected(contents);
}

void DefaultTabHandler::TabSelectedAt(TabContentsWrapper* old_contents,
                                      TabContentsWrapper* new_contents,
                                      int index,
                                      bool user_gesture) {
  delegate_->AsBrowser()->TabSelectedAt(old_contents,
                                        new_contents,
                                        index,
                                        user_gesture);
}

void DefaultTabHandler::TabMoved(TabContentsWrapper* contents,
                                 int from_index,
                                 int to_index) {
  delegate_->AsBrowser()->TabMoved(contents, from_index, to_index);
}

void DefaultTabHandler::TabReplacedAt(TabStripModel* tab_strip_model,
                                      TabContentsWrapper* old_contents,
                                      TabContentsWrapper* new_contents,
                                      int index) {
  delegate_->AsBrowser()->TabReplacedAt(tab_strip_model, old_contents,
                                        new_contents, index);
}

void DefaultTabHandler::TabPinnedStateChanged(TabContentsWrapper* contents,
                                              int index) {
  delegate_->AsBrowser()->TabPinnedStateChanged(contents, index);
}

void DefaultTabHandler::TabStripEmpty() {
  delegate_->AsBrowser()->TabStripEmpty();
}

////////////////////////////////////////////////////////////////////////////////
// TabHandler, public:

// static
TabHandler* TabHandler::CreateTabHandler(TabHandlerDelegate* delegate) {
  return new DefaultTabHandler(delegate);
}

