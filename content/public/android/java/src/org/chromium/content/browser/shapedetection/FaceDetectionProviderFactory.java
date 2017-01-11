// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.shapedetection;

import android.content.Context;

import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.shape_detection.mojom.FaceDetectionProvider;

/**
 * A factory method for registry in InterfaceRegistrarImpl.java.
 */
public class FaceDetectionProviderFactory implements InterfaceFactory<FaceDetectionProvider> {
    public FaceDetectionProviderFactory(Context context) {}

    @Override
    public FaceDetectionProvider createImpl() {
        return new FaceDetectionProviderImpl();
    }
}
