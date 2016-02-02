// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.graphics.Rect;
import android.graphics.Region;
import android.os.Build;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.PrerenderTestHelper;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.UiUtils;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test suite for verifying the behavior of various fullscreen actions.
 */
public class FullscreenManagerTest extends ChromeTabbedActivityTestBase {

    private static final String LONG_HTML_WITH_AUTO_FOCUS_INPUT_TEST_PAGE =
            UrlUtils.encodeHtmlDataUri("<html>"
                    + "<body style='height:10000px;'>"
                    + "<p>The text input is focused automatically on load."
                    + " The top controls should not hide when page is scrolled.</p><br/>"
                    + "<input id=\"input_text\" type=\"text\" autofocus/>"
                    + "</body>"
                    + "</html>");

    private static final String LONG_HTML_TEST_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html><body style='height:10000px;'></body></html>");
    private static final String LONG_FULLSCREEN_API_HTML_TEST_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "<head>"
            + "  <meta name=\"viewport\" "
            + "    content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" />"
            + "  <script>"
            + "    function toggleFullScreen() {"
            + "      if (document.webkitIsFullScreen) {"
            + "        document.webkitCancelFullScreen();"
            + "      } else {"
            + "        document.body.webkitRequestFullScreen();"
            + "      }"
            + "    };"
            + "  </script>"
            + "  <style>"
            + "    body:-webkit-full-screen { background: red; width: 100%; }"
            + "  </style>"
            + "</head>"
            + "<body style='height:10000px;' onclick='toggleFullScreen();'>"
            + "</body>"
            + "</html>");

    @MediumTest
    @Feature({"Fullscreen"})
    public void testTogglePersistentFullscreen() throws InterruptedException {
        startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        Tab tab = getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = tab.getTabWebContentsDelegateAndroid();

        FullscreenTestUtils.waitForFullscreenFlag(tab, false, getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(tab, true, getActivity());

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(tab, false, getActivity());
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testPersistentFullscreenChangingUiFlags() throws InterruptedException {
        // Exiting fullscreen via UI Flags is not supported in versions prior to MR2.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;

        startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        final Tab tab = getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = tab.getTabWebContentsDelegateAndroid();

        FullscreenTestUtils.waitForFullscreenFlag(tab, false, getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(tab, true, getActivity());

        // There is a race condition in android when setting various system UI flags.
        // Adding this wait to allow the animation transitions to complete before continuing
        // the test (See https://b/10387660)
        UiUtils.settleDownUI(getInstrumentation());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                View view = tab.getContentViewCore().getContainerView();
                view.setSystemUiVisibility(
                        view.getSystemUiVisibility() & ~View.SYSTEM_UI_FLAG_FULLSCREEN);
            }
        });
        FullscreenTestUtils.waitForFullscreenFlag(tab, true, getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, true);
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testExitPersistentFullscreenAllowsManualFullscreen() throws InterruptedException {
        startMainActivityWithURL(LONG_FULLSCREEN_API_HTML_TEST_PAGE);
        if (DeviceClassManager.isAutoHidingToolbarDisabled(getActivity())) return;

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.setAnimationDurationsForTest(1, 1);
        int topControlsHeight = fullscreenManager.getTopControlsHeight();

        Tab tab = getActivity().getActivityTab();
        View view = tab.getView();
        final TabWebContentsDelegateAndroid delegate =
                tab.getTabWebContentsDelegateAndroid();

        singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, true);

        waitForTopControlsPosition(-topControlsHeight);

        TestTouchUtils.sleepForDoubleTapTimeout(getInstrumentation());
        singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);
        waitForNoBrowserTopControlsOffset();
        waitForTopControlsPosition(0);

        scrollTopControls(false);
        scrollTopControls(true);
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testManualHidingShowingTopControls() throws InterruptedException {
        startMainActivityWithURL(LONG_HTML_TEST_PAGE);
        if (DeviceClassManager.isAutoHidingToolbarDisabled(getActivity())) return;

        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        disableBrowserOverrides();

        assertEquals(fullscreenManager.getControlOffset(), 0f);

        waitForTopControlsToBeMoveable(getActivity().getActivityTab());

        // Check that the URL bar has not grabbed focus (http://crbug/236365)
        UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertFalse("Url bar grabbed focus", urlBar.hasFocus());
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testHidingTopControlsRemovesSurfaceFlingerOverlay() throws InterruptedException {
        startMainActivityWithURL(LONG_HTML_TEST_PAGE);
        if (DeviceClassManager.isAutoHidingToolbarDisabled(getActivity())) return;

        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        disableBrowserOverrides();

        assertEquals(fullscreenManager.getControlOffset(), 0f);

        // Detect layouts. Note this doesn't actually need to be atomic (just final).
        final AtomicInteger layoutCount = new AtomicInteger();
        getActivity().getWindow().getDecorView().getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        layoutCount.incrementAndGet();
                    }
                });

        // When the top-controls are removed, we need a layout to trigger the
        // transparent region for the app to be updated.
        scrollTopControls(false);
        CriteriaHelper.pollForUIThreadCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return layoutCount.get() > 0;
                    }
                });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Check that when the top controls are gone, the entire decorView is contained
                // in the transparent region of the app.
                Rect visibleDisplayFrame = new Rect();
                Region transparentRegion = new Region();
                ViewGroup decorView = (ViewGroup)  getActivity().getWindow().getDecorView();
                decorView.getWindowVisibleDisplayFrame(visibleDisplayFrame);
                decorView.gatherTransparentRegion(transparentRegion);
                assertTrue(transparentRegion.quickContains(visibleDisplayFrame));
            }
        });

        // Additional manual test that this is working:
        // - adb shell dumpsys SurfaceFlinger
        // - Observe that there is no 'Chrome' related overlay listed, only 'Surfaceview'.
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testManualFullscreenDisabledForChromePages() throws InterruptedException {
        // The credits page was chosen as it is a chrome:// page that is long and would support
        // manual fullscreen if it were supported.
        startMainActivityWithURL("chrome://credits");

        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        disableBrowserOverrides();
        int topControlsHeight = fullscreenManager.getTopControlsHeight();

        assertEquals(fullscreenManager.getControlOffset(), 0f);

        float dragX = 50f;
        float dragStartY = topControlsHeight * 2;
        float dragFullY = dragStartY - topControlsHeight;

        long downTime = SystemClock.uptimeMillis();
        dragStart(dragX, dragStartY, downTime);
        dragTo(dragX, dragX, dragStartY, dragFullY, 100, downTime);
        waitForTopControlsPosition(0f);
        dragEnd(dragX, dragFullY, downTime);
        waitForTopControlsPosition(0f);
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testControlsShownOnUnresponsiveRenderer() throws InterruptedException {
        startMainActivityWithURL(LONG_HTML_TEST_PAGE);
        if (DeviceClassManager.isAutoHidingToolbarDisabled(getActivity())) return;

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.setAnimationDurationsForTest(1, 1);
        waitForNoBrowserTopControlsOffset();
        assertEquals(fullscreenManager.getControlOffset(), 0f);

        scrollTopControls(false);

        Tab tab = getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate =
                tab.getTabWebContentsDelegateAndroid();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                delegate.rendererUnresponsive();
            }
        });
        waitForTopControlsPosition(0f);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                delegate.rendererResponsive();
            }
        });
        waitForNoBrowserTopControlsOffset();
    }

    @LargeTest
    @Feature({"Fullscreen"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrerenderedPageSupportsManualHiding() throws InterruptedException {
        startMainActivityOnBlankPage();
        if (DeviceClassManager.isAutoHidingToolbarDisabled(getActivity())) return;
        disableBrowserOverrides();

        final Tab tab = getActivity().getActivityTab();
        final String testUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/very_long_google.html");
        PrerenderTestHelper.prerenderUrlAndFocusOmnibox(testUrl, this);
        assertTrue("loadUrl did not use pre-rendered page.",
                PrerenderTestHelper.isLoadUrlResultPrerendered(loadUrl(testUrl)));

        UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, false);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.processEnableFullscreenRunnableForTest();
            }
        });

        waitForTopControlsToBeMoveable(tab);
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testTopControlsShownWhenInputIsFocused()
            throws InterruptedException, TimeoutException {
        startMainActivityWithURL(LONG_HTML_WITH_AUTO_FOCUS_INPUT_TEST_PAGE);
        if (DeviceClassManager.isAutoHidingToolbarDisabled(getActivity())) return;

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        assertEquals(fullscreenManager.getControlOffset(), 0f);

        fullscreenManager.setAnimationDurationsForTest(1, 1);

        int topControlsHeight = fullscreenManager.getTopControlsHeight();
        float dragX = 50f;
        float dragStartY = topControlsHeight * 3;
        float dragEndY = dragStartY - topControlsHeight * 2;
        long downTime = SystemClock.uptimeMillis();
        dragStart(dragX, dragStartY, downTime);
        dragTo(dragX, dragX, dragStartY, dragEndY, 100, downTime);
        dragEnd(dragX, dragEndY, downTime);
        waitForNoBrowserTopControlsOffset();
        assertEquals(fullscreenManager.getControlOffset(), 0f);

        Tab tab = getActivity().getActivityTab();
        singleClickView(tab.getView());
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(),
                "document.getElementById('input_text').blur();");
        waitForEditableNodeToLoseFocus(tab);

        waitForTopControlsToBeMoveable(getActivity().getActivityTab());
    }

    private void scrollTopControls(boolean show) throws InterruptedException {
        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        int topControlsHeight = fullscreenManager.getTopControlsHeight();

        waitForPageToBeScrollable(getActivity().getActivityTab());

        float dragX = 50f;
        // Use a larger scroll range than the height of the top controls to ensure we overcome
        // the delay in a scroll start being sent.
        float dragStartY = topControlsHeight * 3;
        float dragEndY = dragStartY - topControlsHeight * 2;
        float expectedPosition = -topControlsHeight;
        if (show) {
            expectedPosition = 0f;
            float tempDragStartY = dragStartY;
            dragStartY = dragEndY;
            dragEndY = tempDragStartY;
        }
        long downTime = SystemClock.uptimeMillis();
        dragStart(dragX, dragStartY, downTime);
        dragTo(dragX, dragX, dragStartY, dragEndY, 100, downTime);
        dragEnd(dragX, dragEndY, downTime);
        waitForTopControlsPosition(expectedPosition);
    }

    private void waitForTopControlsPosition(final float position)
            throws InterruptedException {
        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason("Top controls did not reach expected position.  Expected: "
                        + position + ", Actual: " + fullscreenManager.getControlOffset());
                return position == fullscreenManager.getControlOffset();
            }
        });
    }

    private void waitForNoBrowserTopControlsOffset() throws InterruptedException {
        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !fullscreenManager.hasBrowserControlOffsetOverride();
            }
        });
    }

    private void waitForPageToBeScrollable(final Tab tab) throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ContentViewCore contentViewCore = tab.getContentViewCore();
                return contentViewCore.computeVerticalScrollRange()
                        > contentViewCore.getContainerView().getHeight();
            }
        });
    }

    /**
     * Waits for the top controls to be moveable by user gesture.
     * <p>
     * This function requires the top controls to start fully visible.  It till then ensure that at
     * some point the controls can be moved by user gesture.  It will then fully cycle the top
     * controls to entirely hidden and back to fully shown.
     */
    private void waitForTopControlsToBeMoveable(final Tab tab) throws InterruptedException {
        waitForTopControlsPosition(0f);

        final CallbackHelper contentMovedCallback = new CallbackHelper();
        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        final float initialVisibleContentOffset = fullscreenManager.getVisibleContentOffset();

        fullscreenManager.addListener(new FullscreenListener() {
            @Override
            public void onVisibleContentOffsetChanged(float offset) {
                if (offset != initialVisibleContentOffset) {
                    contentMovedCallback.notifyCalled();
                    fullscreenManager.removeListener(this);
                }
            }

            @Override
            public void onToggleOverlayVideoMode(boolean enabled) {
            }

            @Override
            public void onContentOffsetChanged(float offset) {
            }
        });

        float dragX = 50f;
        float dragStartY = tab.getView().getHeight() - 50f;

        for (int i = 0; i < 10; i++) {
            float dragEndY = dragStartY - fullscreenManager.getTopControlsHeight();

            long downTime = SystemClock.uptimeMillis();
            dragStart(dragX, dragStartY, downTime);
            dragTo(dragX, dragX, dragStartY, dragEndY, 100, downTime);
            dragEnd(dragX, dragEndY, downTime);

            try {
                contentMovedCallback.waitForCallback(0, 1, 500, TimeUnit.MILLISECONDS);

                scrollTopControls(false);
                scrollTopControls(true);

                return;
            } catch (TimeoutException e) {
                // Ignore and retry
            }
        }

        fail("Visible content never moved as expected.");
    }

    private void waitForEditableNodeToLoseFocus(final Tab tab) throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ContentViewCore contentViewCore = tab.getContentViewCore();
                return !contentViewCore.isFocusedNodeEditable();
            }
        });
    }

    private void disableBrowserOverrides() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getFullscreenManager().disableBrowserOverrideForTest();
            }
        });
    }

    @Override
    protected void startMainActivityWithURL(String url) throws InterruptedException {
        super.startMainActivityWithURL(url);
        final Tab tab = getActivity().getActivityTab();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tab.processEnableFullscreenRunnableForTest();
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Each test will start itself with the appropriate test page.
    }
}
