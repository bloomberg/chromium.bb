// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.widget.LinearLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManagerWrapper;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilterHost;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContextualSearchClient;
import org.chromium.content.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.touch_selection.SelectionEventType;

import javax.annotation.Nullable;

/**
 * Mock touch events with Contextual Search to test behavior of its panel and manager.
 */
public class ContextualSearchTapEventTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private ContextualSearchManagerWrapper mContextualSearchManager;
    private ContextualSearchPanel mPanel;
    private OverlayPanelManagerWrapper mPanelManager;
    private ContextualSearchClient mContextualSearchClient;

    /**
     * A ContextualSearchRequest that forgoes URI template lookup.
     */
    private static class MockContextualSearchRequest extends ContextualSearchRequest {
        public MockContextualSearchRequest(String term, String altTerm, boolean prefetch) {
            super(term, altTerm, "", prefetch);
        }

        @Override
        protected Uri getUriTemplate(
                String query, @Nullable String alternateTerm, String mid, boolean shouldPrefetch) {
            return Uri.parse("");
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * ContextualSearchPanel wrapper that prevents native calls.
     */
    private static class ContextualSearchPanelWrapper extends ContextualSearchPanel {
        public ContextualSearchPanelWrapper(Context context, LayoutUpdateHost updateHost,
                EventFilterHost eventHost, OverlayPanelManager panelManager) {
            super(context, updateHost, eventHost, panelManager);
        }

        @Override
        public void peekPanel(StateChangeReason reason) {
            setHeightForTesting(1);
            super.peekPanel(reason);
        }

        @Override
        public void setBasePageTextControlsVisibility(boolean visible) {}
    }

    // --------------------------------------------------------------------------------------------

    /**
     * ContextualSearchManager wrapper that prevents network requests and most native calls.
     */
    private static class ContextualSearchManagerWrapper extends ContextualSearchManager {
        public ContextualSearchManagerWrapper(ChromeActivity activity,
                WindowAndroid windowAndroid) {
            super(activity, windowAndroid, null);
            setSelectionController(new MockCSSelectionController(activity, this));
            ContentViewCore contentView = getSelectionController().getBaseContentView();
            contentView.setSelectionPopupControllerForTesting(
                    new SelectionPopupController(activity, null, null, null,
                            contentView.getRenderCoordinates(), null));
            contentView.setContextualSearchClient(this);
            MockContextualSearchPolicy policy = new MockContextualSearchPolicy();
            setContextualSearchPolicy(policy);
            mTranslateController = new MockedCSTranslateController(activity, policy, null);
        }

        @Override
        public void startSearchTermResolutionRequest(String selection) {
            // Skip native calls and immediately "resolve" the search term.
            onSearchTermResolutionResponse(
                    true, 200, selection, selection, "", "", false, 0, 10, "", "", "", "",
                    QuickActionCategory.NONE);
        }

        @Override
        protected ContextualSearchRequest createContextualSearchRequest(
                String query, String altTerm, String mid, boolean shouldPrefetch) {
            return new MockContextualSearchRequest(query, altTerm, shouldPrefetch);
        }

        @Override
        protected void nativeGatherSurroundingText(long nativeContextualSearchManager,
                String selection, String homeCountry, WebContents webContents,
                boolean maySendBasePageUrl) {}

        /**
         * @return A stubbed ContentViewCore for mocking text selection.
         */
        public StubbedContentViewCore getBaseContentView() {
            return (StubbedContentViewCore) getSelectionController().getBaseContentView();
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * Selection controller that mocks out anything to do with a ContentViewCore.
     */
    private static class MockCSSelectionController extends ContextualSearchSelectionController {
        private StubbedContentViewCore mContentViewCore;

        public MockCSSelectionController(ChromeActivity activity,
                ContextualSearchSelectionHandler handler) {
            super(activity, handler);
            mContentViewCore = new StubbedContentViewCore(activity);
        }

        @Override
        public StubbedContentViewCore getBaseContentView() {
            return mContentViewCore;
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * Translate controller that mocks out native calls.
     */
    private static class MockedCSTranslateController extends ContextualSearchTranslateController {
        private static final String ENGLISH_TARGET_LANGUAGE = "en";
        private static final String ENGLISH_ACCEPT_LANGUAGES = "en-US,en";

        MockedCSTranslateController(ChromeActivity activity, ContextualSearchPolicy policy,
                ContextualSearchTranslateInterface hostInterface) {
            super(activity, policy, hostInterface);
        }

        @Override
        protected String getNativeAcceptLanguages() {
            return ENGLISH_ACCEPT_LANGUAGES;
        }

        @Override
        protected String getNativeTranslateServiceTargetLanguage() {
            return ENGLISH_TARGET_LANGUAGE;
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * A ContentViewCore that has some methods stubbed out for testing.
     */
    private static final class StubbedContentViewCore extends ContentViewCore {
        private String mCurrentText;

        public StubbedContentViewCore(Context context) {
            super(context, "");
        }

        @Override
        public String getSelectedText() {
            return mCurrentText;
        }

        public void setSelectedText(String string) {
            mCurrentText = string;
        }
    }

    // --------------------------------------------------------------------------------------------

    /**
     * Trigger text selection on the contextual search manager.
     */
    private void mockTapText(String text) {
        mContextualSearchManager.getBaseContentView().setSelectedText(text);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContextualSearchClient.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_SHOWN,
                        0, 0);
            }
        });
    }

    /**
     * Trigger empty space tap.
     */
    private void mockTapEmptySpace() {
        mContextualSearchClient.showUnhandledTapUIIfNeeded(0, 0);
        mContextualSearchClient.onSelectionEvent(
                SelectionEventType.SELECTION_HANDLES_CLEARED, 0, 0);
    }

    // --------------------------------------------------------------------------------------------

    public ContextualSearchTapEventTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mPanelManager = new OverlayPanelManagerWrapper();
        mPanelManager.setContainerView(new LinearLayout(getActivity()));
        mPanelManager.setDynamicResourceLoader(new DynamicResourceLoader(0, null));

        mContextualSearchManager =
                new ContextualSearchManagerWrapper(getActivity(), getActivity().getWindowAndroid());
        mPanel = new ContextualSearchPanelWrapper(getActivity(), null, null, mPanelManager);
        mPanel.setManagementDelegate(mContextualSearchManager);
        mContextualSearchManager.setContextualSearchPanel(mPanel);

        mContextualSearchClient = mContextualSearchManager;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL("about:blank");
    }

    /**
     * Tests that a Tap gesture followed by tapping empty space closes the panel.
     */
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testTextTapFollowedByNonTextTap() {
        assertTrue(mPanelManager.getRequestPanelShowCount() == 0);

        // Fake a selection event.
        mockTapText("text");

        assertTrue(mPanelManager.getRequestPanelShowCount() == 1);
        assertTrue(mPanelManager.getPanelHideCount() == 0);
        assertTrue(mContextualSearchManager.getSelectionController().getSelectedText()
                .equals("text"));

        // Fake tap on non-text.
        mockTapEmptySpace();

        assertTrue(mPanelManager.getRequestPanelShowCount() == 1);
        assertTrue(mPanelManager.getPanelHideCount() == 1);
        assertTrue(mContextualSearchManager.getSelectionController().getSelectedText() == null);
    }
}
