// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManagerWrapper;
import org.chromium.chrome.browser.compositor.bottombar.readermode.ReaderModeBarControl;
import org.chromium.chrome.browser.compositor.bottombar.readermode.ReaderModePanel;
import org.chromium.chrome.browser.compositor.scene_layer.ReaderModeSceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel.MockTabModelDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Tests logic in the ReaderModeManager.
 */
public class ReaderModeManagerTest extends InstrumentationTestCase {

    OverlayPanelManagerWrapper mPanelManager;
    ReaderModeManagerWrapper mReaderManager;
    MockReaderModePanel mPanel;
    ReaderModeMockTabModelSelector mTabModelSelector;

    /**
     * A mock TabModelSelector for creating tabs.
     */
    private static class ReaderModeMockTabModelSelector extends MockTabModelSelector {
        public ReaderModeMockTabModelSelector() {
            super(2, 0,
                    new MockTabModelDelegate() {
                        @Override
                        public Tab createTab(int id, boolean incognito) {
                            return new Tab(id, incognito, null);
                        }
                    });
        }

        @Override
        public int getCurrentTabId() {
            return 0;
        }
    }

    /**
     * A wrapper for the ReaderModeManager; this is used for recording and triggering events
     * manually.
     */
    private static class ReaderModeManagerWrapper extends ReaderModeManager {
        private int mRecordedCount;
        private int mVisibleCount;
        private boolean mShouldTrigger;

        public ReaderModeManagerWrapper(MockTabModelSelector selector) {
            super(selector, null);
            mShouldTrigger = true;
        }

        @Override
        protected void requestReaderPanelShow(StateChangeReason reason) {
            // Skip tab checks and request the panel be shown.
            if (mShouldTrigger) mReaderModePanel.requestPanelShow(reason);
        }

        @Override
        public WebContentsObserver createWebContentsObserver(WebContents webContents) {
            // Do not attempt to create or attach a WebContentsObserver.
            return null;
        }

        @Override
        protected boolean isDistillerHeuristicAlwaysTrue() {
            return true;
        }

        @Override
        protected void recordPanelVisibilityForNavigation(boolean visible) {
            mRecordedCount++;
            mVisibleCount += visible ? 1 : 0;
        }

        public void setShouldTrigger(boolean shouldTrigger) {
            mShouldTrigger = shouldTrigger;
        }

        public int getRecordedCount() {
            return mRecordedCount;
        }

        public int getVisibleCount() {
            return mVisibleCount;
        }

        public ReaderModeTabInfo getTabInfo(int id) {
            return mTabStatusMap.get(id);
        }
    }

    /**
     * Mock ReaderModePanel.
     */
    private static class MockReaderModePanel extends ReaderModePanel {
        public MockReaderModePanel(Context context, OverlayPanelManager manager) {
            super(context, null, null, manager, null);
        }

        @Override
        public ReaderModeSceneLayer createNewReaderModeSceneLayer() {
            return null;
        }

        @Override
        public void peekPanel(StateChangeReason reason) {
            setHeightForTesting(1);
            super.peekPanel(reason);
        }

        @Override
        protected ReaderModeBarControl getReaderModeBarControl() {
            return new MockReaderModeBarControl();
        }

        /**
         * This class is overridden to be completely inert; it would otherwise call many native
         * methods.
         */
        private static class MockReaderModeBarControl extends ReaderModeBarControl {
            public MockReaderModeBarControl() {
                super(null, null, null, null);
            }

            @Override
            public void setBarText(int stringId) {}

            @Override
            public void inflate() {}

            @Override
            public void invalidate() {}
        }

        @Override
        protected void initializeUiState() {}

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
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mPanelManager = new OverlayPanelManagerWrapper();
        mTabModelSelector = new ReaderModeMockTabModelSelector();
        mReaderManager = new ReaderModeManagerWrapper(mTabModelSelector);
        mPanel = new MockReaderModePanel(getInstrumentation().getTargetContext(), mPanelManager);
        mReaderManager.setReaderModePanel(mPanel);
        mPanel.setManagerDelegate(mReaderManager);
    }

    // Start ReaderModeManager test suite.

    /**
     * Tests that the panel behaves appropriately with infobar events.
     */
    @SmallTest
    @Feature({"ReaderModeManager"})
    @RetryOnFailure
    public void testInfoBarEvents() {
        mPanel.requestPanelShow(StateChangeReason.UNKNOWN);

        mReaderManager.onAddInfoBar(null, null, true);
        assertEquals(1, mPanelManager.getRequestPanelShowCount());
        assertEquals(1, mPanelManager.getPanelHideCount());

        mReaderManager.onRemoveInfoBar(null, null, true);
        assertEquals(2, mPanelManager.getRequestPanelShowCount());
        assertEquals(1, mPanelManager.getPanelHideCount());
    }

    /**
     * Tests that the panel behaves appropriately with fullscreen events.
     */
    @SmallTest
    @Feature({"ReaderModeManager"})
    @RetryOnFailure
    public void testFullscreenEvents() {
        mPanel.requestPanelShow(StateChangeReason.UNKNOWN);

        mReaderManager.onToggleFullscreenMode(null, true);
        assertEquals(1, mPanelManager.getRequestPanelShowCount());
        assertEquals(1, mPanelManager.getPanelHideCount());

        mReaderManager.onToggleFullscreenMode(null, false);
        assertEquals(2, mPanelManager.getRequestPanelShowCount());
        assertEquals(1, mPanelManager.getPanelHideCount());
    }

    /**
     * Tests that the metric that tracks when the panel is visible is correctly recorded.
     */
    @SmallTest
    @Feature({"ReaderModeManager"})
    @RetryOnFailure
    public void testPanelOpenRecorded() {
        Tab tab = new Tab(0, false, null);
        mReaderManager.onShown(tab);

        assertEquals(1, mReaderManager.getRecordedCount());
        assertEquals(1, mReaderManager.getVisibleCount());
        assertTrue(null != mReaderManager.getTabInfo(0));

        // Make sure recording the panel showing only occurs once per navigation.
        mReaderManager.onShown(tab);

        assertEquals(1, mReaderManager.getRecordedCount());
        assertEquals(1, mReaderManager.getVisibleCount());

        // Destroy shouldn't record either if the panel showed.
        mReaderManager.onDestroyed(tab);

        assertEquals(1, mReaderManager.getRecordedCount());
        assertEquals(1, mReaderManager.getVisibleCount());
        assertTrue(null == mReaderManager.getTabInfo(0));
    }

    /**
     * Tests that a tab closing records the panel was not visible.
     */
    @SmallTest
    @Feature({"ReaderModeManager"})
    @RetryOnFailure
    public void testPanelCloseRecorded() {
        Tab tab = new Tab(0, false, null);
        mReaderManager.setShouldTrigger(false);
        mReaderManager.onShown(tab);
        mReaderManager.onDestroyed(tab);

        assertEquals(1, mReaderManager.getRecordedCount());
        assertEquals(0, mReaderManager.getVisibleCount());
        assertTrue(null == mReaderManager.getTabInfo(0));
    }

    // TODO(mdjones): Test add/remove infobar while fullscreen is enabled.
    // TODO(mdjones): Test onclosebuttonpressed disables Reader Mode for a particular tab.
    // TODO(mdjones): Test onorientationchanged closes and re-opens panel.
    // TODO(mdjones): Test tab events.
    // TODO(mdjones): Test distillability callback.
}
