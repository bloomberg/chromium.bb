// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Base class for tests related to checking connectivity.
 *
 * It includes a {@link ConnectivityTestServer} which is set up and torn down automatically
 * for tests.
 */
public class ConnectivityCheckerTestBase extends NativeLibraryTestBase {

    protected static final int TIMEOUT_MS = 5000;

    private EmbeddedTestServer mTestServer;
    protected String mGenerate200Url;
    protected String mGenerate204Url;
    protected String mGenerate302Url;
    protected String mGenerate404Url;
    protected String mGenerateSlowUrl;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();

        mTestServer = EmbeddedTestServer.createAndStartDefaultServer(
                getInstrumentation().getContext());
        mGenerate200Url = mTestServer.getURL("/echo?status=200");
        mGenerate204Url = mTestServer.getURL("/echo?status=204");
        mGenerate302Url = mTestServer.getURL("/echo?status=302");
        mGenerate404Url = mTestServer.getURL("/echo?status=404");
        mGenerateSlowUrl = mTestServer.getURL("/slow?5");
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }
}
