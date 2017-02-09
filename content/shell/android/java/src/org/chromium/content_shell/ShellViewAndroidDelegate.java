// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.content.Intent;
import android.view.ViewGroup;

import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for content shell.
 * Extended for testing.
 */
public class ShellViewAndroidDelegate extends ViewAndroidDelegate {
    private final ViewGroup mContainerView;
    private ContentIntentHandler mContentIntentHandler;

    /**
     * Interface used to define/modify what {@link #startContentIntent} does.
     */
    public interface ContentIntentHandler {
        /**
         * Called when intent url from content is received.
         * @param intentUrl intent url.
         */
        void onIntentUrlReceived(String intentUrl);
    }

    public ShellViewAndroidDelegate(ViewGroup containerView) {
        mContainerView = containerView;
    }

    /**
     * Set the {@link ContentIntentHandler} for {@link #starContentIntent}.
     * @param handler Handler to inject to {@link #startContentIntent}.
     */
    public void setContentIntentHandler(ContentIntentHandler handler) {
        mContentIntentHandler = handler;
    }

    @Override
    public void startContentIntent(Intent intent, String intentUrl, boolean isMainFrame) {
        if (mContentIntentHandler != null) mContentIntentHandler.onIntentUrlReceived(intentUrl);
    }

    @Override
    public ViewGroup getContainerView() {
        return mContainerView;
    }
}
