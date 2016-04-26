// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import android.os.Environment;
import android.test.ActivityInstrumentationTestCase2;
import android.test.suitebuilder.annotation.MediumTest;

import junit.framework.ComparisonFailure;

import org.chromium.base.Log;
import org.chromium.webview_shell.WebViewLayoutTestActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests running end-to-end layout tests.
 */
public class WebViewLayoutTest
        extends ActivityInstrumentationTestCase2<WebViewLayoutTestActivity> {

    private static final String TAG = "WebViewLayoutTest";

    private static final String EXTERNAL_PREFIX =
            Environment.getExternalStorageDirectory().getAbsolutePath() + "/";
    private static final String BASE_WEBVIEW_TEST_PATH =
            "android_webview/tools/system_webview_shell/test/data/";
    private static final String BASE_BLINK_TEST_PATH = "third_party/WebKit/LayoutTests/";
    private static final String BASE_BLINK_STABLE_TEST_PATH =
            BASE_BLINK_TEST_PATH + "virtual/stable/";
    private static final String PATH_WEBVIEW_PREFIX = EXTERNAL_PREFIX + BASE_WEBVIEW_TEST_PATH;
    private static final String PATH_BLINK_PREFIX = EXTERNAL_PREFIX + BASE_BLINK_TEST_PATH;
    private static final String PATH_BLINK_STABLE_PREFIX =
            EXTERNAL_PREFIX + BASE_BLINK_STABLE_TEST_PATH;

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

    @MediumTest
    public void testSimple() throws Exception {
        runWebViewLayoutTest("experimental/basic-logging.html",
                             "experimental/basic-logging-expected.txt");
    }

    // This is a non-failing test because it tends to require frequent rebaselines.
    @MediumTest
    public void testGlobalInterfaceNoFail() throws Exception {
        runBlinkLayoutTest("webexposed/global-interface-listing.html",
                           "webexposed/global-interface-listing-expected.txt", true);
    }

    // This is a non-failing test to avoid 'blind' rebaselines by the sheriff
    // (see crbug.com/564765).
    @MediumTest
    public void testNoUnexpectedInterfaces() throws Exception {
        ensureJsTestCopied();
        loadUrlWebViewAsync("file://" + PATH_BLINK_PREFIX
                + "webexposed/global-interface-listing.html", mTestActivity);
        String webviewExpected = readFile(PATH_WEBVIEW_PREFIX
                + "webexposed/global-interface-listing-expected.txt");
        mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        String result = mTestActivity.getTestResult();

        HashMap<String, HashSet<String>> webviewInterfacesMap = buildHashMap(result);
        HashMap<String, HashSet<String>> webviewExpectedInterfacesMap =
                buildHashMap(webviewExpected);
        StringBuilder newInterfaces = new StringBuilder();

        // Check that each current webview interface is one of webview expected interfaces.
        for (String interfaceS : webviewInterfacesMap.keySet()) {
            if (webviewExpectedInterfacesMap.get(interfaceS) == null) {
                newInterfaces.append(interfaceS + "\n");
            }
        }

        if (newInterfaces.length() > 0) {
            Log.w(TAG, "Unexpected WebView interfaces found: " + newInterfaces.toString());
        }
    }

    @MediumTest
    public void testWebViewExcludedInterfaces() throws Exception {
        ensureJsTestCopied();
        loadUrlWebViewAsync("file://" + PATH_BLINK_PREFIX
                + "webexposed/global-interface-listing.html", mTestActivity);
        String blinkExpected = readFile(PATH_BLINK_PREFIX
                + "webexposed/global-interface-listing-expected.txt");
        String webviewExcluded = readFile(PATH_WEBVIEW_PREFIX
                + "webexposed/not-webview-exposed.txt");
        mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        String result = mTestActivity.getTestResult();

        HashMap<String, HashSet<String>> webviewExcludedInterfacesMap =
                buildHashMap(webviewExcluded);
        HashMap<String, HashSet<String>> webviewInterfacesMap = buildHashMap(result);
        HashMap<String, HashSet<String>> blinkInterfacesMap = buildHashMap(blinkExpected);
        StringBuilder unexpected = new StringBuilder();

        // Check that each excluded interface and its properties are present in blinkInterfaceMap
        // but not in webviewInterfacesMap.
        for (HashMap.Entry<String, HashSet<String>> entry :
                webviewExcludedInterfacesMap.entrySet()) {
            String interfaceS = entry.getKey();
            HashSet<String> subsetBlink = blinkInterfacesMap.get(interfaceS);
            assertNotNull("Interface " + interfaceS + " not exposed in blink", subsetBlink);

            HashSet<String> subsetWebView = webviewInterfacesMap.get(interfaceS);
            HashSet<String> subsetExcluded = entry.getValue();
            if (subsetExcluded.isEmpty() && subsetWebView != null) {
                unexpected.append(interfaceS + "\n");
                continue;
            }

            for (String property : subsetExcluded) {
                assertTrue("Interface " + interfaceS + "." + property + " not exposed in blink",
                        subsetBlink.contains(property));
                if (subsetWebView != null && subsetWebView.contains(property)) {
                    unexpected.append(interfaceS + "." + property + "\n");
                }
            }
        }
        assertEquals("Unexpected webview interfaces found", "", unexpected.toString());
    }

    @MediumTest
    public void testWebViewIncludedStableInterfaces() throws Exception {
        ensureJsTestCopied();
        loadUrlWebViewAsync("file://" + PATH_BLINK_PREFIX
                + "webexposed/global-interface-listing.html", mTestActivity);
        String blinkStableExpected = readFile(PATH_BLINK_STABLE_PREFIX
                + "webexposed/global-interface-listing-expected.txt");
        String webviewExcluded = readFile(PATH_WEBVIEW_PREFIX
                + "webexposed/not-webview-exposed.txt");
        mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        String result = mTestActivity.getTestResult();

        HashMap<String, HashSet<String>> webviewExcludedInterfacesMap =
                buildHashMap(webviewExcluded);
        HashMap<String, HashSet<String>> webviewInterfacesMap = buildHashMap(result);
        HashMap<String, HashSet<String>> blinkStableInterfacesMap =
                buildHashMap(blinkStableExpected);
        StringBuilder missing = new StringBuilder();

        // Check that each stable blink interface and its properties are present in webview
        // except the excluded interfaces/properties.
        for (HashMap.Entry<String, HashSet<String>> entry : blinkStableInterfacesMap.entrySet()) {
            String interfaceS = entry.getKey();
            HashSet<String> subsetExcluded = webviewExcludedInterfacesMap.get(interfaceS);
            if (subsetExcluded != null && subsetExcluded.isEmpty()) continue;

            HashSet<String> subsetBlink = entry.getValue();
            HashSet<String> subsetWebView = webviewInterfacesMap.get(interfaceS);

            if (subsetWebView == null) {
                // interface is missing completely
                missing.append(interfaceS + "\n");
                continue;
            }

            for (String propertyBlink : subsetBlink) {
                if (subsetExcluded != null && subsetExcluded.contains(propertyBlink)) continue;
                if (!subsetWebView.contains(propertyBlink)) {
                    missing.append(interfaceS + "." + propertyBlink + "\n");
                }
            }
        }
        assertEquals("Missing webview interfaces found", "", missing.toString());
    }

    @MediumTest
    public void testRequestMIDIAccess() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest("blink-apis/webmidi/requestmidiaccess.html",
                "blink-apis/webmidi/requestmidiaccess-expected.txt");
        mTestActivity.setGrantPermission(false);
    }

    @MediumTest
    public void testRequestMIDIAccessDenyPermission() throws Exception {
        runWebViewLayoutTest("blink-apis/webmidi/requestmidiaccess-permission-denied.html",
                "blink-apis/webmidi/requestmidiaccess-permission-denied-expected.html");
    }

    // Blink platform API tests

    @MediumTest
    public void testGeolocationCallbacks() throws Exception {
        runWebViewLayoutTest("blink-apis/geolocation/geolocation-permission-callbacks.html",
                "blink-apis/geolocation/geolocation-permission-callbacks-expected.txt");
    }

    @MediumTest
    public void testMediaStreamApiDenyPermission() throws Exception {
        runWebViewLayoutTest("blink-apis/webrtc/mediastream-permission-denied-callbacks.html",
                "blink-apis/webrtc/mediastream-permission-denied-callbacks-expected.txt");
    }

    @MediumTest
    public void testMediaStreamApi() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest("blink-apis/webrtc/mediastream-callbacks.html",
                "blink-apis/webrtc/mediastream-callbacks-expected.txt");
        mTestActivity.setGrantPermission(false);
    }

    @MediumTest
    public void testBatteryApi() throws Exception {
        runWebViewLayoutTest("blink-apis/battery-status/battery-callback.html",
                "blink-apis/battery-status/battery-callback-expected.txt");
    }

    @MediumTest
    public void testEMEPermission() throws Exception {
        mTestActivity.setGrantPermission(true);
        runWebViewLayoutTest("blink-apis/eme/eme.html", "blink-apis/eme/eme-expected.txt");
        mTestActivity.setGrantPermission(false);
    }


    // test helper methods

    private void runWebViewLayoutTest(final String fileName, final String fileNameExpected)
            throws Exception {
        runTest(PATH_WEBVIEW_PREFIX + fileName, PATH_WEBVIEW_PREFIX + fileNameExpected, false);
    }

    private void runBlinkLayoutTest(final String fileName, final String fileNameExpected,
            boolean noFail) throws Exception {
        ensureJsTestCopied();
        runTest(PATH_BLINK_PREFIX + fileName, PATH_WEBVIEW_PREFIX + fileNameExpected, noFail);
    }

    private void runTest(final String fileName, final String fileNameExpected, boolean noFail)
            throws FileNotFoundException, IOException, InterruptedException, TimeoutException {
        loadUrlWebViewAsync("file://" + fileName, mTestActivity);

        if (getInstrumentation().isRebaseline()) {
            // this is the rebaseline process
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
            writeFile(fileNameExpected, result, true);
            Log.i(TAG, "file: " + fileNameExpected + " --> rebaselined, length=" + result.length());
        } else {
            String expected = readFile(fileNameExpected);
            mTestActivity.waitForFinish(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            String result = mTestActivity.getTestResult();
            if (noFail && !expected.equals(result)) {
                ComparisonFailure cf = new ComparisonFailure("Unexpected result", expected, result);
                Log.e(TAG, cf.toString());
            } else {
                assertEquals(expected, result);
            }
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

    private static void ensureJsTestCopied() throws IOException {
        File jsTestFile = new File(PATH_BLINK_PREFIX + "resources/js-test.js");
        if (jsTestFile.exists()) return;
        String original = readFile(PATH_WEBVIEW_PREFIX + "resources/js-test.js");
        writeFile(PATH_BLINK_PREFIX + "resources/js-test.js", original, false);
    }

    /**
     * Reads a file and returns it's contents as string.
     */
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

    /**
     * Writes a file with the given fileName and contents. If overwrite is true overwrites any
     * exisiting file with the same file name. If the file does not exist any intermediate
     * required directories are created.
     */
    private static void writeFile(final String fileName, final String contents, boolean overwrite)
            throws FileNotFoundException, IOException {
        File fileOut = new File(fileName);

        if (fileOut.exists() && !overwrite) {
            return;
        }

        String absolutePath = fileOut.getAbsolutePath();
        File filePath = new File(absolutePath.substring(0, absolutePath.lastIndexOf("/")));

        if (!filePath.exists()) {
            if (!filePath.mkdirs())
                throw new IOException("failed to create directories: " + filePath);
        }

        FileOutputStream outputStream = new FileOutputStream(fileOut);
        try {
            outputStream.write(contents.getBytes());
        } finally {
            outputStream.close();
        }
    }

    private HashMap<String, HashSet<String>> buildHashMap(String contents) {
        String[] lineByLine = contents.split("\\n");

        HashSet subset = null;
        HashMap<String, HashSet<String>> interfaces = new HashMap<String, HashSet<String>>();
        for (String line : lineByLine) {
            String s = trimAndRemoveComments(line);
            if (isInterfaceOrGlobalObject(s)) {
                subset = interfaces.get(s);
                if (subset == null) {
                    subset = new HashSet();
                    interfaces.put(s, subset);
                }
            } else if (isInterfaceProperty(s) && subset != null) {
                subset.add(s);
            }
        }
        return interfaces;
    }

    private String trimAndRemoveComments(String line) {
        String s = line.trim();
        int commentIndex = s.indexOf("#"); // remove any potential comments
        return (commentIndex >= 0) ? s.substring(0, commentIndex).trim() : s;
    }

    private boolean isInterfaceOrGlobalObject(String s) {
        return s.startsWith("interface") || s.startsWith("[GLOBAL OBJECT]");
    }

    private boolean isInterfaceProperty(String s) {
        return s.startsWith("getter") || s.startsWith("setter")
                || s.startsWith("method") || s.startsWith("attribute");
    }

}
