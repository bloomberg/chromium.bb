// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

void TabStripModelObserver::TabInsertedAt(TabContents* contents,
                                          int index,
                                          bool foreground) {
}

void TabStripModelObserver::TabClosingAt(TabStripModel* tab_strip_model,
                                         TabContents* contents,
                                         int index) {
}

void TabStripModelObserver::TabDetachedAt(TabContents* contents,
                                          int index) {
}

void TabStripModelObserver::TabDeactivated(TabContents* contents) {
}

void TabStripModelObserver::ActiveTabChanged(TabContents* old_contents,
                                             TabContents* new_contents,
                                             int index,
                                             bool user_gesture) {
}

void TabStripModelObserver::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const TabStripSelectionModel& model) {
}

void TabStripModelObserver::TabMoved(TabContents* contents,
                                     int from_index,
                                     int to_index) {
}

void TabStripModelObserver::TabChangedAt(TabContents* contents,
                                         int index,
                                         TabChangeType change_type) {
}

void TabStripModelObserver::TabReplacedAt(TabStripModel* tab_strip_model,
                                          TabContents* old_contents,
                                          TabContents* new_contents,
                                          int index) {
}

void TabStripModelObserver::TabPinnedStateChanged(TabContents* contents,
                                                  int index) {
}

void TabStripModelObserver::TabMiniStateChanged(TabContents* contents,
                                                int index) {
}

void TabStripModelObserver::TabBlockedStateChanged(TabContents* contents,
                                                   int index) {
}

void TabStripModelObserver::TabStripEmpty() {}

void TabStripModelObserver::TabStripModelDeleted() {}

void TabStripModelObserver::ActiveTabClicked(int index) {}
