// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;

/**
 * Class responsible for testing the OverlayPanelManager.
 */
public class OverlayPanelManagerTest extends InstrumentationTestCase {

    // --------------------------------------------------------------------------------------------
    // MockOverlayPanel
    // --------------------------------------------------------------------------------------------

    /**
     * Mocks the ContextualSearchPanel, so it doesn't create ContentViewCore.
     */
    public static class MockOverlayPanel extends OverlayPanel {

        private PanelPriority mPriority;
        private boolean mCanBeSuppressed;

        public MockOverlayPanel(Context context, LayoutUpdateHost updateHost,
                OverlayPanelManager panelManager, PanelPriority priority,
                boolean canBeSuppressed) {
            super(context, updateHost, panelManager);
            mPriority = priority;
            mCanBeSuppressed = canBeSuppressed;
        }

        @Override
        public OverlayPanelContent createNewOverlayPanelContent() {
            return new MockOverlayPanelContent();
        }

        @Override
        public PanelPriority getPriority() {
            return mPriority;
        }

        @Override
        public boolean canBeSuppressed() {
            return mCanBeSuppressed;
        }

        @Override
        public void closePanel(StateChangeReason reason, boolean animate) {
            // Immediately call onClosed rather than wait for animation to finish.
            onClosed(reason);
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
    // Test Suite
    // --------------------------------------------------------------------------------------------

    @SmallTest
    @Feature({"OverlayPanel"})
    public void testPanelRequestingShow() {
        Context context = getInstrumentation().getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel panel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, false);

        panel.requestPanelShow(StateChangeReason.UNKNOWN);

        assertTrue(panelManager.getActivePanel() == panel);
    }

    @SmallTest
    @Feature({"OverlayPanel"})
    public void testPanelClosed() {
        Context context = getInstrumentation().getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel panel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, false);

        panel.requestPanelShow(StateChangeReason.UNKNOWN);
        panel.closePanel(StateChangeReason.UNKNOWN, false);

        assertTrue(panelManager.getActivePanel() == null);
    }

    @SmallTest
    @Feature({"OverlayPanel"})
    public void testHighPrioritySuppressingLowPriority() {
        Context context = getInstrumentation().getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, false);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);

        assertTrue(panelManager.getActivePanel() == highPriorityPanel);
    }

    @SmallTest
    @Feature({"OverlayPanel"})
    public void testSuppressedPanelRestored() {
        Context context = getInstrumentation().getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, true);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        assertTrue(panelManager.getActivePanel() == lowPriorityPanel);
    }

    @SmallTest
    @Feature({"OverlayPanel"})
    public void testUnsuppressiblePanelNotRestored() {
        Context context = getInstrumentation().getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, false);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        assertTrue(panelManager.getActivePanel() == null);
    }

    @SmallTest
    @Feature({"OverlayPanel"})
    public void testSuppressedPanelClosedBeforeRestore() {
        Context context = getInstrumentation().getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        OverlayPanel lowPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.LOW, true);
        OverlayPanel highPriorityPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.HIGH, false);

        lowPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        highPriorityPanel.requestPanelShow(StateChangeReason.UNKNOWN);
        lowPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);
        highPriorityPanel.closePanel(StateChangeReason.UNKNOWN, false);

        assertTrue(panelManager.getActivePanel() == null);
    }
}
