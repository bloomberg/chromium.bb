// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.R;

// TODO(crbug.com/870056): This class purely exists to silence lint errors. Remove this class once
// we have moved resources into VR DFM.
/* package */ class SilenceLintErrors {
    private int[] mRes = new int[] {
            R.anim.stay_hidden,
            R.drawable.vr_services,
    };

    private SilenceLintErrors() {}
}
