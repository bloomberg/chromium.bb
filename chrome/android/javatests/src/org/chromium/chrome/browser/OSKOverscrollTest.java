// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Rect;
import android.support.test.filters.MediumTest;
import android.test.MoreAsserts;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;

import java.util.Locale;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;


/**
 * Integration test to ensure that OSK resizes only the visual viewport.
 */

public class OSKOverscrollTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String FIXED_FOOTER_PAGE = UrlUtils.encodeHtmlDataUri(""
            + "<html>"
            + "<head>"
            + "  <meta name=\"viewport\" "
            + "    content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" />"
            + "  <style>"
            + "    body {"
            + "      height:1500px;"
            + "      margin:0px;"
            + "    }"
            + "    #footer {"
            + "      position:fixed;"
            + "      left:0px;"
            + "      bottom:0px;"
            + "      height:50px;"
            + "      width:100%;"
            + "      background:#FFFF00;"
            + "    }"
            + "  </style>"
            + "</head>"
            + "<body>"
            + "  <form method=\"POST\">"
            + "    <input type=\"text\" id=\"fn\"/><br>"
            + "    <div id=\"footer\"></div>"
            + "  </form>"
            + "</body>"
            + "</html>");

    // We convert CSS pixels into device pixels and compare the viewport size before and after the
    // keyboard show. window.innerHeight returns an integer and the actual height is a floating
    // point. Need some buffer for error.
    private static final int ERROR_EPS_PIX = 1;

    public OSKOverscrollTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() {
        // Don't launch activity automatically.
    }

    private void waitForKeyboard() {
        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(new Criteria("Keyboard was never shown.") {
            @Override
            public boolean isSatisfied() {
                return UiUtils.isKeyboardShowing(
                        getActivity(),
                        getActivity().getCurrentContentViewCore().getContainerView());
            }
        });
    }

    private int getViewportHeight(WebContents webContents) {
        try {
            String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, "window.innerHeight");
            MoreAsserts.assertNotEqual(jsonText.trim().toLowerCase(Locale.US), "null");
            return Integer.parseInt(jsonText);
        } catch (Exception ex) {
            fail(ex.toString());
        }
        return -1;
    }

    private boolean almostEqual(int a, int b) {
        return Math.abs(a - b) <= ERROR_EPS_PIX;
    }

    /**
     * Verify that OSK show only resizes the visual viewport when the ENABLE_OSK_OVERSCROLL flag is
     * set.
     * @throws InterruptedException
     * @throws TimeoutException
     * @throws ExecutionException
     */
    @MediumTest
    @CommandLineFlags.Add({ChromeSwitches.ENABLE_OSK_OVERSCROLL})
    @RetryOnFailure
    public void testOnlyVisualViewportResizes()
            throws InterruptedException, TimeoutException, ExecutionException {
        startMainActivityWithURL(FIXED_FOOTER_PAGE);

        final AtomicReference<ContentViewCore> viewCoreRef = new AtomicReference<ContentViewCore>();
        final AtomicReference<WebContents> webContentsRef = new AtomicReference<WebContents>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                viewCoreRef.set(getActivity().getCurrentContentViewCore());
                webContentsRef.set(viewCoreRef.get().getWebContents());
            }
        });

        DOMUtils.waitForNonZeroNodeBounds(webContentsRef.get(), "fn");

        // Get the position of the footer and the viewport height before bringing up the OSK.
        Rect footerPositionBefore = DOMUtils.getNodeBounds(webContentsRef.get(), "footer");
        final int viewportHeightBeforeCss = getViewportHeight(webContentsRef.get());
        final float cssToDevicePixFactor = viewCoreRef.get().getPageScaleFactor()
                * viewCoreRef.get().getDeviceScaleFactor();

        // Click on the unfocused input element for the first time to focus on it. This brings up
        // the OSK.
        DOMUtils.clickNode(viewCoreRef.get(), "fn");

        waitForKeyboard();

        // Get the position of the footer after bringing up the OSK. This should be the same as the
        // position before because only the visual viewport should have resized.
        Rect footerPositionAfter = DOMUtils.getNodeBounds(webContentsRef.get(), "footer");
        assertEquals(footerPositionBefore, footerPositionAfter);

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // Verify that the size of the viewport before the OSK show is equal to the size of
                // the viewport after the OSK show plus the size of the keyboard.
                int viewportHeightAfterCss = getViewportHeight(webContentsRef.get());
                int keyboardHeight =
                        viewCoreRef.get().getContentViewClient().getSystemWindowInsetBottom();

                int priorHeight = (int) (viewportHeightBeforeCss * cssToDevicePixFactor);
                int afterHeightPlusKeyboard =
                        (int) (viewportHeightAfterCss * cssToDevicePixFactor) + keyboardHeight;
                updateFailureReason("Values [" + priorHeight + "], [" + afterHeightPlusKeyboard
                        + "] did not match within allowed error range [" + ERROR_EPS_PIX + "]");
                return almostEqual(priorHeight, afterHeightPlusKeyboard);
            }
        });
    }
}
