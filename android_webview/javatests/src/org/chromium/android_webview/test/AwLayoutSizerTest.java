// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.view.View;
import android.view.View.MeasureSpec;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwLayoutSizer;

public class AwLayoutSizerTest extends InstrumentationTestCase {
    static class LayoutSizerDelegate implements AwLayoutSizer.Delegate {
        public int requestLayoutCallCount;
        public boolean setMeasuredDimensionCalled;
        public int measuredWidth;
        public int measuredHeight;

        @Override
        public void requestLayout() {
            requestLayoutCallCount++;
        }

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {
            setMeasuredDimensionCalled = true;
            this.measuredWidth = measuredWidth;
            this.measuredHeight = measuredHeight;
        }
    }

    private static final int FIRST_CONTENT_WIDTH = 101;
    private static final int FIRST_CONTENT_HEIGHT = 389;
    private static final int SECOND_CONTENT_WIDTH = 103;
    private static final int SECOND_CONTENT_HEIGHT = 397;

    private static final int SMALLER_CONTENT_SIZE = 25;
    private static final int AT_MOST_MEASURE_SIZE = 50;
    private static final int TOO_LARGE_CONTENT_SIZE = 100;

    private static final double INITIAL_PAGE_SCALE = 1.0;
    private static final double DIP_SCALE = 1.0;

    public void testCanQueryContentSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        final int contentWidth = 101;
        final int contentHeight = 389;

        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        assertTrue(delegate.setMeasuredDimensionCalled);
        assertEquals(contentWidth, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        assertEquals(contentHeight, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    public void testContentSizeChangeRequestsLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, SECOND_CONTENT_WIDTH);

        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    public void testContentSizeChangeDoesNotRequestLayoutIfMeasuredExcatly() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);

        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    public void testDuplicateContentSizeChangeDoesNotRequestLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);

        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    public void testContentHeightGrowsTillAtMostSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);

        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    public void testScaleChangeRequestsLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE + 0.5);

        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    public void testDuplicateScaleChangeDoesNotRequestLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onPageScaleChanged(DIP_SCALE);

        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    public void testScaleChangeGrowsTillAtMostSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        final double tooLargePageScale = 3.00;

        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);

        layoutSizer.onPageScaleChanged(tooLargePageScale);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    public void testFreezeAndUnfreezeDoesntCauseLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.freezeLayoutRequests();
        layoutSizer.unfreezeLayoutRequests();
        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    public void testFreezeInhibitsLayoutRequest() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.freezeLayoutRequests();
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, SECOND_CONTENT_WIDTH);
        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    public void testUnfreezeIssuesLayoutRequest() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.freezeLayoutRequests();
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, SECOND_CONTENT_WIDTH);
        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
        layoutSizer.unfreezeLayoutRequests();
        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }
}
