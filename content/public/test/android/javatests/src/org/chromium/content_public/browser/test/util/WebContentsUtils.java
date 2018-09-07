// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.ExecutionException;

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

    /**
     * Issues a fake notification about the renderer being killed.
     *
     * @param webContents The WebContents in use.
     * @param wasOomProtected True if the renderer was protected from the OS out-of-memory killer
     *                        (e.g. renderer for the currently selected tab)
     */
    public static void simulateRendererKilled(WebContents webContents, boolean wasOomProtected) {
        ThreadUtils.runOnUiThreadBlocking(() ->
            ((WebContentsImpl) webContents).simulateRendererKilledForTesting(wasOomProtected));
    }

    /**
     * Returns {@link ImeAdapter} instance associated with a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static ImeAdapter getImeAdapter(WebContents webContents) {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> ImeAdapter.fromWebContents(webContents));
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns {@link GestureListenerManager} instance associated with a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static GestureListenerManager getGestureListenerManager(WebContents webContents) {
        try {
            return ThreadUtils.runOnUiThreadBlocking(
                    () -> GestureListenerManager.fromWebContents(webContents));
        } catch (ExecutionException e) {
            return null;
        }
    }

    /**
     * Returns {@link ViewEventSink} instance associated with a given {@link WebContents}.
     * @param webContents The WebContents in use.
     */
    public static ViewEventSink getViewEventSink(WebContents webContents) {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> ViewEventSink.from(webContents));
        } catch (ExecutionException e) {
            return null;
        }
    }

    private static native void nativeReportAllFrameSubmissions(
            WebContents webContents, boolean enabled);
    private static native RenderFrameHost nativeGetFocusedFrame(WebContents webContents);
}
