// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import org.chromium.base.CalledByNative;

/**
 * Container and generator of ShellViews.
 */
public class ShellManager extends FrameLayout {

    private ShellView mActiveShellView;

    /**
     * Constructor for inflating via XML.
     */
    public ShellManager(Context context, AttributeSet attrs) {
        super(context, attrs);
        nativeInit(this);
    }

    /**
     * @return The currently visible shell view or null if one is not showing.
     */
    protected ShellView getActiveShellView() {
        return mActiveShellView;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private int createShell() {
        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        ShellView shellView = (ShellView) inflater.inflate(R.layout.shell_view, null);

        removeAllViews();
        addView(shellView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        mActiveShellView = shellView;

        return shellView.getNativeShellView();
    }

    private static native void nativeInit(Object shellManagerInstance);
}
