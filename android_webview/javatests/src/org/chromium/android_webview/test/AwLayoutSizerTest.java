// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    private final int firstContentWidth = 101;
    private final int firstContentHeight = 389;
    private final int secondContentWidth = 103;
    private final int secondContentHeight = 397;

    public void testCanQueryContentSize() {
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        AwLayoutSizer layoutSizer = new AwLayoutSizer(delegate);
        final int contentWidth = 101;
        final int contentHeight = 389;

        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        assertTrue(delegate.setMeasuredDimensionCalled);
        assertEquals(contentWidth, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        assertEquals(contentHeight, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    public void testContentSizeChangeRequestsLayout() {
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        AwLayoutSizer layoutSizer = new AwLayoutSizer(delegate);

        layoutSizer.onContentSizeChanged(firstContentWidth, firstContentHeight);
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(secondContentWidth, secondContentWidth);

        assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    public void testContentSizeChangeDoesNotRequestLayoutIfMeasuredExcatly() {
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        AwLayoutSizer layoutSizer = new AwLayoutSizer(delegate);

        layoutSizer.onContentSizeChanged(firstContentWidth, firstContentHeight);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(secondContentWidth, firstContentHeight);

        assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    public void testContentHeightGrowsTillAtMostSize() {
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        AwLayoutSizer layoutSizer = new AwLayoutSizer(delegate);

        final int smallerContentSize = 25;
        final int atMostMeasureSize = 50;
        final int tooLargeContentSize = 100;

        layoutSizer.onContentSizeChanged(smallerContentSize, smallerContentSize);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(atMostMeasureSize, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(atMostMeasureSize, MeasureSpec.AT_MOST));
        assertEquals(atMostMeasureSize, delegate.measuredWidth);
        assertEquals(smallerContentSize, delegate.measuredHeight);

        layoutSizer.onContentSizeChanged(tooLargeContentSize, tooLargeContentSize);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(atMostMeasureSize, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(atMostMeasureSize, MeasureSpec.AT_MOST));
        assertEquals(atMostMeasureSize, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        assertEquals(atMostMeasureSize, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }
}
