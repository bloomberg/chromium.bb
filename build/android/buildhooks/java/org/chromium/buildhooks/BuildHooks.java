// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.buildhooks;

/**
 * Callback interface.
 */
public class BuildHooks {
    static Callback<AssertionError> sAssertCallback;
    /**
     * Handle AssertionError, decide whether throw or report without crashing base on gn arg.
     */
    public static void assertFailureHandler(AssertionError assertionError) throws AssertionError {
        if (sAssertCallback != null) {
            sAssertCallback.run(assertionError);
        } else {
            throw assertionError;
        }
    }

    public static void setAssertCallback(Callback<AssertionError> callback) {
        sAssertCallback = callback;
    }
}
