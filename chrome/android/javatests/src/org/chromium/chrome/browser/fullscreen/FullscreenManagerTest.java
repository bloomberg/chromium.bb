// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.graphics.Rect;
import android.graphics.Region;
import android.os.Build;
import android.os.SystemClock;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.PrerenderTestHelper;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.UiUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
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

        assertTrue(waitForFullscreenFlag(tab, false));
        assertTrue(waitForPersistentFullscreen(delegate, false));

        togglePersistentFullscreen(delegate, true);
        assertTrue(waitForFullscreenFlag(tab, true));
        assertTrue(waitForPersistentFullscreen(delegate, true));

        togglePersistentFullscreen(delegate, false);
        assertTrue(waitForFullscreenFlag(tab, false));
        assertTrue(waitForPersistentFullscreen(delegate, false));
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testPersistentFullscreenChangingUiFlags() throws InterruptedException {
        // Exiting fullscreen via UI Flags is not supported in versions prior to MR2.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;

        startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        final Tab tab = getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = tab.getTabWebContentsDelegateAndroid();

        assertTrue(waitForFullscreenFlag(tab, false));
        assertTrue(waitForPersistentFullscreen(delegate, false));

        togglePersistentFullscreen(delegate, true);
        assertTrue(waitForFullscreenFlag(tab, true));
        assertTrue(waitForPersistentFullscreen(delegate, true));

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
        assertTrue(waitForFullscreenFlag(tab, true));
        assertTrue(waitForPersistentFullscreen(delegate, true));
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testExitPersistentFullscreenAllowsManualFullscreen()
            throws InterruptedException, ExecutionException {
        startMainActivityWithURL(LONG_FULLSCREEN_API_HTML_TEST_PAGE);

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.setAnimationDurationsForTest(1, 1);
        int topControlsHeight = fullscreenManager.getTopControlsHeight();

        Tab tab = getActivity().getActivityTab();
        View view = tab.getView();
        final TabWebContentsDelegateAndroid delegate =
                tab.getTabWebContentsDelegateAndroid();

        singleClickView(view);
        waitForPersistentFullscreen(delegate, true);
        assertEquals((float) -topControlsHeight, waitForTopControlsPosition(-topControlsHeight));

        TestTouchUtils.sleepForDoubleTapTimeout(getInstrumentation());
        singleClickView(view);
        waitForPersistentFullscreen(delegate, false);
        waitForNoBrowserTopControlsOffset();
        assertEquals((float) 0, waitForTopControlsPosition(0));

        scrollTopControls(false);
        scrollTopControls(true);
    }

    /**
     * Marked flaky on 2015-05-15: http://crbug.com/488393
     * @LargeTest
     * @Feature({"Fullscreen"})
     */
    @FlakyTest
    public void testManualHidingShowingTopControls()
            throws InterruptedException, ExecutionException {
        startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.disableBrowserOverrideForTest();

        assertEquals(fullscreenManager.getControlOffset(), 0f);

        scrollTopControls(false);
        // Reverse the scroll and ensure the controls come back into view.
        scrollTopControls(true);
        // Check that the URL bar has not grabbed focus (http://crbug/236365)
        UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertFalse("Url bar grabbed focus", urlBar.hasFocus());
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testHidingTopControlsRemovesSurfaceFlingerOverlay()
            throws InterruptedException, ExecutionException {
        startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.disableBrowserOverrideForTest();

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
        boolean layoutOccured = CriteriaHelper.pollForUIThreadCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return layoutCount.get() > 0;
                    }
                });
        assertTrue(layoutOccured);

        getInstrumentation().runOnMainSync(new Runnable() {
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
    public void testManualFullscreenDisabledForChromePages()
            throws InterruptedException, ExecutionException {
        // The credits page was chosen as it is a chrome:// page that is long and would support
        // manual fullscreen if it were supported.
        startMainActivityWithURL("chrome://credits");

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.disableBrowserOverrideForTest();
        int topControlsHeight = fullscreenManager.getTopControlsHeight();

        assertEquals(fullscreenManager.getControlOffset(), 0f);

        float dragX = 50f;
        float dragStartY = topControlsHeight * 2;
        float dragFullY = dragStartY - topControlsHeight;

        long downTime = SystemClock.uptimeMillis();
        dragStart(dragX, dragStartY, downTime);
        dragTo(dragX, dragX, dragStartY, dragFullY, 100, downTime);
        assertEquals(0f, waitForTopControlsPosition(0f));
        dragEnd(dragX, dragFullY, downTime);
        assertEquals(0f, waitForTopControlsPosition(0f));
    }

    @LargeTest
    @Feature({"Fullscreen"})
    public void testControlsShownOnUnresponsiveRenderer()
            throws InterruptedException, ExecutionException {
        startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.setAnimationDurationsForTest(1, 1);
        assertTrue(waitForNoBrowserTopControlsOffset());
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
        assertEquals(0f, waitForTopControlsPosition(0f));

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                delegate.rendererResponsive();
            }
        });
        assertTrue(waitForNoBrowserTopControlsOffset());
    }

    /*
    @LargeTest
    @Feature({"Fullscreen"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    crbug.com/339668
    */
    @FlakyTest
    public void testPrerenderedPageSupportsManualHiding()
            throws InterruptedException, ExecutionException {
        startMainActivityOnBlankPage();

        ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        fullscreenManager.disableBrowserOverrideForTest();

        final Tab tab = getActivity().getActivityTab();
        final String testUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/very_long_google.html");
        PrerenderTestHelper.trainAutocompleteActionPredictorAndTestPrerender(testUrl, this);
        assertTrue("loadUrl did not use pre-rendered page.",
                PrerenderTestHelper.isLoadUrlResultPrerendered(loadUrl(testUrl)));

        UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.processEnableFullscreenRunnableForTest();
            }
        });

        scrollTopControls(false);
    }

    /*
    Marked flaky on 2015-07-20: http://crbug.com/512299
    @LargeTest
    @Feature({"Fullscreen"})
    */
    @FlakyTest
    public void testTopControlsShownWhenInputIsFocused()
            throws InterruptedException, ExecutionException {
        startMainActivityWithURL(LONG_HTML_WITH_AUTO_FOCUS_INPUT_TEST_PAGE);

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
        assertTrue(waitForNoBrowserTopControlsOffset());
        assertEquals(fullscreenManager.getControlOffset(), 0f);

        Tab tab = getActivity().getActivityTab();
        singleClickView(tab.getView());
        waitForEditableNodeToLoseFocus(tab);
        scrollTopControls(false);
        scrollTopControls(true);
    }

    private void scrollTopControls(boolean show) throws InterruptedException, ExecutionException {
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
        assertEquals(expectedPosition, waitForTopControlsPosition(expectedPosition));
        dragEnd(dragX, dragEndY, downTime);
        assertEquals(expectedPosition, waitForTopControlsPosition(expectedPosition));
    }

    private void togglePersistentFullscreen(final TabWebContentsDelegateAndroid delegate,
            final boolean state) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                delegate.toggleFullscreenModeForTab(state);
            }
        });
    }

    private static boolean isFlagSet(int flags, int flag) {
        return (flags & flag) == flag;
    }

    private boolean isFullscreenFlagSet(final Tab tab, final boolean state) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            View view = tab.getContentViewCore().getContainerView();
            int visibility = view.getSystemUiVisibility();
            // SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN should only be used during the transition between
            // fullscreen states, so it should always be cleared when fullscreen transitions are
            // completed.
            return (!isFlagSet(visibility, View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN))
                    && (isFlagSet(visibility, View.SYSTEM_UI_FLAG_FULLSCREEN) == state);
        } else {
            WindowManager.LayoutParams attributes =
                    getActivity().getWindow().getAttributes();
            return isFlagSet(
                    attributes.flags, WindowManager.LayoutParams.FLAG_FULLSCREEN) == state;
        }
    }

    private boolean waitForFullscreenFlag(final Tab tab, final boolean state)
            throws InterruptedException {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isFullscreenFlagSet(tab, state);
            }
        });
    }

    private boolean waitForPersistentFullscreen(final TabWebContentsDelegateAndroid delegate,
            final boolean state) throws InterruptedException {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return state == delegate.isFullscreenForTabOrPending();
            }
        });
    }

    private float waitForTopControlsPosition(final float position)
            throws InterruptedException, ExecutionException {
        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return position == fullscreenManager.getControlOffset();
            }
        });
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Float>() {
            @Override
            public Float call() throws Exception {
                return fullscreenManager.getControlOffset();
            }
        });
    }

    private boolean waitForNoBrowserTopControlsOffset() throws InterruptedException {
        final ChromeFullscreenManager fullscreenManager = getActivity().getFullscreenManager();
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !fullscreenManager.hasBrowserControlOffsetOverride();
            }
        });
    }

    private boolean waitForPageToBeScrollable(final Tab tab) throws InterruptedException {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ContentViewCore contentViewCore = tab.getContentViewCore();
                return contentViewCore.computeVerticalScrollRange()
                        > contentViewCore.getContainerView().getHeight();
            }
        });
    }

    private boolean waitForEditableNodeToLoseFocus(final Tab tab) throws InterruptedException {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ContentViewCore contentViewCore = tab.getContentViewCore();
                return !contentViewCore.isFocusedNodeEditable();
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
