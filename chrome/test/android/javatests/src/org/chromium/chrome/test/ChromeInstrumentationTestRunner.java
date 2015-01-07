// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

import org.apache.http.HttpEntity;
import org.apache.http.HttpException;
import org.apache.http.HttpRequest;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.HttpVersion;
import org.apache.http.RequestLine;
import org.apache.http.StatusLine;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.entity.FileEntity;
import org.apache.http.impl.DefaultHttpServerConnection;
import org.apache.http.message.BasicHttpResponse;
import org.apache.http.message.BasicStatusLine;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.params.CoreProtocolPNames;
import org.apache.http.params.HttpParams;

import org.chromium.base.test.BaseInstrumentationTestRunner;

import java.io.File;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;


/**
 *  An Instrumentation test runner that optionally spawns a test HTTP server.
 *  The server's root directory is the device's external storage directory.
 */
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
        if (mRunTestHttpServer) startTestHttpServer();
        super.onStart();
    }

    private void startTestHttpServer() {
        Object startLock = new Object();
        mHttpServer = new TestHttpServer(Environment.getExternalStorageDirectory(), 8000,
                                         startLock);
        mHttpServerThread = new Thread(mHttpServer);
        mHttpServerThread.start();
        synchronized (startLock) {
            try {
                while (!mHttpServer.hasStarted()) {
                    startLock.wait();
                }
            } catch (InterruptedException e) {
                Log.e(TAG, "Interrupted while starting test http server: " + e.toString());
            }
        }
    }

    private static class TestHttpServer implements Runnable {
        private static final String TAG = "ChromeInstrumentationTestRunner.TestHttpServer";

        private AtomicBoolean mHasStarted;
        private AtomicBoolean mKeepRunning;
        private int mPort;
        private final File mRootDirectory;
        private final String mRootPath;
        private ServerSocket mServerSocket;
        private Object mStartLock;

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

        public TestHttpServer(File rootDirectory, int port, Object startLock) {
            mHasStarted = new AtomicBoolean(false);
            mKeepRunning = new AtomicBoolean(true);
            mPort = port;
            mRootDirectory = rootDirectory;
            mRootPath = mRootDirectory.getAbsolutePath();
            mServerSocket = null;
            mStartLock = startLock;
        }

        @Override
        public void run() {
            synchronized (mStartLock) {
                try {
                    mServerSocket = new ServerSocket(mPort);
                } catch (IOException e) {
                    Log.e(TAG, "Could not open server socket on " + Integer.toString(mPort)
                            + ": " + e.toString());
                    return;
                } finally {
                    mHasStarted.set(true);
                    mStartLock.notify();
                }
            }

            HttpParams httpParams = new BasicHttpParams();
            httpParams.setParameter(CoreProtocolPNames.PROTOCOL_VERSION, HttpVersion.HTTP_1_0);

            while (mKeepRunning.get()) {
                try {
                    Socket sock = mServerSocket.accept();
                    if (!sock.getInetAddress().isLoopbackAddress()) {
                        Log.w(TAG, "Remote connections forbidden.");
                        sock.close();
                        continue;
                    }

                    DefaultHttpServerConnection conn = new DefaultHttpServerConnection();
                    conn.bind(sock, httpParams);

                    HttpRequest request = conn.receiveRequestHeader();
                    HttpResponse response = generateResponse(request);

                    conn.sendResponseHeader(response);
                    conn.sendResponseEntity(response);
                    conn.close();
                    sock.close();
                } catch (IOException e) {
                    Log.e(TAG, "Error while handling incoming connection: " + e.toString());
                } catch (HttpException e) {
                    Log.e(TAG, "Error while handling HTTP request: " + e.toString());
                }
            }
        }

        private HttpResponse generateResponse(HttpRequest request) {
            RequestLine requestLine = request.getRequestLine();

            String requestPath = requestLine.getUri();
            if (requestPath.startsWith(File.separator)) {
                requestPath = requestPath.substring(File.separator.length());
            }
            File requestedFile = new File(mRootDirectory, requestPath);
            String requestedPath = requestedFile.getAbsolutePath();

            int status = HttpStatus.SC_INTERNAL_SERVER_ERROR;
            String reason = "";
            HttpEntity entity = null;
            if (!HttpGet.METHOD_NAME.equals(requestLine.getMethod())) {
                Log.w(TAG, "Client made request using unsupported method: "
                        + requestLine.getMethod());
                status = HttpStatus.SC_METHOD_NOT_ALLOWED;
            } else if (!requestedPath.startsWith(mRootPath)) {
                Log.w(TAG, "Client tried to request something outside of " + mRootPath + ": "
                        + requestedPath);
                status = HttpStatus.SC_FORBIDDEN;
            } else if (!requestedFile.exists()) {
                Log.w(TAG, "Client requested non-existent file: " + requestedPath);
                status = HttpStatus.SC_NOT_FOUND;
            } else if (!requestedFile.isFile()) {
                Log.w(TAG, "Client requested something that isn't a file: " + requestedPath);
                status = HttpStatus.SC_BAD_REQUEST;
                reason = requestLine.getUri() + " is not a file.";
            } else {
                status = HttpStatus.SC_OK;
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
                entity = new FileEntity(requestedFile, contentType);
            }

            StatusLine statusLine = new BasicStatusLine(HttpVersion.HTTP_1_0, status, reason);
            HttpResponse response = new BasicHttpResponse(statusLine);
            if (entity != null) {
                response.setEntity(entity);
            }
            return response;
        }

        public boolean hasStarted() {
            return mHasStarted.get();
        }

        public void stopServer() {
            mKeepRunning.set(false);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mRunTestHttpServer) stopTestHttpServer();
    }

    private void stopTestHttpServer() {
        mHttpServer.stopServer();
        try {
            mHttpServerThread.join();
        } catch (InterruptedException e) {
            Log.w(TAG, "Interrupted while joining for TestHttpServer thread: " + e.toString());
        }
    }



}
