// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.RectF;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.ExternalVideoSurfaceContainer;
import org.chromium.android_webview.test.util.VideoTestUtil;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;

/**
 * A test suite for ExternalVideoSurfaceContainerTest.
 */
public class ExternalVideoSurfaceContainerTest extends AwTestBase {

    // Callback helper to track the position/size of the external surface.
    private static class OnExternalVideoSurfacePositionChanged extends CallbackHelper {
        private RectF mRect;

        public RectF getRectF() {
            return mRect;
        }

        public void notifyCalled(RectF rect) {
            mRect = rect;
            notifyCalled();
        }
    }
    private OnExternalVideoSurfacePositionChanged mOnExternalVideoSurfacePositionChanged =
            new OnExternalVideoSurfacePositionChanged();
    private CallbackHelper mOnRequestExternalVideoSurface = new CallbackHelper();

    private static void waitForVideoSizeChangeTo(OnExternalVideoSurfacePositionChanged helper,
            int callCount, float widthCss, float heightCss) throws Exception {
        final int maxSizeChangeNotificationToWaitFor = 5;
        final float epsilon = 0.000001f;
        for (int i = 1; i <= maxSizeChangeNotificationToWaitFor; ++i) {
            helper.waitForCallback(callCount, i);
            RectF rect = helper.getRectF();
            if (Math.abs(rect.width() - widthCss) < epsilon
                    && Math.abs(rect.height() - heightCss) < epsilon) {
                break;
            }
            assertTrue(i < maxSizeChangeNotificationToWaitFor);
        }
    }

    private class MockExternalVideoSurfaceContainer extends ExternalVideoSurfaceContainer {
        private int mPlayerId = INVALID_PLAYER_ID;

        public MockExternalVideoSurfaceContainer(
                long nativeExternalVideoSurfaceContainer, ContentViewCore contentViewCore) {
            super(nativeExternalVideoSurfaceContainer, contentViewCore);
        }

        @Override
        protected void requestExternalVideoSurface(int playerId) {
            mPlayerId = playerId;
            mOnRequestExternalVideoSurface.notifyCalled();
        }

        @Override
        protected void releaseExternalVideoSurface(int playerId) {
            mPlayerId = INVALID_PLAYER_ID;
        }

        @Override
        protected void destroy() {}

        @Override
        protected void onExternalVideoSurfacePositionChanged(
                int playerId, float left, float top, float right, float bottom) {
            if (mPlayerId != playerId) {
                return;
            }
            mOnExternalVideoSurfacePositionChanged.notifyCalled(
                    new RectF(left, top, right, bottom));
        }

        @Override
        protected void onFrameInfoUpdated() {}
    }

    private void setUpMockExternalVideoSurfaceContainer() {
        ExternalVideoSurfaceContainer.setFactory(new ExternalVideoSurfaceContainer.Factory() {
            @Override
            public ExternalVideoSurfaceContainer create(
                    long nativeContainer, ContentViewCore contentViewCore) {
                return new MockExternalVideoSurfaceContainer(nativeContainer, contentViewCore);
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testEnableVideoOverlayForEmbeddedVideo() throws Throwable {
        setUpMockExternalVideoSurfaceContainer();

        CommandLine.getInstance().appendSwitch("force-use-overlay-embedded-video");

        int onRequestCallCount = mOnRequestExternalVideoSurface.getCallCount();
        int onPositionChangedCallCount = mOnExternalVideoSurfacePositionChanged.getCallCount();

        assertTrue(VideoTestUtil.runVideoTest(this, false, WAIT_TIMEOUT_MS));

        mOnRequestExternalVideoSurface.waitForCallback(onRequestCallCount);
        waitForVideoSizeChangeTo(mOnExternalVideoSurfacePositionChanged,
                                 onPositionChangedCallCount, 150.0f, 150.0f);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDisableVideoOverlayForEmbeddedVideo() throws Throwable {
        setUpMockExternalVideoSurfaceContainer();

        assertTrue(VideoTestUtil.runVideoTest(this, false, WAIT_TIMEOUT_MS));

        assertEquals(0, mOnRequestExternalVideoSurface.getCallCount());
        assertEquals(0, mOnExternalVideoSurfacePositionChanged.getCallCount());
    }
}
