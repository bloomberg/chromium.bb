// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"

void TabStripObserver::TabStripAddedTabAt(TabStrip* tab_strip, int index) {
}

void TabStripObserver::TabStripMovedTab(TabStrip* tab_strip,
                                        int from_index,
                                        int to_index) {
}

void TabStripObserver::TabStripRemovedTabAt(TabStrip* tab_strip, int index) {
}

void TabStripObserver::TabStripDeleted(TabStrip* tab_strip) {
}
