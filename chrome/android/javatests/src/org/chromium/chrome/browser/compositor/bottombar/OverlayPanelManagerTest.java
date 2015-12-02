// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

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
        private OverlayPanelHost mHost;
        private ViewGroup mContainerView;
        private DynamicResourceLoader mResourceLoader;

        public MockOverlayPanel(Context context, LayoutUpdateHost updateHost,
                OverlayPanelManager panelManager, PanelPriority priority,
                boolean canBeSuppressed) {
            super(context, updateHost, panelManager);
            mPriority = priority;
            mCanBeSuppressed = canBeSuppressed;
        }

        @Override
        public void setHost(OverlayPanelHost host) {
            super.setHost(host);
            mHost = host;
        }

        public OverlayPanelHost getHost() {
            return mHost;
        }

        @Override
        public void setContainerView(ViewGroup container) {
            super.setContainerView(container);
            mContainerView = container;
        }

        public ViewGroup getContainerView() {
            return mContainerView;
        }

        @Override
        public void setDynamicResourceLoader(DynamicResourceLoader loader) {
            super.setDynamicResourceLoader(loader);
            mResourceLoader = loader;
        }

        public DynamicResourceLoader getDynamicResourceLoader() {
            return mResourceLoader;
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

    @SmallTest
    @Feature({"OverlayPanel"})
    public void testLatePanelGetsNecessaryVars() {
        Context context = getInstrumentation().getTargetContext();

        OverlayPanelManager panelManager = new OverlayPanelManager();
        MockOverlayPanel earlyPanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, true);

        OverlayPanelHost host = new OverlayPanelHost() {
                    @Override
                    public void hideLayout(boolean immediately) {
                        // Intentionally do nothing.
                    }
                };

        // Set necessary vars before any other panels are registered in the manager.
        panelManager.setPanelHost(host);
        panelManager.setContainerView(new LinearLayout(getInstrumentation().getTargetContext()));
        panelManager.setDynamicResourceLoader(new DynamicResourceLoader(0, null));

        MockOverlayPanel latePanel =
                new MockOverlayPanel(context, null, panelManager, PanelPriority.MEDIUM, true);

        assertTrue(earlyPanel.getHost() == latePanel.getHost());
        assertTrue(earlyPanel.getContainerView() == latePanel.getContainerView());
        assertTrue(earlyPanel.getDynamicResourceLoader() == latePanel.getDynamicResourceLoader());
    }
}
