// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.eventfilter.MockEventFilterHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ContextualSearchEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilterHost;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.GestureHandler;
import org.chromium.chrome.browser.compositor.scene_layer.ContextualSearchSceneLayer;

/**
 * Class responsible for testing the ContextualSearchEventFilter.
 */
public class ContextualSearchEventFilterTest extends InstrumentationTestCase
        implements GestureHandler {

    private static final float SEARCH_PANEL_ALMOST_MAXIMIZED_OFFSET_Y_DP = 50.f;
    private static final float SEARCH_BAR_HEIGHT_DP = 100.f;

    private static final float LAYOUT_WIDTH_DP = 600.f;
    private static final float LAYOUT_HEIGHT_DP = 800.f;

    // A small value used to check whether two floats are almost equal.
    private static final float EPSILON = 1e-04f;

    private float mTouchSlopDp;
    private float mDpToPx;

    private float mAlmostMaximizedSearchContentViewOffsetYDp;
    private float mMaximizedSearchContentViewOffsetYDp;

    private float mSearchContentViewVerticalScroll;

    private boolean mWasTapDetectedOnSearchPanel;
    private boolean mWasScrollDetectedOnSearchPanel;
    private boolean mWasTapDetectedOnSearchContentView;
    private boolean mWasScrollDetectedOnSearchContentView;

    private ContextualSearchPanel mContextualSearchPanel;
    private ContextualSearchEventFilterWrapper mEventFilter;

    private boolean mShouldLockHorizontalMotionInSearchContentView;
    private MotionEvent mEventPropagatedToSearchContentView;

    // --------------------------------------------------------------------------------------------
    // MockEventFilterHostWrapper
    // --------------------------------------------------------------------------------------------

    /**
     * Wrapper around MockEventFilterHost used to mimic the event forwarding mechanism
     * of the EventFilterHost.
     */
    public class MockEventFilterHostWrapper extends MockEventFilterHost {
        GestureDetector mGestureDetector;

        public MockEventFilterHostWrapper(Context context) {
            mGestureDetector = new GestureDetector(context, new SimpleOnGestureListener() {
                @Override
                public boolean onSingleTapUp(MotionEvent e) {
                    mWasTapDetectedOnSearchContentView = true;
                    return true;
                }

                @Override
                public boolean onScroll(MotionEvent e1, MotionEvent e2, float dx, float dy) {
                    mWasScrollDetectedOnSearchContentView = true;
                    return true;
                }
            });
        }

        @Override
        public boolean propagateEvent(MotionEvent e) {
            // Check that the event offset is correct.
            if (!mShouldLockHorizontalMotionInSearchContentView) {
                float propagatedEventY = mEventPropagatedToSearchContentView.getY();
                float offsetY = mContextualSearchPanel.getContentY() * mDpToPx;
                assertEquals(propagatedEventY - offsetY, e.getY(), EPSILON);
            }

            // Propagates the event to the GestureDetector in order to be able to tell
            // if the gesture was properly triggered in the SearchContentView.
            return mGestureDetector.onTouchEvent(e);
        }

    }

    // --------------------------------------------------------------------------------------------
    // ContextualSearchEventFilterWrapper
    // --------------------------------------------------------------------------------------------

    /**
     * Wrapper around ContextualSearchEventFilter used by tests.
     */
    public class ContextualSearchEventFilterWrapper extends ContextualSearchEventFilter {
        public ContextualSearchEventFilterWrapper(Context context, EventFilterHost host,
                GestureHandler handler, OverlayPanelManager panelManager) {
            super(context, host, handler, panelManager);
        }

        @Override
        protected float getSearchContentViewVerticalScroll() {
            return mSearchContentViewVerticalScroll;
        }

        @Override
        protected void propagateEventToSearchContentView(MotionEvent e) {
            mEventPropagatedToSearchContentView = MotionEvent.obtain(e);
            super.propagateEventToSearchContentView(e);
            mEventPropagatedToSearchContentView.recycle();
        }
    }

    // --------------------------------------------------------------------------------------------
    // MockContextualSearchPanel
    // --------------------------------------------------------------------------------------------

    /**
     * Mocks the ContextualSearchPanel, so it doesn't create ContentViewCore.
     */
    public static class MockContextualSearchPanel extends ContextualSearchPanel {

        public MockContextualSearchPanel(Context context, LayoutUpdateHost updateHost,
                OverlayPanelManager panelManager) {
            super(context, updateHost, panelManager);
        }

        @Override
        public OverlayPanelContent createNewOverlayPanelContent() {
            return new MockOverlayPanelContent();
        }

        @Override
        protected ContextualSearchSceneLayer createNewContextualSearchSceneLayer() {
            return null;
        }

        /**
         * Override creation and destruction of the ContentViewCore as they rely on native methods.
         */
        private static class MockOverlayPanelContent extends OverlayPanelContent {
            public MockOverlayPanelContent() {
                super(null, null, null);
            }

            @Override
            public void removeLastHistoryEntry(String url, long timeInMs) {}
        }

        @Override
        protected float getPeekPromoHeight() {
            // Android Views are not used in this test so we cannot get the actual height.
            return 0.f;
        }
    }

    // --------------------------------------------------------------------------------------------
    // MockOverlayPanelManager
    // --------------------------------------------------------------------------------------------

    /**
     * OverlayPanelManager that always returns the MockContextualSearchPanel as the active
     * panel.
     */
    private static class MockOverlayPanelManager extends OverlayPanelManager {
        private OverlayPanel mPanel;

        public MockOverlayPanelManager() {
        }

        public void setOverlayPanel(OverlayPanel panel) {
            mPanel = panel;
        }

        @Override
        public OverlayPanel getActivePanel() {
            return mPanel;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Test Suite
    // --------------------------------------------------------------------------------------------

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        Context context = getInstrumentation().getTargetContext();

        mDpToPx = context.getResources().getDisplayMetrics().density;
        mTouchSlopDp = ViewConfiguration.get(context).getScaledTouchSlop() / mDpToPx;

        EventFilterHost eventFilterHost = new MockEventFilterHostWrapper(context);
        MockOverlayPanelManager panelManager = new MockOverlayPanelManager();
        mContextualSearchPanel = new MockContextualSearchPanel(context, null, panelManager);
        panelManager.setOverlayPanel(mContextualSearchPanel);
        mEventFilter = new ContextualSearchEventFilterWrapper(context, eventFilterHost, this,
                panelManager);

        mContextualSearchPanel.setSearchBarHeightForTesting(SEARCH_BAR_HEIGHT_DP);
        mContextualSearchPanel.setHeightForTesting(LAYOUT_HEIGHT_DP);
        mContextualSearchPanel.setIsFullWidthSizePanelForTesting(true);

        // NOTE(pedrosimonetti): This should be called after calling the method
        // setIsFullscreenSizePanelForTesting(), otherwise it will crash the test.
        mContextualSearchPanel.onSizeChanged(LAYOUT_WIDTH_DP, LAYOUT_HEIGHT_DP, false);

        setSearchContentViewVerticalScroll(0);

        mAlmostMaximizedSearchContentViewOffsetYDp =
                SEARCH_PANEL_ALMOST_MAXIMIZED_OFFSET_Y_DP + SEARCH_BAR_HEIGHT_DP;
        mMaximizedSearchContentViewOffsetYDp = SEARCH_BAR_HEIGHT_DP;

        mWasTapDetectedOnSearchPanel = false;
        mWasScrollDetectedOnSearchPanel = false;
        mWasTapDetectedOnSearchContentView = false;
        mWasScrollDetectedOnSearchContentView = false;

        mShouldLockHorizontalMotionInSearchContentView = false;
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapSearchContentView() {
        positionSearchPanelInAlmostMaximizedState();

        // Simulate tap.
        simulateActionDownEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp + 1.f);
        simulateActionUpEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp + 1.f);

        assertFalse(mWasScrollDetectedOnSearchPanel);
        assertFalse(mWasTapDetectedOnSearchPanel);

        assertTrue(mWasTapDetectedOnSearchContentView);
        assertFalse(mWasScrollDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testScrollingSearchContentViewDragsSearchPanel() {
        positionSearchPanelInAlmostMaximizedState();

        // Simulate swipe up sequence.
        simulateActionDownEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mMaximizedSearchContentViewOffsetYDp);
        simulateActionUpEvent(0.f, mMaximizedSearchContentViewOffsetYDp);

        assertTrue(mWasScrollDetectedOnSearchPanel);
        assertFalse(mWasTapDetectedOnSearchPanel);

        assertFalse(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testScrollUpSearchContentView() {
        positionSearchPanelInMaximizedState();

        // Simulate swipe up sequence.
        simulateActionDownEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mMaximizedSearchContentViewOffsetYDp);
        simulateActionUpEvent(0.f, mMaximizedSearchContentViewOffsetYDp);

        assertFalse(mWasScrollDetectedOnSearchPanel);
        assertFalse(mWasTapDetectedOnSearchPanel);

        assertTrue(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testScrollDownSearchContentView() {
        positionSearchPanelInMaximizedState();

        // When the Panel is maximized and the scroll position is greater than zero, a swipe down
        // on the SearchContentView should trigger a scroll on it.
        setSearchContentViewVerticalScroll(100.f);

        // Simulate swipe down sequence.
        simulateActionDownEvent(0.f, mMaximizedSearchContentViewOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp);
        simulateActionUpEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp);

        assertFalse(mWasScrollDetectedOnSearchPanel);
        assertFalse(mWasTapDetectedOnSearchPanel);

        assertTrue(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDragByOverscrollingSearchContentView() {
        positionSearchPanelInMaximizedState();

        // When the Panel is maximized and the scroll position is zero, a swipe down on the
        // SearchContentView should trigger a swipe on the SearchPanel.
        setSearchContentViewVerticalScroll(0.f);

        // Simulate swipe down sequence.
        simulateActionDownEvent(0.f, mMaximizedSearchContentViewOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp);
        simulateActionUpEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp);

        assertTrue(mWasScrollDetectedOnSearchPanel);
        assertFalse(mWasTapDetectedOnSearchPanel);

        assertFalse(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testUnwantedTapDoesNotHappenInSearchContentView() {
        positionSearchPanelInAlmostMaximizedState();

        float searchContentViewOffsetYStart = mAlmostMaximizedSearchContentViewOffsetYDp + 1.f;
        float searchContentViewOffsetYEnd = mMaximizedSearchContentViewOffsetYDp - 1.f;

        // Simulate swipe up to maximized position.
        simulateActionDownEvent(0.f, searchContentViewOffsetYStart);
        simulateActionMoveEvent(0.f, mMaximizedSearchContentViewOffsetYDp);
        positionSearchPanelInMaximizedState();

        // Confirm that the SearchPanel got a scroll event.
        assertTrue(mWasScrollDetectedOnSearchPanel);

        // Continue the swipe up for one more dp. From now on, the events might be forwarded
        // to the SearchContentView.
        simulateActionMoveEvent(0.f, searchContentViewOffsetYEnd);
        simulateActionUpEvent(0.f, searchContentViewOffsetYEnd);

        // But 1 dp is not enough to trigger a scroll in the SearchContentView, and in this
        // particular case, it should also not trigger a tap because the total displacement
        // of the touch gesture is greater than the touch slop.
        float searchContentViewOffsetDelta =
                searchContentViewOffsetYStart - searchContentViewOffsetYEnd;
        assertTrue(Math.abs(searchContentViewOffsetDelta) > mTouchSlopDp);

        assertFalse(mWasTapDetectedOnSearchPanel);

        assertFalse(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDragSearchPanelThenContinuouslyScrollSearchContentView() {
        positionSearchPanelInAlmostMaximizedState();

        // Simulate swipe up to maximized position.
        simulateActionDownEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mMaximizedSearchContentViewOffsetYDp);
        positionSearchPanelInMaximizedState();

        // Confirm that the SearchPanel got a scroll event.
        assertTrue(mWasScrollDetectedOnSearchPanel);

        // Continue the swipe up for one more dp. From now on, the events might be forwarded
        // to the SearchContentView.
        simulateActionMoveEvent(0.f, mMaximizedSearchContentViewOffsetYDp - 1.f);

        // Now keep swiping up an amount greater than the touch slop. In this case a scroll
        // should be triggered in the SearchContentView.
        simulateActionMoveEvent(0.f, mMaximizedSearchContentViewOffsetYDp - 2 * mTouchSlopDp);
        simulateActionUpEvent(0.f, mMaximizedSearchContentViewOffsetYDp - 2 * mTouchSlopDp);

        assertFalse(mWasTapDetectedOnSearchPanel);

        assertTrue(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapSearchPanel() {
        positionSearchPanelInAlmostMaximizedState();

        // Simulate tap.
        simulateActionDownEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp - 1.f);
        simulateActionUpEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp - 1.f);

        assertFalse(mWasScrollDetectedOnSearchPanel);
        assertTrue(mWasTapDetectedOnSearchPanel);

        assertFalse(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testScrollSearchPanel() {
        positionSearchPanelInAlmostMaximizedState();

        // Simulate swipe up sequence.
        simulateActionDownEvent(0.f, mAlmostMaximizedSearchContentViewOffsetYDp - 1.f);
        simulateActionMoveEvent(0.f, mMaximizedSearchContentViewOffsetYDp);
        simulateActionUpEvent(0.f, mMaximizedSearchContentViewOffsetYDp);

        assertTrue(mWasScrollDetectedOnSearchPanel);
        assertFalse(mWasTapDetectedOnSearchPanel);

        assertFalse(mWasScrollDetectedOnSearchContentView);
        assertFalse(mWasTapDetectedOnSearchContentView);
    }

    // --------------------------------------------------------------------------------------------
    // Helpers
    // --------------------------------------------------------------------------------------------

    /**
     * Positions the SearchPanel in the almost maximized state.
     */
    private void positionSearchPanelInAlmostMaximizedState() {
        mContextualSearchPanel.setMaximizedForTesting(false);
        mContextualSearchPanel.setOffsetYForTesting(SEARCH_PANEL_ALMOST_MAXIMIZED_OFFSET_Y_DP);
    }

    /**
     * Positions the SearchPanel in the maximized state.
     */
    private void positionSearchPanelInMaximizedState() {
        mContextualSearchPanel.setMaximizedForTesting(true);
        mContextualSearchPanel.setOffsetYForTesting(0);
    }

    /**
     * Sets the vertical scroll position of the SearchContentView.
     * @param searchContentViewVerticalScroll The vertical scroll position.
     */
    private void setSearchContentViewVerticalScroll(float searchContentViewVerticalScroll) {
        mSearchContentViewVerticalScroll = searchContentViewVerticalScroll;
    }

    /**
     * Simulates a MotionEvent in the ContextualSearchEventFilter.
     * @param action The event's action.
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateEvent(int action, float x, float y) {
        MotionEvent motionEvent = MotionEvent.obtain(0, 0, action, x * mDpToPx, y * mDpToPx, 0);
        mEventFilter.onTouchEventInternal(motionEvent);
    }

    /**
     * Simulates a MotionEvent.ACTION_DOWN in the ContextualSearchEventFilter.
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateActionDownEvent(float x, float y) {
        simulateEvent(MotionEvent.ACTION_DOWN, x, y);
    }

    /**
     * Simulates a MotionEvent.ACTION_MOVE in the ContextualSearchEventFilter.
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateActionMoveEvent(float x, float y) {
        simulateEvent(MotionEvent.ACTION_MOVE, x, y);
    }

    /**
     * Simulates a MotionEvent.ACTION_UP in the ContextualSearchEventFilter.
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateActionUpEvent(float x, float y) {
        simulateEvent(MotionEvent.ACTION_UP, x, y);
    }

    // --------------------------------------------------------------------------------------------
    // SearchPanel GestureHandler
    // --------------------------------------------------------------------------------------------

    @Override
    public void onDown(float x, float y, boolean fromMouse, int buttons) {}

    @Override
    public void onUpOrCancel() {}

    @Override
    public void drag(float x, float y, float dx, float dy, float tx, float ty) {
        mWasScrollDetectedOnSearchPanel = true;
    }

    @Override
    public void click(float x, float y, boolean fromMouse, int buttons) {
        mWasTapDetectedOnSearchPanel = true;
    }

    @Override
    public void fling(float x, float y, float velocityX, float velocityY) {}

    @Override
    public void onLongPress(float x, float y) {}

    @Override
    public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {}
}
