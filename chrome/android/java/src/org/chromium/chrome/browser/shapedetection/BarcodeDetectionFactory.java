// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shapedetection;

import android.content.Context;

import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.shape_detection.mojom.BarcodeDetection;

/**
 * Factory class registered to create BarcodeDetections upon request.
 */
public class BarcodeDetectionFactory implements InterfaceFactory<BarcodeDetection> {
    private final Context mContext;

    public BarcodeDetectionFactory(Context context) {
        mContext = context;
    }

    @Override
    public BarcodeDetection createImpl() {
        return new BarcodeDetectionImpl(mContext);
    }
}
