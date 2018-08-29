// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

/**
 * Collection of test-only WebContents utilities.
 */
@JNINamespace("content")
public class WebContentsUtils {
    /**
     * Reports all frame submissions to the browser process, even those that do not impact Browser
     * UI.
     * @param webContents The WebContents for which to report all frame submissions.
     * @param enabled Whether to report all frame submissions.
     */
    public static void reportAllFrameSubmissions(final WebContents webContents, boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> { nativeReportAllFrameSubmissions(webContents, enabled); });
    }

    /**
     * @param webContents The WebContents on which the SelectPopup is being shown.
     * @return {@code true} if select popup is being shown.
     */
    public static boolean isSelectPopupVisible(WebContents webContents) {
        return SelectPopup.fromWebContents(webContents).isVisibleForTesting();
    }

    /**
     * Gets the currently focused {@link RenderFrameHost} instance for a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static RenderFrameHost getFocusedFrame(final WebContents webContents) {
        return nativeGetFocusedFrame(webContents);
    }

    private static native void nativeReportAllFrameSubmissions(
            WebContents webContents, boolean enabled);
    private static native RenderFrameHost nativeGetFocusedFrame(WebContents webContents);
}
