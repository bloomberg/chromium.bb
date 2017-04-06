// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.view.ViewGroup;

import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for content shell.
 * Extended for testing.
 */
public class ShellViewAndroidDelegate extends ViewAndroidDelegate {
    private final ViewGroup mContainerView;

    public ShellViewAndroidDelegate(ViewGroup containerView) {
        mContainerView = containerView;
    }

    @Override
    public ViewGroup getContainerView() {
        return mContainerView;
    }
}
