// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.support.test.filters.MediumTest;
import android.util.DisplayMetrics;
import android.view.WindowManager;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Test suite for viewport-related properties.
 */
public class ViewportTest extends ContentShellTestBase {

    protected String evaluateStringValue(String expression) throws Throwable {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(),
                expression);
    }

    protected float evaluateFloatValue(String expression) throws Throwable {
        return Float.valueOf(evaluateStringValue(expression));
    }

    protected int evaluateIntegerValue(String expression) throws Throwable {
        return Integer.parseInt(evaluateStringValue(expression));
    }

    @MediumTest
    @Feature({"Viewport", "InitialViewportSize"})
    public void testDefaultViewportSize() throws Throwable {
        launchContentShellWithUrl("about:blank");
        waitForActiveShellToBeDoneLoading();

        Context context = getInstrumentation().getTargetContext();
        WindowManager winManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        DisplayMetrics metrics = new DisplayMetrics();
        winManager.getDefaultDisplay().getMetrics(metrics);

        // window.devicePixelRatio should match the default display. Only check to 1 decimal place
        // to allow for rounding.
        assertEquals(metrics.density, evaluateFloatValue("window.devicePixelRatio"), 0.1);

        // Check that the viewport width is vaguely sensible.
        int viewportWidth = evaluateIntegerValue("document.documentElement.clientWidth");
        assertTrue(Math.abs(evaluateIntegerValue("window.innerWidth") - viewportWidth) <= 1);
        assertTrue(viewportWidth >= 979);
        assertTrue(viewportWidth <= Math.max(981, metrics.widthPixels / metrics.density + 1));
    }
}
