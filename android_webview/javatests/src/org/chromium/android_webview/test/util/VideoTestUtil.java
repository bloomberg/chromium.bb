// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwTestBase;
import org.chromium.android_webview.test.TestAwContentsClient;

/**
 * Code shared between the various video tests.
 */
public class VideoTestUtil {
    /**
     * Run video test.
     * @param testCase the test case instance we're going to run the test in.
     * @param requiredUserGesture the settings of MediaPlaybackRequiresUserGesture.
     * @return true if the event happened,
     * @throws Throwable throw exception if timeout.
     */
   public static boolean runVideoTest(final AwTestBase testCase, final boolean requiredUserGesture,
           long waitTime) throws Throwable {
        final JavascriptEventObserver observer = new JavascriptEventObserver();
        TestAwContentsClient client = new TestAwContentsClient();
        final AwContents awContents =
            testCase.createAwTestContainerViewOnMainSync(client).getAwContents();
        testCase.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                AwSettings awSettings = awContents.getSettings();
                awSettings.setJavaScriptEnabled(true);
                awSettings.setMediaPlaybackRequiresUserGesture(requiredUserGesture);
                observer.register(awContents.getContentViewCore(), "javaObserver");
            }
        });
        VideoTestWebServer webServer = new VideoTestWebServer(testCase.getActivity());
        try {
            String data = "<html><head><script>" +
                "addEventListener('DOMContentLoaded', function() { " +
                "  document.getElementById('video').addEventListener('play', function() { " +
                "    javaObserver.notifyJava(); " +
                "  }, false); " +
                "}, false); " +
                "</script></head><body>" +
                "<video id='video' autoplay control src='" +
                webServer.getOnePixelOneFrameWebmURL() + "' /> </body></html>";
            testCase.loadDataAsync(awContents, data, "text/html", false);
            return observer.waitForEvent(waitTime);
        } finally {
            if (webServer != null && webServer.getTestWebServer() != null)
                webServer.getTestWebServer().shutdown();
        }
    }
}
