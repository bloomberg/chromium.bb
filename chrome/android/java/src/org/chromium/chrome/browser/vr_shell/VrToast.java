// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.content.Context;

/**
 * This class implements a Toast which is similar to Android Toast in VR.
 */

public class VrToast extends android.widget.Toast {
    public VrToast(Context context) {
        super(context);
    }

    @Override
    public void show() {
        // TODO(bshe): implement this.
    }

    @Override
    public void cancel() {
        // TODO(bshe): implement this.
    }
}
