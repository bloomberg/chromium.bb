// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shapedetection;

import android.content.Context;

import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.shape_detection.mojom.TextDetection;

/**
 * Factory class registered to create TextDetections upon request.
 */
public class TextDetectionFactory implements InterfaceFactory<TextDetection> {
    private final Context mContext;

    public TextDetectionFactory(Context context) {
        mContext = context;
    }

    @Override
    public TextDetection createImpl() {
        return new TextDetectionImpl(mContext);
    }
}
