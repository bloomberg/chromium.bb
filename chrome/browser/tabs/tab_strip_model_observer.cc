// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/tab_strip_model_observer.h"

void TabStripModelObserver::TabInsertedAt(TabContentsWrapper* contents,
                                          int index,
                                          bool foreground) {
}

void TabStripModelObserver::TabClosingAt(TabStripModel* tab_strip_model,
                                         TabContentsWrapper* contents,
                                         int index) {
}

void TabStripModelObserver::TabDetachedAt(TabContentsWrapper* contents,
                                          int index) {
}

void TabStripModelObserver::TabDeselected(TabContentsWrapper* contents) {
}

void TabStripModelObserver::TabSelectedAt(TabContentsWrapper* old_contents,
                                          TabContentsWrapper* new_contents,
                                          int index,
                                          bool user_gesture) {
}

void TabStripModelObserver::TabMoved(TabContentsWrapper* contents,
                                     int from_index,
                                     int to_index) {
}

void TabStripModelObserver::TabChangedAt(TabContentsWrapper* contents,
                                         int index,
                                         TabChangeType change_type) {
}

void TabStripModelObserver::TabReplacedAt(TabStripModel* tab_strip_model,
                                          TabContentsWrapper* old_contents,
                                          TabContentsWrapper* new_contents,
                                          int index) {
}

void TabStripModelObserver::TabPinnedStateChanged(TabContentsWrapper* contents,
                                                  int index) {
}

void TabStripModelObserver::TabMiniStateChanged(TabContentsWrapper* contents,
                                                int index) {
}

void TabStripModelObserver::TabBlockedStateChanged(TabContentsWrapper* contents,
                                                   int index) {
}

void TabStripModelObserver::TabStripEmpty() {}

void TabStripModelObserver::TabStripModelDeleted() {}
