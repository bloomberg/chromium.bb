// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.GeolocationPermissions;

import org.chromium.android_webview.AwContents;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.LocationProviderFactory;
import org.chromium.content.browser.test.util.MockLocationProvider;

import java.util.concurrent.Callable;

/**
 * Test suite for Geolocation in AwContents. Smoke tests for
 * basic functionality, and tests to ensure the AwContents.onPause
 * and onResume APIs affect Geolocation as expected.
 */
public class GeolocationTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private MockLocationProvider mMockLocationProvider;

    private static final String RAW_HTML =
            "<!DOCTYPE html>\n"
            + "<html>\n"
            + "  <head>\n"
            + "    <title>Geolocation</title>\n"
            + "    <script>\n"
            + "      var positionCount = 0;\n"
            + "      function gotPos(position) {\n"
            + "        positionCount++;\n"
            + "      }\n"
            + "      function errorCallback(error){"
            + "        window.document.title = 'deny';"
            + "        console.log('navigator.getCurrentPosition error: ', error);"
            + "      }"
            + "      function initiate_getCurrentPosition() {\n"
            + "        navigator.geolocation.getCurrentPosition(\n"
            + "            gotPos, errorCallback, { });\n"
            + "      }\n"
            + "      function initiate_watchPosition() {\n"
            + "        navigator.geolocation.watchPosition(\n"
            + "            gotPos, errorCallback, { });\n"
            + "      }\n"
            + "    </script>\n"
            + "  </head>\n"
            + "  <body>\n"
            + "  </body>\n"
            + "</html>";

    private static class GrantPermisionAwContentClient extends TestAwContentsClient {
        @Override
        public void onGeolocationPermissionsShowPrompt(String origin,
                GeolocationPermissions.Callback callback) {
            callback.invoke(origin, true, true);
        }
    }

    private static class DefaultPermisionAwContentClient extends TestAwContentsClient {
        @Override
        public void onGeolocationPermissionsShowPrompt(String origin,
                GeolocationPermissions.Callback callback) {
            // This method is empty intentionally to simulate callback is not referenced.
        }
    }

    private void initAwContents(TestAwContentsClient contentsClient) throws Exception {
        mContentsClient = contentsClient;
        mAwContents = createAwTestContainerViewOnMainSync(mContentsClient).getAwContents();
        enableJavaScriptOnUiThread(mAwContents);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.getSettings().setGeolocationEnabled(true);
            }
        });
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mMockLocationProvider = new MockLocationProvider();
        LocationProviderFactory.setLocationProviderImpl(mMockLocationProvider);
    }

    @Override
    public void tearDown() throws Exception {
        mMockLocationProvider.stopUpdates();
        super.tearDown();
    }

    private int getPositionCountFromJS() {
        int result = -1;
        try {
            result = Integer.parseInt(executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "positionCount"));
        } catch (Exception e) {
            fail("Unable to get positionCount");
        }
        return result;
    }

    private void ensureGeolocationRunning(final boolean running) throws Exception {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mMockLocationProvider.isRunning() == running;
            }
        });
    }


    /**
     * Ensure that a call to navigator.getCurrentPosition works in WebView.
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testGetPosition() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScriptForTests("initiate_getCurrentPosition();", null);

        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getPositionCountFromJS() == 1;
            }
        });

        mAwContents.evaluateJavaScriptForTests("initiate_getCurrentPosition();", null);
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getPositionCountFromJS() == 2;
            }
        });
    }

    /**
     * Ensure that a call to navigator.watchPosition works in WebView.
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testWatchPosition() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScriptForTests("initiate_watchPosition();", null);

        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getPositionCountFromJS() > 1;
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPauseGeolocationOnPause() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        // Start a watch going.
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScriptForTests("initiate_watchPosition();", null);

        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getPositionCountFromJS() > 1;
            }
        });

        ensureGeolocationRunning(true);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onPause();
            }
        });

        ensureGeolocationRunning(false);

        try {
            executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, "positionCount = 0");
        } catch (Exception e) {
            fail("Unable to clear positionCount");
        }
        assertEquals(0, getPositionCountFromJS());

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onResume();
            }
        });

        ensureGeolocationRunning(true);

        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getPositionCountFromJS() > 1;
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPauseAwContentsBeforeNavigating() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onPause();
            }
        });

        // Start a watch going.
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScriptForTests("initiate_watchPosition();", null);

        assertEquals(0, getPositionCountFromJS());

        ensureGeolocationRunning(false);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onResume();
            }
        });

        ensureGeolocationRunning(true);

        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return getPositionCountFromJS() > 1;
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testResumeWhenNotStarted() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onPause();
            }
        });

        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onResume();
            }
        });

        ensureGeolocationRunning(false);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testDenyAccessByDefault() throws Throwable {
        initAwContents(new DefaultPermisionAwContentClient());
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScriptForTests("initiate_getCurrentPosition();", null);

        poll(new Callable<Boolean>() {
            @SuppressFBWarnings("DM_GC")
            @Override
            public Boolean call() throws Exception {
                Runtime.getRuntime().gc();
                return "deny".equals(getTitleOnUiThread(mAwContents));
            }
        });
    }

}
