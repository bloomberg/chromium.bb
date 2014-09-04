// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings.test.mojom.mojo;

import org.chromium.mojo.bindings.MessageReceiver;
import org.chromium.mojo.bindings.test.mojom.mojo.IntegrationTestInterface2.Method0Response;
import org.chromium.mojo.bindings.test.mojom.mojo.IntegrationTestInterface2_Internal.IntegrationTestInterface2Method0ResponseParamsForwardToCallback;

/**
 * Helper class to access {@link IntegrationTestInterface2_Internal} package protected method for
 * tests.
 */
public class IntegrationTestInterface2TestHelper {

    private static final class SinkMethod0Response implements Method0Response {
        @Override
        public void call(byte[] arg1) {
        }
    }

    /**
     * Creates a new {@link MessageReceiver} to use for the callback of
     * |IntegrationTestInterface2#method0(Method0Response)|.
     */
    public static MessageReceiver newIntegrationTestInterface2MethodCallback() {
        return new IntegrationTestInterface2Method0ResponseParamsForwardToCallback(
                new SinkMethod0Response());
    }
}
