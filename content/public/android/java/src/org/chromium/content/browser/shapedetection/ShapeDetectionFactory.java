// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.shapedetection;

import android.content.Context;

import org.chromium.blink.mojom.ShapeDetection;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * A factory method for registry in InterfaceRegistrarImpl.java.
 */
public class ShapeDetectionFactory implements InterfaceFactory<ShapeDetection> {
    public ShapeDetectionFactory(Context context) {}

    @Override
    public ShapeDetection createImpl() {
        return new ShapeDetectionImpl();
    }
}
