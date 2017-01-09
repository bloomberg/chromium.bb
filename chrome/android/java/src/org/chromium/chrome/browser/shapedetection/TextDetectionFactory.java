// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shapedetection;

import android.app.Activity;

import org.chromium.blink.mojom.TextDetection;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.ui.base.WindowAndroid;

/**
 * Factory class registered to create TextDetections upon request.
 */
public class TextDetectionFactory implements InterfaceFactory<TextDetection> {
    private final WebContents mWebContents;

    public TextDetectionFactory(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public TextDetection createImpl() {
        // Get android.content.Context out of |mWebContents|.

        final ContentViewCore contentViewCore = ContentViewCore.fromWebContents(mWebContents);
        if (contentViewCore == null) return null;

        final WindowAndroid window = contentViewCore.getWindowAndroid();
        if (window == null) return null;

        final Activity context = window.getActivity().get();
        if (context == null) return null;

        return new TextDetectionImpl(context);
    }
}
