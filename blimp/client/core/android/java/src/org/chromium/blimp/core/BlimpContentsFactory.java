// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.core_public.BlimpContents;

/**
 * This factory creates BlimpContents objects and the associated native counterpart.
 */
@JNINamespace("blimp::client")
public final class BlimpContentsFactory {
    // Don't instantiate me.
    private BlimpContentsFactory() {}

    /**
     * A factory method to build a {@link BlimpContents} object.
     * @return A newly created {@link BlimpContents} object.
     */
    public static BlimpContents createBlimpContents() {
        return nativeCreateBlimpContents();
    }

    private static native BlimpContents nativeCreateBlimpContents();
}
