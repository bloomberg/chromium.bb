// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.registerCategory;

import android.os.SystemClock;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.MotionEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.test.BottomSheetTestCaseBase;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.content.browser.test.util.TestTouchUtils;

/**
 * Instrumentation tests for {@link SuggestionsBottomSheetContent}.
 */
public class SuggestionsBottomSheetTest extends BottomSheetTestCaseBase {
    private FakeSuggestionsSource mSuggestionsSource;

    @Override
    protected void setUp() throws Exception {
        mSuggestionsSource = new FakeSuggestionsSource();
        registerCategory(mSuggestionsSource, /* category = */ 42, /* count = */ 5);
        SuggestionsBottomSheetContent.setSuggestionsSourceForTesting(mSuggestionsSource);
        SuggestionsBottomSheetContent.setEventReporterForTesting(
                new DummySuggestionsEventReporter());
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        SuggestionsBottomSheetContent.setSuggestionsSourceForTesting(null);
        SuggestionsBottomSheetContent.setEventReporterForTesting(null);
        super.tearDown();
    }

    @RetryOnFailure
    @MediumTest
    public void testContextMenu() throws InterruptedException {
        SuggestionsRecyclerView recyclerView =
                (SuggestionsRecyclerView) getBottomSheetContent().getContentView().findViewById(
                        R.id.recycler_view);

        ViewHolder firstCardViewHolder = RecyclerViewTestUtils.waitForView(recyclerView, 2);
        assertEquals(firstCardViewHolder.getItemViewType(), ItemViewType.SNIPPET);

        assertFalse(getActivity().getBottomSheet().onInterceptTouchEvent(createTapEvent()));

        TestTouchUtils.longClickView(getInstrumentation(), firstCardViewHolder.itemView);
        assertTrue(getActivity().getBottomSheet().onInterceptTouchEvent(createTapEvent()));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().closeContextMenu();
            }
        });

        assertFalse(getActivity().getBottomSheet().onInterceptTouchEvent(createTapEvent()));
    }

    private static MotionEvent createTapEvent() {
        return MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                MotionEvent.ACTION_DOWN, 0f, 0f, 0);
    }
}
