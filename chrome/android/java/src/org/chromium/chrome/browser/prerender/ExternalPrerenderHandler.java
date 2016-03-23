// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/**
 * A handler class for prerender requests coming from  other applications.
 */
@JNINamespace("prerender")
public class ExternalPrerenderHandler {

    private long mNativeExternalPrerenderHandler;

    public ExternalPrerenderHandler() {
        mNativeExternalPrerenderHandler = nativeInit();
    }

    /**
     * Add a prerender for the given url and given content view dimensions.
     * <p>
     * The generated {@link WebContents} does not actually contain the prerendered contents but
     * must be used as the container that you load the prerendered URL into.
     *
     * @param profile The profile to use for the prerender.
     * @param url The url to prerender.
     * @param referrer The referrer for the prerender request.
     * @param width The width for the content view (render widget host view) for the prerender.
     * @param height The height for the content view (render widget host view) for the prerender.
     * @param prerenderOnCellular Whether the prerender should happen if the device has a cellular
     *                            connection.
     * @return The {@link WebContents} that is linked to this prerender. {@code null} if
     *         unsuccessful.
     */
    public WebContents addPrerender(Profile profile, String url, String referrer, int width,
            int height, boolean prerenderOnCellular) {
        WebContents webContents = WebContentsFactory.createWebContents(false, false);
        if (addPrerender(profile, webContents, url, referrer, width, height, prerenderOnCellular)) {
            return webContents;
        }
        if (webContents != null) webContents.destroy();
        return null;
    }

    /**
     * Adds a prerender for the given URL to an existing {@link WebContents} with the given
     * dimensions.
     *
     * @param profile The profile to use for the prerender.
     * @param webContents The WebContents to add the prerender to.
     * @param url The url to prerender.
     * @param referrer The referrer for the prerender request.
     * @param width The width for the content view (render widget host view) for the prerender.
     * @param height The height for the content view (render widget host view) for the prerender.
     * @param prerenderOnCellular Whether the prerender should happen if the device has a cellular
     *                            connection.
     * @return Whether the prerender was successful.
     */
    public boolean addPrerender(Profile profile, WebContents webContents, String url,
            String referrer, int width, int height, boolean prerenderOnCellular) {
        return nativeAddPrerender(mNativeExternalPrerenderHandler, profile, webContents,
                url, referrer, width, height, prerenderOnCellular);
    }

    /**
     * Cancel the current prerender action on this {@link ExternalPrerenderHandler}.
     */
    public void cancelCurrentPrerender() {
        nativeCancelCurrentPrerender(mNativeExternalPrerenderHandler);
    }

    /**
     * Check whether a given url has been prerendering for the given profile and session id for the
     * given web contents.
     * @param profile The profile to check for prerendering.
     * @param url The url to check for prerender.
     * @param webContents The {@link WebContents} for which to compare the session info.
     * @return Whether the given url was prerendered.
     */
    @VisibleForTesting
    public static boolean hasPrerenderedUrl(Profile profile, String url, WebContents webContents)  {
        return nativeHasPrerenderedUrl(profile, url, webContents);
    }

    /**
     * Check whether a given url has been prerendering for the given profile and session id for the
     * given web contents, and has finished loading.
     * @param profile The profile to check for prerendering.
     * @param url The url to check for prerender.
     * @param webContents The {@link WebContents} for which to compare the session info.
     * @return Whether the given url was prerendered and has finished loading.
     */
    @VisibleForTesting
    public static boolean hasPrerenderedAndFinishedLoadingUrl(
            Profile profile, String url, WebContents webContents) {
        return nativeHasPrerenderedAndFinishedLoadingUrl(profile, url, webContents);
    }

    private static native long nativeInit();
    private static native boolean nativeAddPrerender(
            long nativeExternalPrerenderHandlerAndroid, Profile profile,
            WebContents webContents, String url, String referrer,
            int width, int height, boolean prerenderOnCellular);
    private static native boolean nativeHasPrerenderedUrl(
            Profile profile, String url, WebContents webContents);
    private static native boolean nativeHasPrerenderedAndFinishedLoadingUrl(
            Profile profile, String url, WebContents webContents);
    private static native void nativeCancelCurrentPrerender(
            long nativeExternalPrerenderHandlerAndroid);
}
