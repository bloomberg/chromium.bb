// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.view.KeyEvent;

/** A controller for touchless UI. */
public interface TouchlessUiController {
    /**
     * A notification that a key event occurred.
     * @param event The event object.
     * @return Whether the event was consumed.
     */
    boolean onKeyEvent(KeyEvent event);

    /**
     * Clean up anything that needs to be.
     */
    void destroy();
}
