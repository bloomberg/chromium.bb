// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;

/**
 * TestSuite for EME key systems.
 */
public class KeySystemTest extends AwTestBase {

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwContents mAwContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(mAwContents);

        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
            getKeySystemTestPage(), "text/html", false);
    }

    private String getKeySystemTestPage() {
        return "<html> <script>" +
                "function isKeySystemSupported(keySystem) {" +
                    "var video = document.createElement('video');" +
                    "return video.canPlayType('video/mp4', keySystem);" +
                "}" +
                "</script> </html>";
    }

    private String IsKeySystemSupported(String keySystem) throws Exception {
        return executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                  "isKeySystemSupported('" + keySystem + "')");
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportClearKeySystem() throws Throwable {
        assertEquals("\"maybe\"", IsKeySystemSupported("webkit-org.w3.clearkey"));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportWidevineKeySystem() throws Throwable {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            return;  // MediaDrm/Crypto is supported from KitKat.
        }
        assertEquals("\"maybe\"", IsKeySystemSupported("com.widevine.alpha"));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testNotSupportFooKeySystem() throws Throwable {
        assertEquals("\"\"", IsKeySystemSupported("com.foo.keysystem"));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportPlatformKeySystem() throws Throwable {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            return;  // MediaDrm/Crypto is supported from KitKat.
        }
        assertEquals("\"maybe\"", IsKeySystemSupported("com.oem.test-keysystem"));
    }
}
