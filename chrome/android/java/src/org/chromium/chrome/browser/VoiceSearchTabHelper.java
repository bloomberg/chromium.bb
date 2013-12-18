// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.WebContents;
import org.chromium.content.browser.WebContentsObserverAndroid;

/**
 * Tab helper to toggle media autoplay for voice URL searches.
 */
public class VoiceSearchTabHelper extends WebContentsObserverAndroid {
    private static final String TAG = "VoiceSearchTabHelper";
    private long mNativeVoiceSearchTabHelper;
    private WebContents mWebContents;

    /**
     * Create an instance of VoiceSearchTabHelper.
     *
     * @param contentViewCore ContentViewCore to update media autoplay status.
     */
    public VoiceSearchTabHelper(ContentViewCore contentViewCore) {
        super(contentViewCore);
        mWebContents = contentViewCore.getWebContents();
    }

    @Override
    public void navigationEntryCommitted() {
        nativeUpdateAutoplayStatus(mWebContents);
    }

    private native void nativeUpdateAutoplayStatus(WebContents webContents);
}
