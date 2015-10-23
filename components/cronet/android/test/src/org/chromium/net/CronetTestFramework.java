// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.ConditionVariable;
import android.os.Environment;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;
import static junit.framework.Assert.fail;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.net.urlconnection.CronetURLStreamHandlerFactory;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.InputStream;

import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.util.HashMap;

/**
 * Framework for testing Cronet.
 */
@SuppressFBWarnings("URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD")
public class CronetTestFramework {
    private static final String TAG = "CronetTestFramework";

    public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";
    public static final String POST_DATA_KEY = "postData";
    public static final String CONFIG_KEY = "config";
    public static final String CACHE_KEY = "cache";
    public static final String SDCH_KEY = "sdch";

    public static final String LIBRARY_INIT_KEY = "libraryInit";
    /**
      * Skips library initialization.
      */
    public static final String LIBRARY_INIT_SKIP = "skip";

    // Uses disk cache.
    public static final String CACHE_DISK = "disk";

    // Uses disk cache but does not store http data.
    public static final String CACHE_DISK_NO_HTTP = "diskNoHttp";

    // Uses in-memory cache.
    public static final String CACHE_IN_MEMORY = "memory";

    // Enables Sdch.
    public static final String SDCH_ENABLE = "enable";

    /**
      * Initializes Cronet Async API only.
      */
    public static final String LIBRARY_INIT_CRONET_ONLY = "cronetOnly";

    /**
      * Initializes Cronet HttpURLConnection Wrapper API.
      */
    public static final String LIBRARY_INIT_WRAPPER = "wrapperOnly";

    public CronetURLStreamHandlerFactory mStreamHandlerFactory;
    public CronetEngine mCronetEngine;
    HttpUrlRequestFactory mRequestFactory;
    @SuppressFBWarnings("URF_UNREAD_FIELD") HistogramManager mHistogramManager;

    private final String[] mCommandLine;
    private final Context mContext;

    private String mUrl;
    private boolean mLoading = false;
    private int mHttpStatusCode = 0;

    // CronetEngine.Builder used for this activity.
    private CronetEngine.Builder mCronetEngineBuilder;

    private class TestHttpUrlRequestListener implements HttpUrlRequestListener {
        private final ConditionVariable mComplete = new ConditionVariable();

        public TestHttpUrlRequestListener() {}

        @Override
        public void onResponseStarted(HttpUrlRequest request) {
            mHttpStatusCode = request.getHttpStatusCode();
        }

        @Override
        public void onRequestComplete(HttpUrlRequest request) {
            mLoading = false;
            mComplete.open();
        }

        public void blockForComplete() {
            mComplete.block();
        }
    }

    public CronetTestFramework(String appUrl, String[] commandLine, Context context) {
        mCommandLine = commandLine;
        mContext = context;
        prepareTestStorage();

        // Print out extra arguments passed in starting this activity.
        if (commandLine != null) {
            assertEquals(0, commandLine.length % 2);
            for (int i = 0; i < commandLine.length / 2; i++) {
                Log.i(TAG, "Cronet commandLine %s = %s", commandLine[i * 2],
                        commandLine[i * 2 + 1]);
            }
        }

        // Initializes CronetEngine.Builder from commandLine args.
        mCronetEngineBuilder = initializeCronetEngineBuilder();
        Log.i(TAG, "Using Config: " + mCronetEngineBuilder.toString());

        String initString = getCommandLineArg(LIBRARY_INIT_KEY);
        if (LIBRARY_INIT_SKIP.equals(initString)) {
            return;
        }

        mCronetEngine = initCronetEngine();

        if (LIBRARY_INIT_WRAPPER.equals(initString)) {
            mStreamHandlerFactory = new CronetURLStreamHandlerFactory(mCronetEngine);
        }

        mHistogramManager = HistogramManager.createHistogramManager();

        if (LIBRARY_INIT_CRONET_ONLY.equals(initString)) {
            return;
        }

        mRequestFactory = initRequestFactory();
        if (appUrl != null) {
            startWithURL(appUrl);
        }
    }

    /**
     * Prepares the path for the test storage (http cache, QUIC server info).
     */
    private void prepareTestStorage() {
        File storage = new File(getTestStorage());
        if (storage.exists()) {
            assertTrue(recursiveDelete(storage));
        }
        assertTrue(storage.mkdir());
    }

    String getTestStorage() {
        return PathUtils.getDataDirectory(mContext) + "/test_storage";
    }

    @SuppressFBWarnings("NP_NULL_ON_SOME_PATH_FROM_RETURN_VALUE")
    private boolean recursiveDelete(File path) {
        if (path.isDirectory()) {
            for (File c : path.listFiles()) {
                if (!recursiveDelete(c)) {
                    return false;
                }
            }
        }
        return path.delete();
    }

    CronetEngine.Builder getCronetEngineBuilder() {
        return mCronetEngineBuilder;
    }

    private CronetEngine.Builder initializeCronetEngineBuilder() {
        return createCronetEngineBuilder(mContext);
    }

    CronetEngine.Builder createCronetEngineBuilder(Context context) {
        CronetEngine.Builder cronetEngineBuilder = new CronetEngine.Builder(context);
        cronetEngineBuilder.enableHTTP2(true).enableQUIC(true);

        // Override config if it is passed from the launcher.
        String configString = getCommandLineArg(CONFIG_KEY);
        if (configString != null) {
            try {
                cronetEngineBuilder = new CronetEngine.Builder(mContext, configString);
            } catch (org.json.JSONException e) {
                fail("Invalid Config." + e);
                return null;
            }
        }

        String cacheString = getCommandLineArg(CACHE_KEY);
        if (CACHE_DISK.equals(cacheString)) {
            cronetEngineBuilder.setStoragePath(getTestStorage());
            cronetEngineBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        } else if (CACHE_DISK_NO_HTTP.equals(cacheString)) {
            cronetEngineBuilder.setStoragePath(getTestStorage());
            cronetEngineBuilder.enableHttpCache(
                    CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, 1000 * 1024);
        } else if (CACHE_IN_MEMORY.equals(cacheString)) {
            cronetEngineBuilder.enableHttpCache(
                    CronetEngine.Builder.HTTP_CACHE_IN_MEMORY, 100 * 1024);
        }

        String sdchString = getCommandLineArg(SDCH_KEY);
        if (SDCH_ENABLE.equals(sdchString)) {
            cronetEngineBuilder.enableSDCH(true);
        }

        // Setting this here so it isn't overridden on the command line
        cronetEngineBuilder.setLibraryName("cronet_tests");
        return cronetEngineBuilder;
    }

    // Helper function to initialize Cronet engine. Also used in testing.
    public CronetEngine initCronetEngine() {
        return mCronetEngineBuilder.build();
    }

    // Helper function to initialize request factory. Also used in testing.
    public HttpUrlRequestFactory initRequestFactory() {
        return HttpUrlRequestFactory.createFactory(mContext, mCronetEngineBuilder);
    }

    private String getCommandLineArg(String key) {
        if (mCommandLine != null) {
            for (int i = 0; i < mCommandLine.length; ++i) {
                if (mCommandLine[i].equals(key)) {
                    return mCommandLine[++i];
                }
            }
        }
        return null;
    }

    private void applyCommandLineToHttpUrlRequest(HttpUrlRequest request) {
        String postData = getCommandLineArg(POST_DATA_KEY);
        if (postData != null) {
            InputStream dataStream = new ByteArrayInputStream(postData.getBytes());
            ReadableByteChannel dataChannel = Channels.newChannel(dataStream);
            request.setUploadChannel("text/plain", dataChannel, postData.length());
            request.setHttpMethod("POST");
        }
    }

    public void startWithURL(String url) {
        Log.i(TAG, "Cronet started: " + url);
        mUrl = url;
        mLoading = true;

        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = mRequestFactory.createRequest(
                url, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        applyCommandLineToHttpUrlRequest(request);
        request.start();
        listener.blockForComplete();
    }

    public String getUrl() {
        return mUrl;
    }

    public boolean isLoading() {
        return mLoading;
    }

    public int getHttpStatusCode() {
        return mHttpStatusCode;
    }

    public void startNetLog() {
        if (mRequestFactory != null) {
            mRequestFactory.startNetLogToFile(Environment.getExternalStorageDirectory().getPath()
                            + "/cronet_sample_netlog_old_api.json",
                    false);
        }
        if (mCronetEngine != null) {
            mCronetEngine.startNetLogToFile(Environment.getExternalStorageDirectory().getPath()
                            + "/cronet_sample_netlog_new_api.json",
                    false);
        }
    }

    public void stopNetLog() {
        if (mRequestFactory != null) {
            mRequestFactory.stopNetLog();
        }
        if (mCronetEngine != null) {
            mCronetEngine.stopNetLog();
        }
    }
}
