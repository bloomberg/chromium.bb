// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.permission.Resource;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.Callable;

/**
 * TestSuite for EME key systems.
 *
 * MediaDrm support requires KitKat or later.
 * Although, WebView requires Lollipop for the onPermissionRequest() API,
 * this test intercepts this path and thus can run on KitKat.
 */
public class KeySystemTest extends AwTestBase {
    /**
     * AwContentsClient subclass that allows permissions requests for the
     * protected media ID. This is required for all supported key systems other
     * than Clear Key.
     */
    private static class EmeAllowingAwContentsClient extends TestAwContentsClient {
        @Override
        public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
            if (awPermissionRequest.getResources() == Resource.ProtectedMediaId) {
                awPermissionRequest.grant();
            } else {
                awPermissionRequest.deny();
            }
        }
    };

    private TestAwContentsClient mContentsClient = new EmeAllowingAwContentsClient();
    private AwContents mAwContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(mAwContents);

        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                "file:///android_asset/key-system-test.html");
    }

    private String isKeySystemSupported(String keySystem) throws Exception {
        executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "isKeySystemSupported('" + keySystem + "')");

        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return !getResultFromJS().equals("null");
            }
        });

        return getResultFromJS();
    }

    private boolean areProprietaryCodecsSupported() throws Exception {
        String result = executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "areProprietaryCodecsSupported()");
        return !result.isEmpty();
    }

    private String getResultFromJS() {
        String result = "null";
        try {
            result = executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "result");
        } catch (Exception e) {
            fail("Unable to get result");
        }
        return result;
    }

    private String getPlatformKeySystemExpectations() throws Exception {
        // Android key systems only support non-proprietary codecs on Lollipop+.
        // When neither is true isKeySystemSupported() will return an error for
        // all key systems except ClearKey (which is handled by Chrome itself).
        if (!areProprietaryCodecsSupported()
                && Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return "\"NotSupportedError\"";
        }

        return "\"supported\"";
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportClearKeySystem() throws Throwable {
        assertEquals("\"supported\"", isKeySystemSupported("org.w3.clearkey"));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportWidevineKeySystem() throws Throwable {
        assertEquals(
                getPlatformKeySystemExpectations(), isKeySystemSupported("com.widevine.alpha"));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testNotSupportFooKeySystem() throws Throwable {
        assertEquals("\"NotSupportedError\"", isKeySystemSupported("com.foo.keysystem"));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportPlatformKeySystem() throws Throwable {
        assertEquals(getPlatformKeySystemExpectations(),
                isKeySystemSupported("x-com.oem.test-keysystem"));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportPlatformKeySystemNoPrefix() throws Throwable {
        assertEquals("\"NotSupportedError\"", isKeySystemSupported("com.oem.test-keysystem"));
    }
}
