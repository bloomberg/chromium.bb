// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.shapedetection;

import android.content.Context;

import org.chromium.blink.mojom.FaceDetection;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * A factory method for registry in InterfaceRegistrarImpl.java.
 */
public class FaceDetectionFactory implements InterfaceFactory<FaceDetection> {
    public FaceDetectionFactory(Context context) {}

    @Override
    public FaceDetection createImpl() {
        return new FaceDetectionImpl();
    }
}
