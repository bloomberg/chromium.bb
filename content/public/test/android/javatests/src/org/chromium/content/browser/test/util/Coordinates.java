// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents;

/**
 * Util class that allows tests to get various view-related coordinate values and
 * scale factors from {@link RenderCoordinates}.
 */
public class Coordinates {
    private final RenderCoordinates mRenderCoordinates;

    public static Coordinates createFor(WebContents webContents) {
        return new Coordinates(webContents);
    }

    private Coordinates(WebContents webContents) {
        mRenderCoordinates = ((WebContentsImpl) webContents).getRenderCoordinates();
    }

    public float getPageScaleFactor() {
        return mRenderCoordinates.getPageScaleFactor();
    }

    public float getDeviceScaleFactor() {
        return mRenderCoordinates.getDeviceScaleFactor();
    }

    public float fromLocalCssToPix(float css) {
        return mRenderCoordinates.fromLocalCssToPix(css);
    }

    public int getScrollXPixInt() {
        return mRenderCoordinates.getScrollXPixInt();
    }

    public int getScrollYPixInt() {
        return mRenderCoordinates.getScrollYPixInt();
    }

    public int getContentWidthPixInt() {
        return mRenderCoordinates.getContentWidthPixInt();
    }

    public int getContentHeightPixInt() {
        return mRenderCoordinates.getContentHeightPixInt();
    }
}
