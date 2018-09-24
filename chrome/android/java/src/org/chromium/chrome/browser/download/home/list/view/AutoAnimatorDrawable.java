// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.graphics.drawable.Drawable;
import android.support.v7.graphics.drawable.DrawableWrapper;

/**
 * A helper {@link Drawable} that wraps another {@link Drawable} and starts/stops any
 * {@link Animatable} {@link Drawable}s in the {@link Drawable} hierarchy when this {@link Drawable}
 * is shown or hidden.
 */
public class AutoAnimatorDrawable extends DrawableWrapper {
    // Since Drawables default visible to true by default, we might not get a change and start the
    // animation on the first visibility request.
    private boolean mGotVisibilityCall;

    public AutoAnimatorDrawable(Drawable drawable) {
        super(drawable);
    }

    // DrawableWrapper implementation.
    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        boolean changed = super.setVisible(visible, restart);
        if (visible) {
            if (changed || restart || !mGotVisibilityCall) UiUtils.startAnimatedDrawables(this);
        } else {
            UiUtils.stopAnimatedDrawables(this);
        }

        mGotVisibilityCall = true;
        return changed;
    }
}