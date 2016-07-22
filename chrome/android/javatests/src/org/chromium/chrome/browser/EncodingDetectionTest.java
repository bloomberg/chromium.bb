// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Environment;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests text encoding detection.
 */
public class EncodingDetectionTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    // TODO(jinsukkim): Fix and enable BrowserEncodingTest::TestEncodingAutoDetect()
    //     once auto-detection turns on for desktop platforms. Then this test can
    //     be removed safely since the native part handling encoding detection is
    //     shared with Android platform.

    private EmbeddedTestServer mTestServer;

    public EncodingDetectionTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    @Feature({"Encoding"})
    public void testAutodetectEncoding() throws Exception {

        String testUrl = mTestServer.getURL(
                "/chrome/test/data/encoding_tests/auto_detect/"
                + "Big5_with_no_encoding_specified.html");
        loadUrl(testUrl);
        assertEquals("Wrong page encoding detected", "Big5",
                getActivity().getCurrentContentViewCore().getWebContents().getEncoding());
    }
}
