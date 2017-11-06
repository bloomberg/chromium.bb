// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.graphics.Bitmap;
import android.view.ViewGroup;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.blink_public.web.WebCursorInfoType;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for content shell.
 * Extended for testing.
 */
public class ShellViewAndroidDelegate extends ViewAndroidDelegate {
    /**
     * CallbackHelper for cursor update.
     */
    public static class OnCursorUpdateHelper extends CallbackHelper {
        private int mPointerType;
        public void notifyCalled(int type) {
            mPointerType = type;
            notifyCalled();
        }
        public int getPointerType() {
            assert getCallCount() > 0;
            return mPointerType;
        }
    }

    private final ViewGroup mContainerView;
    private final OnCursorUpdateHelper mOnCursorUpdateHelper;

    public ShellViewAndroidDelegate(ViewGroup containerView) {
        mContainerView = containerView;
        mOnCursorUpdateHelper = new OnCursorUpdateHelper();
    }

    @Override
    public ViewGroup getContainerView() {
        return mContainerView;
    }

    public OnCursorUpdateHelper getOnCursorUpdateHelper() {
        return mOnCursorUpdateHelper;
    }

    @Override
    public void onCursorChangedToCustom(Bitmap customCursorBitmap, int hotspotX, int hotspotY) {
        super.onCursorChangedToCustom(customCursorBitmap, hotspotX, hotspotY);
        mOnCursorUpdateHelper.notifyCalled(WebCursorInfoType.CUSTOM);
    }

    @Override
    public void onCursorChanged(int cursorType) {
        super.onCursorChanged(cursorType);
        mOnCursorUpdateHelper.notifyCalled(cursorType);
    }
}
