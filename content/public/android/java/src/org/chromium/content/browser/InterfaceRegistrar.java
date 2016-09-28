// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Registers interfaces exposed by the browser in the given registry.
 */
@JNINamespace("content")
class InterfaceRegistrar {
    @CalledByNative
    static void exposeInterfacesToRenderer(InterfaceRegistry registry, Context applicationContext) {
        assert applicationContext != null;
    }

    @CalledByNative
    static void exposeInterfacesToFrame(InterfaceRegistry registry,
            Context applicationContext, WebContents contents) {
        assert applicationContext != null;
    }
}
