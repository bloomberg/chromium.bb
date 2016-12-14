// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.shapedetection;

import org.chromium.blink.mojom.FaceDetection;
import org.chromium.blink.mojom.FaceDetectionProvider;
import org.chromium.blink.mojom.FaceDetectorOptions;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;

/**
 * Service provider to create FaceDetection and BarcodeDetection services
 */
public class FaceDetectionProviderImpl implements FaceDetectionProvider {
    @Override
    public void createFaceDetection(
            InterfaceRequest<FaceDetection> request, FaceDetectorOptions options) {
        FaceDetection.MANAGER.bind(new FaceDetectionImpl(options), request);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
