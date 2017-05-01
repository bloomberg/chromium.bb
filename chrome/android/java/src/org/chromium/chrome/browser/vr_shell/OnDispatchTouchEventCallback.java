// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

/**
 * Callback used to execute code additional test code after VrShellImpl's
 * dispatchTouchEvent() runs.
 */
public interface OnDispatchTouchEventCallback {
    /**
     * Runs some code that may need to know if the event was consumed
     * by the parent and if a cardboard event was triggered.
     */
    void onDispatchTouchEvent(boolean parentConsumed, boolean cardboardTriggered);
}
