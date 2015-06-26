// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.test.ActivityInstrumentationTestCase2;

import org.chromium.base.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests running end-to-end layout tests.
 */
public class WebViewLayoutTest
        extends ActivityInstrumentationTestCase2<WebViewLayoutTestActivity> {

    private static final String TAG = "WebViewLayoutTest";
    private static final String PATH_PREFIX = "/data/local/tmp/webview_test/";
    private static final long TIMEOUT_SECONDS = 20;
    private WebViewLayoutTestActivity mTestActivity;

    public WebViewLayoutTest() {
        super(WebViewLayoutTestActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestActivity = (WebViewLayoutTestActivity) getActivity();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestActivity.finish();
        super.tearDown();
    }

    @Override
    public WebViewLayoutTestRunner getInstrumentation() {
        return (WebViewLayoutTestRunner) super.getInstrumentation();
    }

    public void testSimple() throws Exception {
        runTest("experimental/basic-logging.html", "experimental/basic-logging-expected.txt");
    }

    public void testGlobalInterface() throws Exception {
        runTest("webexposed/global-interface-listing.html",
                "webexposed/global-interface-listing-expected.txt");
    }

    // test helper methods
    private void runTest(final String fileName, final String fileNameExpected)
            throws FileNotFoundException, IOException, InterruptedException, TimeoutException {
        loadUrlWebViewAsync("file://" + PATH_PREFIX + fileName, mTestActivity);

        if (getInstrumentation().isRebaseline()) {
            // this is the rebase line process;
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
            writeFile(fileNameExpected, result, mTestActivity.getFilesDir());
            Log.i(TAG, "file: " + fileNameExpected + " --> rebaselined, length=" + result.length());
        } else {
            String expected = readFile(PATH_PREFIX + fileNameExpected);
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
            assertEquals(expected, result);
        }
    }

    private void loadUrlWebViewAsync(final String fileUrl,
            final WebViewLayoutTestActivity activity) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                activity.loadUrl(fileUrl);
            }
        });
    }

    private static String readFile(String fileName) throws IOException {
        FileInputStream inputStream = new FileInputStream(new File(fileName));
        try {
            BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
            try {
                StringBuilder contents = new StringBuilder();
                String line;

                while ((line = reader.readLine()) != null) {
                    contents.append(line);
                    contents.append("\n");
                }
                return contents.toString();
            } finally {
                reader.close();
            }
        } finally {
            inputStream.close();
        }
    }

    private static void writeFile(final String fileName, final String contents,
            final File internalFilesDir) throws FileNotFoundException, IOException {
        File fileOut = new File(internalFilesDir, fileName);
        String absolutePath = fileOut.getAbsolutePath();
        String path = absolutePath.substring(0, absolutePath.lastIndexOf("/"));
        boolean mkdirsSuccess = new File(path).mkdirs();
        if (!mkdirsSuccess)
            throw new IOException("failed to create directories: " + path);

        FileOutputStream outputStream = new FileOutputStream(fileOut);
        try {
            outputStream.write(contents.getBytes());
        } finally {
            outputStream.close();
        }
    }
}