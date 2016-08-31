// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp_public;

import android.content.Context;

/**
 * BlimpClientContextDelegate contains all embedder's Java functions used by Blimp Java classes.
 */
public interface BlimpClientContextDelegate {
    /**
     * Request embedder to restart browser.
     */
    public void restartBrowser();

    /**
     * Start user sign in flow.
     */
    public void startUserSignInFlow(Context context);
}
