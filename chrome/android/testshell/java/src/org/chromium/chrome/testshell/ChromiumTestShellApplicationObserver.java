// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.Intent;

/**
 * Monitors events happening in {@link ChromiumTestShellApplication}.
 */
public interface ChromiumTestShellApplicationObserver {
    /**
     * Called when an Intent is about to be broadcast.
     * @return Whether or not the Application should really fire the broadcast.
     */
    public boolean onSendBroadcast(Intent intent);
}
