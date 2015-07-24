// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.content.Context;

import org.chromium.base.annotations.SuppressFBWarnings;

/**
 * Temp version of old class to avoid breaking downstream builds
 * TODO(aberent): Remove once downstream CL lands.
 * FB warnings suppressed because FB doesn't like the two classes having the
 * same simple name, which I can't avoid here.
 */
@SuppressFBWarnings
public abstract class PolicyProvider extends org.chromium.policy.PolicyProvider {
    /**
     * @param context
     */
    protected PolicyProvider(Context context) {
        super(context);
    }

}
