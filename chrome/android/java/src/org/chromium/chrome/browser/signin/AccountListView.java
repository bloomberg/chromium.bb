// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ListView;

/**
* ListView to list accounts on device.
*/
public class AccountListView extends ListView {
    public AccountListView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected float getTopFadingEdgeStrength() {
        // Disable fading out effect at the top of this ListView.
        return 0;
    }
}
