// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import java.lang.reflect.InvocationTargetException;

/** This class provides methods to call into AR. */
public class ArDelegate {
    /**
     * Needs to be called once native libraries are available. Has no effect if AR is not compiled
     * into Chrome.
     **/
    public static void maybeInit() {
        try {
            // AR may not be compiled into Chrome. Thus, use reflection to access it.
            Class.forName("org.chromium.chrome.browser.vr.ArCoreJavaUtils")
                    .getDeclaredMethod("installArCoreDeviceProviderFactory")
                    .invoke(null);
        } catch (ClassNotFoundException e) {
            // AR not available. No initialization required.
        } catch (NoSuchMethodException e) {
            // This and the exceptions below should not happen. Multiple catch statements to work
            // around ReflectiveOperationException in API level 19.
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        } catch (InvocationTargetException e) {
            throw new RuntimeException(e);
        }
    }

    private ArDelegate() {}
}
