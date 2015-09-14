// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestActivity;
import org.chromium.net.CronetTestBase;

public class CronetURLStreamHandlerFactoryTest extends CronetTestBase {
    @SmallTest
    @Feature({"Cronet"})
    public void testRequireConfig() throws Exception {
        CronetTestActivity activity = launchCronetTestApp();
        try {
            new CronetURLStreamHandlerFactory(activity, null);
        } catch (NullPointerException e) {
            assertEquals("UrlRequestContextConfig is null.", e.getMessage());
        }
    }
}
