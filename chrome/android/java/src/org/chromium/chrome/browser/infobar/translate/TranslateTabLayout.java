// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar.translate;

import android.content.Context;
import android.support.design.widget.TabLayout;
import android.util.AttributeSet;

/**
 * Lays out an infobar that has all of its controls along a line.
 * It's currently used for Translate UI redesign only.
 */
public class TranslateTabLayout extends TabLayout {
    /**
     * Constructor for inflating from XML.
     */
    public TranslateTabLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    // TODO(martiw): Add special methods on demand.
}
