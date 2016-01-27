// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.graphics.PointF;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.widget.FrameLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeEventFilter.ScrollDirection;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel.MockTabModelDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content.browser.BrowserStartupController;

/**
 * Unit tests for {@link org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome}
 */
public class LayoutManagerTest extends InstrumentationTestCase
        implements MockTabModelDelegate {
    private static final String TAG = "LayoutManagerTest";

    private long mLastDownTime = 0;

    private TabModelSelector mTabModelSelector;
    private LayoutManagerChrome mManager;
    private LayoutManagerChromePhone mManagerPhone;

    private final PointerProperties[] mProperties = new PointerProperties[2];
    private final PointerCoords[] mPointerCoords = new PointerCoords[2];

    private float mDpToPx;

    private void initializeMotionEvent() {
        mProperties[0] = new PointerProperties();
        mProperties[0].id = 0;
        mProperties[0].toolType = MotionEvent.TOOL_TYPE_FINGER;
        mProperties[1] = new PointerProperties();
        mProperties[1].id = 1;
        mProperties[1].toolType = MotionEvent.TOOL_TYPE_FINGER;

        mPointerCoords[0] = new PointerCoords();
        mPointerCoords[0].x = 0;
        mPointerCoords[0].y = 0;
        mPointerCoords[0].pressure = 1;
        mPointerCoords[0].size = 1;
        mPointerCoords[1] = new PointerCoords();
        mPointerCoords[1].x = 0;
        mPointerCoords[1].y = 0;
        mPointerCoords[1].pressure = 1;
        mPointerCoords[1].size = 1;
    }

    /**
     * Simulates time so the animation updates.
     * @param layoutManager The {@link LayoutManagerChrome} to update.
     * @param maxFrameCount The maximum number of frames to simulate before the motion ends.
     * @return              Whether the maximum number of frames was enough for the
     *                      {@link LayoutManagerChrome} to reach the end of the animations.
     */
    private static boolean simulateTime(LayoutManagerChrome layoutManager, int maxFrameCount) {
        // Simulating time
        int frame = 0;
        long time = 0;
        final long dt = 16;
        while (layoutManager.onUpdate(time, dt) && frame < maxFrameCount) {
            time += dt;
            frame++;
        }
        Log.w(TAG, "simulateTime frame " + frame);
        return frame < maxFrameCount;
    }

    private void initializeLayoutManagerPhone(int standardTabCount, int incognitoTabCount) {
        initializeLayoutManagerPhone(standardTabCount, incognitoTabCount,
                TabModel.INVALID_TAB_INDEX, TabModel.INVALID_TAB_INDEX, false);
    }

    private void initializeLayoutManagerPhone(int standardTabCount, int incognitoTabCount,
            int standardIndexSelected, int incognitoIndexSelected, boolean incognitoSelected) {
        Context context = new MockContextForLayout(getInstrumentation().getContext());

        mDpToPx = context.getResources().getDisplayMetrics().density;

        mTabModelSelector = new MockTabModelSelector(standardTabCount, incognitoTabCount, this);
        if (standardIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(false), standardIndexSelected);
        }
        if (incognitoIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(true), incognitoIndexSelected);
        }
        mTabModelSelector.selectModel(incognitoSelected);
        LayoutManagerHost layoutManagerHost = new MockLayoutHost(context);

        // Build a fake content container
        FrameLayout parentContainer = new FrameLayout(context);
        FrameLayout container = new FrameLayout(context);
        parentContainer.addView(container);

        mManagerPhone = new LayoutManagerChromePhone(
                layoutManagerHost, new LayoutManagerChromePhone.OverviewLayoutFactoryDelegate() {
                    @Override
                    public Layout createOverviewLayout(Context context, LayoutUpdateHost updateHost,
                            LayoutRenderHost renderHost, EventFilter eventFilter) {
                        return new StackLayout(context, updateHost, renderHost, eventFilter);
                    }
                });
        mManager = mManagerPhone;
        mManager.init(mTabModelSelector, null, null, container, null, null, null);
        initializeMotionEvent();
    }

    private void eventDown(long time, PointF p) {
        mLastDownTime = time;

        mPointerCoords[0].x = p.x * mDpToPx;
        mPointerCoords[0].y = p.y * mDpToPx;

        MotionEvent event = MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_DOWN,
                1, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0);
        assertTrue("Down event not intercepted", mManager.onInterceptTouchEvent(event, false));
        assertTrue("Down event not handled", mManager.onTouchEvent(event));
    }

    private void eventDown1(long time, PointF p) {
        mPointerCoords[1].x = p.x * mDpToPx;
        mPointerCoords[1].y = p.y * mDpToPx;

        assertTrue("Down_1 event not handled", mManager.onTouchEvent(
                MotionEvent.obtain(mLastDownTime, time,
                MotionEvent.ACTION_POINTER_DOWN | (0x1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                2, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventMove(long time, PointF p) {
        mPointerCoords[0].x = p.x * mDpToPx;
        mPointerCoords[0].y = p.y * mDpToPx;

        assertTrue("Move event not handled", mManager.onTouchEvent(
                MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_MOVE,
                1, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventUp(long time, PointF p) {
        mPointerCoords[0].x = p.x * mDpToPx;
        mPointerCoords[0].y = p.y * mDpToPx;

        assertTrue("Up event not handled", mManager.onTouchEvent(
                MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_UP,
                1, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventUp1(long time, PointF p) {
        mPointerCoords[1].x = p.x * mDpToPx;
        mPointerCoords[1].y = p.y * mDpToPx;

        assertTrue("Up_1 event not handled", mManager.onTouchEvent(
                MotionEvent.obtain(mLastDownTime, time,
                MotionEvent.ACTION_POINTER_UP | (0x1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                2, mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    private void eventMoveBoth(long time, PointF p0, PointF p1) {
        mPointerCoords[0].x = p0.x * mDpToPx;
        mPointerCoords[0].y = p0.y * mDpToPx;
        mPointerCoords[1].x = p1.x * mDpToPx;
        mPointerCoords[1].y = p1.y * mDpToPx;

        assertTrue("Move event not handled", mManager.onTouchEvent(
                MotionEvent.obtain(mLastDownTime, time, MotionEvent.ACTION_MOVE, 2,
                mProperties, mPointerCoords, 0, 0, 1, 1, 0, 0, 0, 0)));
    }

    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCreation() {
        initializeLayoutManagerPhone(0, 0);
    }

    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testStack() throws Exception {
        initializeLayoutManagerPhone(3, 0);
        mManagerPhone.showOverview(true);
        assertTrue("layoutManager is way too long to end motion", simulateTime(mManager, 1000));
        assertTrue("The activate layout type is expected to be StackLayout",
                mManager.getActiveLayout() instanceof StackLayout);
        mManagerPhone.hideOverview(true);
        assertTrue("layoutManager is way too long to end motion", simulateTime(mManager, 1000));
    }

    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testStackNoAnimation() throws Exception {
        initializeLayoutManagerPhone(1, 0);
        mManagerPhone.showOverview(false);
        assertTrue("The activate layout type is expected to be StackLayout",
                mManager.getActiveLayout() instanceof StackLayout);
        mManagerPhone.hideOverview(false);
    }

    /**
     * Tests the tab pinching behavior with two finger.
     * This test is still under development.
     */
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testStackPinch() throws Exception {
        initializeLayoutManagerPhone(5, 0);
        // Setting the index to the second to last element ensure the stack can be scrolled in both
        // directions.
        mManager.tabSelected(mTabModelSelector.getCurrentModel().getTabAt(3).getId(),
                Tab.INVALID_TAB_ID, false);

        mManagerPhone.showOverview(false);
        // Basic verifications
        assertTrue("The activate layout type is expected to be StackLayout",
                mManager.getActiveLayout() instanceof StackLayout);

        StackLayout layout = (StackLayout) mManager.getActiveLayout();
        Stack stack = layout.getTabStack(false);
        StackTab[] tabs = stack.getTabs();

        long time = 0;
        // At least one update is necessary to get updated positioning of LayoutTabs.
        mManager.onUpdate(time, 16);
        time++;

        LayoutTab tab1 = tabs[1].getLayoutTab();
        LayoutTab tab3 = tabs[3].getLayoutTab();

        float fingerOffset = Math.min(tab1.getClippedHeight() / 2, tab3.getClippedHeight() / 2);

        PointF finger0 = new PointF(tab1.getX() + tab1.getFinalContentWidth() / 2,
                tab1.getY() + fingerOffset);

        // Initiate finger 0
        eventDown(time, finger0);
        mManager.onUpdate(time, 16);
        time++;

        // Move finger 0: Y to simulate a scroll
        final float scrollOffset1 = (tab3.getY() - tab1.getY()) / 8.0f;
        finger0.y += scrollOffset1;

        eventMove(time, finger0);
        mManager.onUpdate(time, 16);
        time++;

        finger0.y -= scrollOffset1;

        eventMove(time, finger0);
        mManager.onUpdate(time, 16);
        time++;

        float expectedTab1X = tab1.getX();
        float expectedTab1Y = tab1.getY();

        // Initiate the pinch with finger 1
        PointF finger1 = new PointF(tab3.getX() + tab3.getFinalContentWidth() / 2,
                tab3.getY() + fingerOffset);
        eventDown1(time, finger1);
        mManager.onUpdate(time, 16);
        time++;

        final float delta = 0.001f;
        assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        float expectedTab3X = tab3.getX();
        float expectedTab3Y = tab3.getY();

        // Move Finger 0: Y only
        finger0.y += scrollOffset1;
        expectedTab1Y += scrollOffset1;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Move finger 0: Y and X
        finger0.y += scrollOffset1;
        finger0.x += tab1.getFinalContentWidth() / 8.0f;
        expectedTab1Y += scrollOffset1;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Move finger 1: Y and X
        final float scrollOffset3 = (tab3.getY() - layout.getHeight()) / 8.0f;
        finger1.y += scrollOffset3;
        finger1.x += tab3.getFinalContentWidth() / 8.0f;
        expectedTab3Y += scrollOffset3;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Move finger 0 and 1: Y and X
        finger0.y += scrollOffset1;
        finger0.x += tab1.getFinalContentWidth() / 8.0f;
        expectedTab1Y += scrollOffset1;
        finger1.y += scrollOffset3;
        finger1.x += tab3.getFinalContentWidth() / 8.0f;
        expectedTab3Y += scrollOffset3;

        eventMoveBoth(time, finger0, finger1);
        mManager.onUpdate(time, 16);
        time++;

        assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        // Done
        eventUp1(time, finger1);
        eventUp(time, finger0);

        assertEquals("Wrong x offset for tab 1", expectedTab1X, tab1.getX(), delta);
        assertEquals("Wrong y offset for tab 1", expectedTab1Y, tab1.getY(), delta);
        assertEquals("Wrong x offset for tab 3", expectedTab3X, tab3.getX(), delta);
        assertEquals("Wrong y offset for tab 3", expectedTab3Y, tab3.getY(), delta);

        mManagerPhone.hideOverview(false);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipeOnlyTab() throws Exception {
        initializeLayoutManagerPhone(1, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipeOnlyTabIncognito() throws Exception {
        initializeLayoutManagerPhone(0, 1, TabModel.INVALID_TAB_INDEX, 0, true);
        assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipeNextTab() throws Exception {
        initializeLayoutManagerPhone(2, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipePrevTab() throws Exception {
        initializeLayoutManagerPhone(2, 0, 1, TabModel.INVALID_TAB_INDEX, false);
        assertEquals(mTabModelSelector.getModel(false).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipeNextTabNone() throws Exception {
        initializeLayoutManagerPhone(2, 0, 1, TabModel.INVALID_TAB_INDEX, false);
        assertEquals(mTabModelSelector.getModel(false).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipePrevTabNone() throws Exception {
        initializeLayoutManagerPhone(2, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipeNextTabIncognito() throws Exception {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 0, true);
        assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipePrevTabIncognito() throws Exception {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 1, true);
        assertEquals(mTabModelSelector.getModel(true).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipeNextTabNoneIncognito() throws Exception {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 1, true);
        assertEquals(mTabModelSelector.getModel(true).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    public void testToolbarSideSwipePrevTabNoneIncognito() throws Exception {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 0, true);
        assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Load the browser process.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    BrowserStartupController.get(
                            getInstrumentation().getTargetContext(),
                            LibraryProcessType.PROCESS_BROWSER)
                                    .startBrowserProcessesSync(false);
                } catch (ProcessInitException e) {
                    fail("Failed to load browser");
                }
            }
        });
    }

    private void runToolbarSideSwipeTestOnCurrentModel(ScrollDirection direction, int finalIndex) {
        final TabModel model = mTabModelSelector.getCurrentModel();
        final int finalId = model.getTabAt(finalIndex).getId();

        performToolbarSideSwipe(direction);

        assertEquals("Unexpected model change after side swipe", model.isIncognito(),
                mTabModelSelector.isIncognitoSelected());

        assertEquals("Wrong index after side swipe", finalIndex, model.index());
        assertEquals("Wrong current tab id", finalId, TabModelUtils.getCurrentTab(model).getId());
        assertTrue("LayoutManager#getActiveLayout() should be StaticLayout",
                mManager.getActiveLayout() instanceof StaticLayout);
    }

    private void performToolbarSideSwipe(ScrollDirection direction) {
        assertTrue("Unexpected direction for side swipe " + direction,
                direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT);

        final Layout layout = mManager.getActiveLayout();
        final EdgeSwipeHandler eventHandler = mManager.getTopSwipeHandler();

        assertNotNull("LayoutManager#getTopSwipeHandler() returned null", eventHandler);
        assertNotNull("LayoutManager#getActiveLayout() returned null", layout);

        final float layoutWidth = layout.getWidth();
        final boolean scrollLeft = direction == ScrollDirection.LEFT;
        final float deltaX = MathUtils.flipSignIf(layoutWidth / 2.f, scrollLeft);

        eventHandler.swipeStarted(direction, layoutWidth, 0);
        eventHandler.swipeUpdated(deltaX, 0.f, deltaX, 0.f, deltaX, 0.f);
        eventHandler.swipeFinished();

        assertTrue("LayoutManager#getActiveLayout() should be ToolbarSwipeLayout",
                mManager.getActiveLayout() instanceof ToolbarSwipeLayout);
        assertTrue("LayoutManager took too long to finish the animations",
                simulateTime(mManager, 1000));
    }

    @Override
    public Tab createTab(int id, boolean incognito) {
        return new Tab(id, incognito, null);
    }
}
