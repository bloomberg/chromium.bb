// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.View.MeasureSpec;

import org.chromium.android_webview.AwLayoutSizer;
import org.chromium.base.test.util.Feature;

public class AwLayoutSizerTest extends InstrumentationTestCase {
    static class LayoutSizerDelegate implements AwLayoutSizer.Delegate {
        public int requestLayoutCallCount;
        public boolean setMeasuredDimensionCalled;
        public int measuredWidth;
        public int measuredHeight;
        public int fixedLayoutWidth;
        public int fixedLayoutHeight;
        public boolean heightWrapContent;

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

        @Override
        public void setFixedLayoutSize(int widthDip, int heightDip) {
            fixedLayoutWidth = widthDip;
            fixedLayoutHeight = heightDip;
        }

        @Override
        public boolean isLayoutParamsHeightWrapContent() {
            return heightWrapContent;
        }
    }

    private static final int FIRST_CONTENT_WIDTH = 101;
    private static final int FIRST_CONTENT_HEIGHT = 389;
    private static final int SECOND_CONTENT_WIDTH = 103;
    private static final int SECOND_CONTENT_HEIGHT = 397;

    private static final int SMALLER_CONTENT_SIZE = 25;
    private static final int AT_MOST_MEASURE_SIZE = 50;
    private static final int TOO_LARGE_CONTENT_SIZE = 100;

    private static final float INITIAL_PAGE_SCALE = 1.0f;
    private static final double DIP_SCALE = 1.0;

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentHeightGrowthRequestsLayoutInAtMostSizeMode() {
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

        int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, AT_MOST_MEASURE_SIZE - 1);
        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);

        // Test that crossing the AT_MOST_MEASURE_SIZE threshold results in a requestLayout.
        requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, AT_MOST_MEASURE_SIZE + 1);
        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentHeightShrinksAfterAtMostSize() {
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

        int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE + 1);
        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);

        requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
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
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE + 0.5f);

        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
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
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);

        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScaleChangeGrowsTillAtMostSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        final float tooLargePageScale = 3.00f;

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

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
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

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewportWithExactMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);

        final float dipScale = 2.0f;
        final int measuredWidth = 800;
        final int measuredHeight = 400;

        layoutSizer.setDIPScale(dipScale);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(measuredWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(measuredHeight, MeasureSpec.EXACTLY));
        assertEquals(measuredWidth, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        assertEquals(measuredHeight, delegate.measuredHeight & View.MEASURED_SIZE_MASK);

        layoutSizer.onSizeChanged(measuredWidth, measuredHeight, 0, 0);

        assertEquals(0, delegate.fixedLayoutWidth);
        assertEquals(0, delegate.fixedLayoutHeight);

        layoutSizer.onPageScaleChanged(2.0f);

        assertEquals(0, delegate.fixedLayoutWidth);
        assertEquals(0, delegate.fixedLayoutHeight);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewportDipSizeOverrideRounding() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);

        final float dipScale = 0.666f;

        int contentWidth = 9;
        int contentHeight = 6;

        layoutSizer.setDIPScale(dipScale);
        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        assertTrue(delegate.setMeasuredDimensionCalled);
        int measuredWidth = delegate.measuredWidth & View.MEASURED_SIZE_MASK;
        int measuredHeight = delegate.measuredHeight & View.MEASURED_SIZE_MASK;
        assertFalse((int) Math.ceil(measuredWidth / dipScale) == contentWidth);
        assertFalse((int) Math.ceil(measuredHeight / dipScale) == contentHeight);

        layoutSizer.onSizeChanged(measuredWidth, measuredHeight, 0, 0);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewportWithUnspecifiedMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);

        final float dipScale = 1.5f;
        final int pageScale = 2;
        final int dipAndPageScale = (int) (dipScale * pageScale);

        int contentWidth = 800;
        int contentHeight = 400;
        int atMostWidth = contentWidth * dipAndPageScale;
        int atMostHeight = contentHeight * dipAndPageScale;

        layoutSizer.setDIPScale(dipScale);
        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onPageScaleChanged(pageScale);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        assertTrue(delegate.setMeasuredDimensionCalled);
        int measuredWidth = delegate.measuredWidth & View.MEASURED_SIZE_MASK;
        int measuredHeight = delegate.measuredHeight & View.MEASURED_SIZE_MASK;

        int sizeWidth = measuredWidth;
        int sizeHeight = measuredHeight;
        layoutSizer.onSizeChanged(sizeWidth, sizeHeight, 0, 0);

        assertEquals(contentWidth, delegate.fixedLayoutWidth);
        assertEquals(AwLayoutSizer.FIXED_LAYOUT_HEIGHT, delegate.fixedLayoutHeight);

        sizeWidth = measuredWidth * 2;
        sizeHeight = measuredHeight * 2;
        layoutSizer.onSizeChanged(sizeWidth, sizeHeight, 0, 0);

        assertEquals(sizeWidth / dipAndPageScale, delegate.fixedLayoutWidth);
        assertEquals(AwLayoutSizer.FIXED_LAYOUT_HEIGHT, delegate.fixedLayoutHeight);

        sizeWidth = measuredWidth / 2;
        sizeHeight = measuredHeight / 2;
        layoutSizer.onSizeChanged(sizeWidth, sizeHeight, 0, 0);

        assertEquals(sizeWidth / dipAndPageScale, delegate.fixedLayoutWidth);
        assertEquals(AwLayoutSizer.FIXED_LAYOUT_HEIGHT, delegate.fixedLayoutHeight);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewportWithAtMostMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        delegate.heightWrapContent = true;
        layoutSizer.setDelegate(delegate);

        final float dipScale = 1.5f;
        final int pageScale = 2;
        final int dipAndPageScale = (int) (dipScale * pageScale);

        int contentWidth = 800;
        int contentHeight = 400;
        int contentWidthPix = contentWidth * dipAndPageScale;
        int contentHeightPix = contentHeight * dipAndPageScale;

        layoutSizer.setDIPScale(dipScale);
        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onPageScaleChanged(pageScale);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(contentWidthPix, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(contentHeightPix * 2, MeasureSpec.AT_MOST));

        assertTrue(delegate.setMeasuredDimensionCalled);
        int measuredWidth = delegate.measuredWidth & View.MEASURED_SIZE_MASK;
        int measuredHeight = delegate.measuredHeight & View.MEASURED_SIZE_MASK;

        int sizeWidth = measuredWidth;
        int sizeHeight = measuredHeight;
        layoutSizer.onSizeChanged(sizeWidth, sizeHeight, 0, 0);

        assertEquals(contentWidth, delegate.fixedLayoutWidth);
        assertEquals(AwLayoutSizer.FIXED_LAYOUT_HEIGHT, delegate.fixedLayoutHeight);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFixedLayoutViewportGoesBackToZeroWithWrapContentMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        layoutSizer.onSizeChanged((int) (FIRST_CONTENT_WIDTH * DIP_SCALE),
                (int) (FIRST_CONTENT_HEIGHT * DIP_SCALE), 0, 0);

        assertTrue(delegate.fixedLayoutWidth != 0);
        assertEquals(AwLayoutSizer.FIXED_LAYOUT_HEIGHT, delegate.fixedLayoutHeight);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, AwLayoutSizer.FIXED_LAYOUT_HEIGHT);
        layoutSizer.onSizeChanged((int) (FIRST_CONTENT_WIDTH * DIP_SCALE),
                (int) (FIRST_CONTENT_HEIGHT * DIP_SCALE), 0, 0);
        assertTrue(delegate.fixedLayoutWidth != 0);
        assertEquals(0, delegate.fixedLayoutHeight);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, 0);
        layoutSizer.onSizeChanged((int) (FIRST_CONTENT_WIDTH * DIP_SCALE),
                (int) (FIRST_CONTENT_HEIGHT * DIP_SCALE), 0, 0);
        assertTrue(delegate.fixedLayoutWidth != 0);
        assertEquals(0, delegate.fixedLayoutHeight);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFixedLayoutSizeUpdatedOnPageScaleChangeItNoLayoutRequest() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        delegate.heightWrapContent = true;
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        layoutSizer.onSizeChanged(AT_MOST_MEASURE_SIZE, AT_MOST_MEASURE_SIZE, 0, 0);

        assertTrue(delegate.fixedLayoutWidth != 0);
        final int fixedLayoutWidth = delegate.fixedLayoutWidth;
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE * 2f);
        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
        assertEquals(fixedLayoutWidth / 2, delegate.fixedLayoutWidth);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFixedLayoutSizeUpdatedIfNoSizeChangeAfterLayoutRequested() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        layoutSizer.onSizeChanged((int) (FIRST_CONTENT_WIDTH * DIP_SCALE),
                (int) (FIRST_CONTENT_HEIGHT * DIP_SCALE), 0, 0);

        assertTrue(delegate.fixedLayoutWidth != 0);
        final int fixedLayoutWidth = delegate.fixedLayoutWidth;
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE * 0.5f);
        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
        assertEquals(fixedLayoutWidth, delegate.fixedLayoutWidth);

        // onMeasure and onLayoutChange should always be called as a result of the AwLayoutSizer
        // calling Delegate.requestLayout.
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        layoutSizer.onLayoutChange();

        assertEquals(fixedLayoutWidth * 2, delegate.fixedLayoutWidth);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFixedLayoutSizeDoesNotDependOnMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        delegate.heightWrapContent = false;
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        layoutSizer.onSizeChanged(AT_MOST_MEASURE_SIZE, AT_MOST_MEASURE_SIZE, 0, 0);

        assertEquals(0, delegate.fixedLayoutWidth);
        assertEquals(0, delegate.fixedLayoutHeight);
    }
}
