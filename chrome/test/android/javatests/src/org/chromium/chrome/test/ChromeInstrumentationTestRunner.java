// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import junit.framework.TestCase;

import org.chromium.base.test.BaseInstrumentationTestRunner;
import org.chromium.base.test.BaseTestResult;
import org.chromium.base.test.util.SkipCheck;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.net.test.BaseHttpTestServer;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.net.Socket;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 *  An Instrumentation test runner that optionally spawns a test HTTP server.
 *  The server's root directory is the device's external storage directory.
 *
 *  TODO(jbudorick): remove uses of deprecated org.apache.* crbug.com/488192
 */
@SuppressWarnings("deprecation")
public class ChromeInstrumentationTestRunner extends BaseInstrumentationTestRunner {

    private static final String TAG = "ChromeInstrumentationTestRunner";

    public static final String EXTRA_DISABLE_TEST_HTTP_SERVER =
            "org.chromium.chrome.test.ChromeInstrumentationTestRunner.DisableTestHttpServer";
    public static final String EXTRA_ENABLE_TEST_HTTP_SERVER =
            "org.chromium.chrome.test.ChromeInstrumentationTestRunner.EnableTestHttpServer";

    // TODO(jbudorick): Default this to true once the bots are all enabling the server via the
    // flag.
    private static final boolean DEFAULT_ENABLE_TEST_HTTP_SERVER = false;

    private Thread mHttpServerThread;
    private TestHttpServer mHttpServer;
    private boolean mRunTestHttpServer;

    @Override
    public void onCreate(Bundle arguments) {
        if (arguments.containsKey(EXTRA_DISABLE_TEST_HTTP_SERVER)) {
            mRunTestHttpServer = false;
        } else if (arguments.containsKey(EXTRA_ENABLE_TEST_HTTP_SERVER)) {
            mRunTestHttpServer = true;
        } else {
            mRunTestHttpServer = DEFAULT_ENABLE_TEST_HTTP_SERVER;
        }

        // InstrumentationTestRunner.onCreate() calls start() at the end, so we call it last.
        super.onCreate(arguments);
    }

    @Override
    public void onStart() {
        if (mRunTestHttpServer) {
            try {
                startTestHttpServer();
            } catch (IOException e) {
                Log.e(TAG, "Failed to start HTTP test server", e);
                finish(Activity.RESULT_CANCELED, null);
            }
        }
        super.onStart();
    }

    private void startTestHttpServer() throws IOException {
        mHttpServer = new TestHttpServer(Environment.getExternalStorageDirectory(), 8000);
        mHttpServerThread = new Thread(mHttpServer);
        mHttpServerThread.start();
        mHttpServer.waitForServerToStart();
    }

    private static class TestHttpServer extends BaseHttpTestServer {
        private static final String TAG = "ChromeInstrumentationTestRunner.TestHttpServer";

        private final File mRootDirectory;
        private final String mRootPath;

        private static final int ACCEPT_TIMEOUT_MS = 5000;
        private static final String DEFAULT_CONTENT_TYPE = "application/octet-stream";
        private static final Map<String, String> EXTENSION_CONTENT_TYPE_MAP;
        static {
            Map<String, String> m = new HashMap<String, String>();
            m.put(".conf", "text/plain");
            m.put(".css", "text/css");
            m.put(".dtd", "text/xml");
            m.put(".gif", "image/gif");
            m.put(".htm", "text/html");
            m.put(".html", "text/html");
            m.put(".jpeg", "image/jpeg");
            m.put(".jpg", "image/jpeg");
            m.put(".js", "application/x-javascript");
            m.put(".log", "text/plain");
            m.put(".manifest", "text/cache-manifest");
            m.put(".mp4", "video/mp4");
            m.put(".png", "image/png");
            m.put(".svg", "image/svg+xml");
            m.put(".text", "text/plain");
            m.put(".txt", "text/plain");
            m.put(".xhtml", "application/xhtml+xml");
            m.put(".xhtmlmp", "application/vnd.wap.xhtml+xml");
            m.put(".xml", "text/xml");
            EXTENSION_CONTENT_TYPE_MAP = Collections.unmodifiableMap(m);
        }

        public TestHttpServer(File rootDirectory, int port) throws IOException {
            super(port, ACCEPT_TIMEOUT_MS);
            mRootDirectory = rootDirectory;
            mRootPath = mRootDirectory.getAbsolutePath();
        }

        @Override
        protected boolean validateSocket(Socket sock) {
            return sock.getInetAddress().isLoopbackAddress();
        }

        @Override
        protected org.apache.http.params.HttpParams getConnectionParams() {
            org.apache.http.params.HttpParams httpParams =
                    new org.apache.http.params.BasicHttpParams();
            httpParams.setParameter(org.apache.http.params.CoreProtocolPNames.PROTOCOL_VERSION,
                    org.apache.http.HttpVersion.HTTP_1_0);
            return httpParams;
        }

        @Override
        protected void handleGet(org.apache.http.HttpRequest request, HttpResponseCallback callback)
                throws org.apache.http.HttpException {
            org.apache.http.RequestLine requestLine = request.getRequestLine();

            String requestPath = requestLine.getUri();
            if (requestPath.startsWith(File.separator)) {
                requestPath = requestPath.substring(File.separator.length());
            }
            File requestedFile = new File(mRootDirectory, requestPath);
            String requestedPath = requestedFile.getAbsolutePath();

            int status = org.apache.http.HttpStatus.SC_INTERNAL_SERVER_ERROR;
            String reason = "";
            org.apache.http.HttpEntity entity = null;
            if (!requestedPath.startsWith(mRootPath)) {
                Log.w(TAG, "Client tried to request something outside of " + mRootPath + ": "
                        + requestedPath);
                status = org.apache.http.HttpStatus.SC_FORBIDDEN;
            } else if (!requestedFile.exists()) {
                Log.w(TAG, "Client requested non-existent file: " + requestedPath);
                status = org.apache.http.HttpStatus.SC_NOT_FOUND;
            } else if (!requestedFile.isFile()) {
                Log.w(TAG, "Client requested something that isn't a file: " + requestedPath);
                status = org.apache.http.HttpStatus.SC_BAD_REQUEST;
                reason = requestLine.getUri() + " is not a file.";
            } else {
                status = org.apache.http.HttpStatus.SC_OK;
                String contentType = null;
                int extensionIndex = requestedPath.lastIndexOf('.');
                if (extensionIndex == -1) {
                    contentType = DEFAULT_CONTENT_TYPE;
                } else {
                    String extension = requestedPath.substring(extensionIndex);
                    contentType = EXTENSION_CONTENT_TYPE_MAP.get(extension);
                    if (contentType == null) {
                        Log.w(TAG, "Unrecognized extension: " + extension);
                        contentType = DEFAULT_CONTENT_TYPE;
                    }
                }
                entity = new org.apache.http.entity.FileEntity(requestedFile, contentType);
            }

            org.apache.http.StatusLine statusLine = new org.apache.http.message.BasicStatusLine(
                    org.apache.http.HttpVersion.HTTP_1_0, status, reason);
            org.apache.http.HttpResponse response =
                    new org.apache.http.message.BasicHttpResponse(statusLine);
            if (entity != null) {
                response.setEntity(entity);
            }
            callback.onResponse(response);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mRunTestHttpServer) stopTestHttpServer();
    }

    private void stopTestHttpServer() {
        mHttpServer.stop();
        try {
            mHttpServerThread.join();
        } catch (InterruptedException e) {
            Log.w(TAG, "Interrupted while joining for TestHttpServer thread: " + e.toString());
        }
    }

    @Override
    protected void addTestHooks(BaseTestResult result) {
        super.addTestHooks(result);
        result.addSkipCheck(new DisableInTabbedModeSkipCheck());
        result.addSkipCheck(new ChromeRestrictionSkipCheck());

        result.addPreTestHook(Policies.getRegistrationHook());
    }

    private class ChromeRestrictionSkipCheck extends RestrictionSkipCheck {

        @Override
        protected boolean restrictionApplies(String restriction) {
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_PHONE)
                    && DeviceFormFactor.isTablet(getTargetContext())) {
                return true;
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_TABLET)
                    && !DeviceFormFactor.isTablet(getTargetContext())) {
                return true;
            }
            if (TextUtils.equals(restriction,
                                 ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
                    && (ConnectionResult.SUCCESS != GoogleApiAvailability.getInstance()
                            .isGooglePlayServicesAvailable(getTargetContext()))) {
                return true;
            }
            return false;
        }
    }

    /**
     * Checks for tests that should only run in document mode.
     */
    private class DisableInTabbedModeSkipCheck extends SkipCheck {

        /**
         * If the test is running in tabbed mode, checks for
         * {@link org.chromium.chrome.test.util.DisableInTabbedMode}.
         *
         * @param testCase The test to check.
         * @return Whether the test is running in tabbed mode and has been marked as disabled in
         *      tabbed mode.
         */
        @Override
        public boolean shouldSkip(TestCase testCase) {
            Class<?> testClass = testCase.getClass();
            try {
                if (!FeatureUtilities.isDocumentMode(getContext())) {
                    Method testMethod = testClass.getMethod(testCase.getName());
                    if (testMethod.isAnnotationPresent(DisableInTabbedMode.class)
                            || testClass.isAnnotationPresent(DisableInTabbedMode.class)) {
                        Log.i(TAG, "Test " + testClass.getName() + "#" + testCase.getName()
                                + " is disabled in non-document mode.");
                        return true;
                    }
                }
            } catch (NoSuchMethodException e) {
                Log.e(TAG, "Couldn't find test method: " + e.toString());
            }

            return false;
        }
    }
}
