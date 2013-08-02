// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.OverScroller;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwScrollOffsetManager;
import org.chromium.base.test.util.Feature;

public class AwScrollOffsetManagerTest extends InstrumentationTestCase {
    private static class TestScrollOffsetManagerDelegate implements AwScrollOffsetManager.Delegate {
        private int mOverScrollDeltaX;
        private int mOverScrollDeltaY;
        private int mOverScrollCallCount;
        private int mScrollX;
        private int mScrollY;
        private int mNativeScrollX;
        private int mNativeScrollY;
        private int mInvalidateCount;

        public int getOverScrollDeltaX() {
            return mOverScrollDeltaX;
        }

        public int getOverScrollDeltaY() {
            return mOverScrollDeltaY;
        }

        public int getOverScrollCallCount() {
            return mOverScrollCallCount;
        }

        public int getScrollX() {
            return mScrollX;
        }

        public int getScrollY() {
            return mScrollY;
        }

        public int getNativeScrollX() {
            return mNativeScrollX;
        }

        public int getNativeScrollY() {
            return mNativeScrollY;
        }

        public int getInvalidateCount() {
            return mInvalidateCount;
        }

        @Override
        public void overScrollContainerViewBy(int deltaX, int deltaY, int scrollX, int scrollY,
                int scrollRangeX, int scrollRangeY, boolean isTouchEvent) {
            mOverScrollDeltaX = deltaX;
            mOverScrollDeltaY = deltaY;
            mOverScrollCallCount += 1;
        }

        @Override
        public void scrollContainerViewTo(int x, int y) {
            mScrollX = x;
            mScrollY = y;
        }

        @Override
        public void scrollNativeTo(int x, int y) {
            mNativeScrollX = x;
            mNativeScrollY = y;
        }

        @Override
        public int getContainerViewScrollX() {
            return mScrollX;
        }

        @Override
        public int getContainerViewScrollY() {
            return mScrollY;
        }

        @Override
        public void invalidate() {
            mInvalidateCount += 1;
        }
    }

    private void simulateScrolling(AwScrollOffsetManager offsetManager,
            TestScrollOffsetManagerDelegate delegate, int scrollX, int scrollY) {
        // Scrolling is a two-phase action. First we ask the manager to scroll
        int callCount = delegate.getOverScrollCallCount();
        offsetManager.scrollContainerViewTo(scrollX, scrollY);
        // The manager then asks the delegate to overscroll the view.
        assertEquals(callCount + 1, delegate.getOverScrollCallCount());
        assertEquals(scrollX, delegate.getOverScrollDeltaX() + delegate.getScrollX());
        assertEquals(scrollY, delegate.getOverScrollDeltaY() + delegate.getScrollY());
        // As a response to that the menager expects the view to call back with the new scroll.
        offsetManager.onContainerViewOverScrolled(scrollX, scrollY, false, false);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWhenContentSizeMatchesView() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        final int width = 132;
        final int height = 212;
        final int scrollX = 11;
        final int scrollY = 13;

        offsetManager.setContentSize(width, height);
        offsetManager.setContainerViewSize(width, height);

        assertEquals(width, offsetManager.computeHorizontalScrollRange());
        assertEquals(height, offsetManager.computeVerticalScrollRange());

        // Since the view size and contents size are equal no scrolling should be possible.
        assertEquals(0, offsetManager.computeMaximumHorizontalScrollOffset());
        assertEquals(0, offsetManager.computeMaximumVerticalScrollOffset());

        // Scrolling should generate overscroll but not update the scroll offset.
        simulateScrolling(offsetManager, delegate, scrollX, scrollY);
        assertEquals(scrollX, delegate.getOverScrollDeltaX());
        assertEquals(scrollY, delegate.getOverScrollDeltaY());
        assertEquals(0, delegate.getScrollX());
        assertEquals(0, delegate.getScrollY());
        assertEquals(0, delegate.getNativeScrollX());
        assertEquals(0, delegate.getNativeScrollY());

        // Scrolling to 0,0 should result in no deltas.
        simulateScrolling(offsetManager, delegate, 0, 0);
        assertEquals(0, delegate.getOverScrollDeltaX());
        assertEquals(0, delegate.getOverScrollDeltaY());

        // Negative scrolling should result in negative deltas but no scroll offset update.
        simulateScrolling(offsetManager, delegate, -scrollX, -scrollY);
        assertEquals(-scrollX, delegate.getOverScrollDeltaX());
        assertEquals(-scrollY, delegate.getOverScrollDeltaY());
        assertEquals(0, delegate.getScrollX());
        assertEquals(0, delegate.getScrollY());
        assertEquals(0, delegate.getNativeScrollX());
        assertEquals(0, delegate.getNativeScrollY());
    }

    private static final int VIEW_WIDTH = 211;
    private static final int VIEW_HEIGHT = 312;
    private static final int MAX_HORIZONTAL_OFFSET = 61;
    private static final int MAX_VERTICAL_OFFSET = 42;
    private static final int CONTENT_WIDTH = VIEW_WIDTH + MAX_HORIZONTAL_OFFSET;
    private static final int CONTENT_HEIGHT = VIEW_HEIGHT + MAX_VERTICAL_OFFSET;

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScrollRangeAndMaxOffset() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        offsetManager.setContentSize(CONTENT_WIDTH, CONTENT_HEIGHT);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        assertEquals(CONTENT_WIDTH, offsetManager.computeHorizontalScrollRange());
        assertEquals(CONTENT_HEIGHT, offsetManager.computeVerticalScrollRange());

        assertEquals(MAX_HORIZONTAL_OFFSET, offsetManager.computeMaximumHorizontalScrollOffset());
        assertEquals(MAX_VERTICAL_OFFSET, offsetManager.computeMaximumVerticalScrollOffset());

        // Scrolling beyond the maximum should be clamped.
        final int scrollX = MAX_HORIZONTAL_OFFSET + 10;
        final int scrollY = MAX_VERTICAL_OFFSET + 11;

        simulateScrolling(offsetManager, delegate, scrollX, scrollY);
        assertEquals(scrollX, delegate.getOverScrollDeltaX());
        assertEquals(scrollY, delegate.getOverScrollDeltaY());
        assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getScrollX());
        assertEquals(MAX_VERTICAL_OFFSET, delegate.getScrollY());
        assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getNativeScrollX());
        assertEquals(MAX_VERTICAL_OFFSET, delegate.getNativeScrollY());

        // Scrolling to negative coordinates should be clamped back to 0,0.
        simulateScrolling(offsetManager, delegate, -scrollX, -scrollY);
        assertEquals(0, delegate.getScrollX());
        assertEquals(0, delegate.getScrollY());
        assertEquals(0, delegate.getNativeScrollX());
        assertEquals(0, delegate.getNativeScrollY());

        // The onScrollChanged method is callable by third party code and should also be clamped
        offsetManager.onContainerViewScrollChanged(scrollX, scrollY);
        assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getNativeScrollX());
        assertEquals(MAX_VERTICAL_OFFSET, delegate.getNativeScrollY());

        offsetManager.onContainerViewScrollChanged(-scrollX, -scrollY);
        assertEquals(0, delegate.getNativeScrollX());
        assertEquals(0, delegate.getNativeScrollY());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDelegateCanOverrideScroll() {
        final int overrideScrollX = 10;
        final int overrideScrollY = 10;

        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate() {
            @Override
            public int getContainerViewScrollX() {
                return overrideScrollX;
            }

            @Override
            public int getContainerViewScrollY() {
                return overrideScrollY;
            }
        };
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        offsetManager.setContentSize(CONTENT_WIDTH, CONTENT_HEIGHT);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.onContainerViewOverScrolled(0, 0, false, false);
        assertEquals(overrideScrollX, delegate.getNativeScrollX());
        assertEquals(overrideScrollY, delegate.getNativeScrollY());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDelegateOverridenScrollsDontExceedBounds() {
        final int overrideScrollX = 222;
        final int overrideScrollY = 333;
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate() {
            @Override
            public int getContainerViewScrollX() {
                return overrideScrollX;
            }

            @Override
            public int getContainerViewScrollY() {
                return overrideScrollY;
            }
        };
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        offsetManager.setContentSize(CONTENT_WIDTH, CONTENT_HEIGHT);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.onContainerViewOverScrolled(0, 0, false, false);
        assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getNativeScrollX());
        assertEquals(MAX_VERTICAL_OFFSET, delegate.getNativeScrollY());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScrollContainerViewTo() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        final int scrollX = 31;
        final int scrollY = 41;

        offsetManager.setContentSize(CONTENT_WIDTH, CONTENT_HEIGHT);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        assertEquals(0, delegate.getOverScrollDeltaX());
        assertEquals(0, delegate.getOverScrollDeltaY());
        int callCount = delegate.getOverScrollCallCount();

        offsetManager.scrollContainerViewTo(scrollX, scrollY);
        assertEquals(callCount + 1, delegate.getOverScrollCallCount());
        assertEquals(scrollX, delegate.getOverScrollDeltaX());
        assertEquals(scrollY, delegate.getOverScrollDeltaY());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnContainerViewOverScrolled() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        final int scrollX = 31;
        final int scrollY = 41;

        offsetManager.setContentSize(CONTENT_WIDTH, CONTENT_HEIGHT);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        assertEquals(0, delegate.getScrollX());
        assertEquals(0, delegate.getScrollY());
        assertEquals(0, delegate.getNativeScrollX());
        assertEquals(0, delegate.getNativeScrollY());

        offsetManager.onContainerViewOverScrolled(scrollX, scrollY, false, false);
        assertEquals(scrollX, delegate.getScrollX());
        assertEquals(scrollY, delegate.getScrollY());
        assertEquals(scrollX, delegate.getNativeScrollX());
        assertEquals(scrollY, delegate.getNativeScrollY());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDefersScrollUntilTouchEnd() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        final int scrollX = 31;
        final int scrollY = 41;

        offsetManager.setContentSize(CONTENT_WIDTH, CONTENT_HEIGHT);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.setProcessingTouchEvent(true);
        offsetManager.onContainerViewOverScrolled(scrollX, scrollY, false, false);
        assertEquals(scrollX, delegate.getScrollX());
        assertEquals(scrollY, delegate.getScrollY());
        assertEquals(0, delegate.getNativeScrollX());
        assertEquals(0, delegate.getNativeScrollY());

        offsetManager.setProcessingTouchEvent(false);
        assertEquals(scrollX, delegate.getScrollX());
        assertEquals(scrollY, delegate.getScrollY());
        assertEquals(scrollX, delegate.getNativeScrollX());
        assertEquals(scrollY, delegate.getNativeScrollY());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFlingScroll() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        OverScroller scroller = new OverScroller(getInstrumentation().getContext());
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate, scroller);

        offsetManager.flingScroll(0, 101);
        assertTrue(!scroller.isFinished());
        assertTrue(delegate.getInvalidateCount() == 1);
        assertEquals(101, (int) scroller.getCurrVelocity());

        offsetManager.flingScroll(111, 0);
        assertTrue(!scroller.isFinished());
        assertTrue(delegate.getInvalidateCount() == 2);
        assertEquals(111, (int) scroller.getCurrVelocity());
    }
}
