// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.PendingIntent;

/**
 * Abstract away DaydreamImpl class, which may or may not be present at runtime depending on compile
 * flags.
 */
public interface VrDaydreamApi {
    /**
     * Register the intent to launch after phone inserted into a Daydream viewer.
     */
    void registerDaydreamIntent(PendingIntent pendingIntent);

    /**
     * Unregister the intent if any.
     */
    void unregisterDaydreamIntent();

    /**
     * Close the private api.
     */
    void close();
}
