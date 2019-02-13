// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/**
 * An interface to be notified about changes to a {@linkt TabModelFilter}.
 */
public interface TabModelFilterObserver {
    // TODO(meiliang): Add individual update listener, like addTab, closeTab, etc.

    /**
     * Called whenever {@link TabModelFilter} changes.
     */
    void update();
}
