// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.content.browser.WebContentsObserverAndroid;
import org.chromium.content_public.browser.WebContents;

/**
 * Tab helper to toggle media autoplay for voice URL searches.
 */
public class VoiceSearchTabHelper extends WebContentsObserverAndroid {
    private final WebContents mWebContents;

    /**
     * Create an instance of VoiceSearchTabHelper.
     *
     * @param webContents WebContents to update media autoplay status.
     */
    public VoiceSearchTabHelper(WebContents webContents) {
        super(webContents);
        mWebContents = webContents;
    }

    @Override
    public void navigationEntryCommitted() {
        nativeUpdateAutoplayStatus(mWebContents);
    }

    private native void nativeUpdateAutoplayStatus(WebContents webContents);
}
