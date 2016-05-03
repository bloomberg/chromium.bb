// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;


import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;

/**
 * This class tests AwQuotaManagerBridge runs without AwContents etc. It simulates
 * use case that user calls WebStorage getInstance() without WebView.
 */
public class StandaloneAwQuotaManagerBridgeTest extends AwTestBase {
    public void testStartup() throws Exception {
        // AwQuotaManager should run without any issue.
        AwQuotaManagerBridge.Origins origins = AwQuotaManagerBridgeTestUtil.getOrigins(this);
        assertEquals(origins.mOrigins.length, 0);
        assertEquals(AwContents.getNativeInstanceCount(), 0);
    }
}
