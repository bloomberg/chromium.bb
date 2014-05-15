// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * This will provide an implementation of GCMDriver using the Java GCM APIs.
 */
@JNINamespace("gcm")
public final class GCMDriver {
    /*
     * JNI Generator complains if there are no methods exposed to JNI.
     * TODO(johnme): Replace this with something useful.
     */
    @CalledByNative
    private static void doNothing() {
    }
}