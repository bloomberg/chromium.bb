// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shapedetection;

import android.app.Activity;

import org.chromium.blink.mojom.BarcodeDetection;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.ui.base.WindowAndroid;

/**
 * Factory class registered to create BarcodeDetections upon request.
 */
public class BarcodeDetectionFactory implements InterfaceFactory<BarcodeDetection> {
    private final WebContents mWebContents;

    public BarcodeDetectionFactory(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public BarcodeDetection createImpl() {
        // Get android.content.Context out of |mWebContents|.
        final ContentViewCore contentViewCore = ContentViewCore.fromWebContents(mWebContents);
        if (contentViewCore == null) return null;

        final WindowAndroid window = contentViewCore.getWindowAndroid();
        if (window == null) return null;

        final Activity context = window.getActivity().get();
        if (context == null) return null;

        return new BarcodeDetectionImpl(context);
    }
}
