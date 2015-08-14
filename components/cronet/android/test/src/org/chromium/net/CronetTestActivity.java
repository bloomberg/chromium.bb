// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

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
 * Activity for managing the Cronet Test.
 */
@SuppressFBWarnings("URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD")
public class CronetTestActivity extends Activity {
    private static final String TAG = "CronetTestActivity";

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
    public UrlRequestContext mUrlRequestContext;
    HttpUrlRequestFactory mRequestFactory;
    @SuppressFBWarnings("URF_UNREAD_FIELD")
    HistogramManager mHistogramManager;

    String mUrl;

    boolean mLoading = false;

    int mHttpStatusCode = 0;

    // UrlRequestContextConfig used for this activity.
    private UrlRequestContextConfig mConfig;

    class TestHttpUrlRequestListener implements HttpUrlRequestListener {
        public TestHttpUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(HttpUrlRequest request) {
            mHttpStatusCode = request.getHttpStatusCode();
        }

        @Override
        public void onRequestComplete(HttpUrlRequest request) {
            mLoading = false;
        }
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prepareTestStorage();

        // Print out extra arguments passed in starting this activity.
        Intent intent = getIntent();
        Bundle extras = intent.getExtras();
        Log.i(TAG, "Cronet extras: " + extras);
        if (extras != null) {
            String[] commandLine = extras.getStringArray(COMMAND_LINE_ARGS_KEY);
            if (commandLine != null) {
                assertEquals(0, commandLine.length % 2);
                for (int i = 0; i < commandLine.length / 2; i++) {
                    Log.i(TAG, "Cronet commandLine %s = %s", commandLine[i * 2],
                            commandLine[i * 2 + 1]);
                }
            }
        }

        // Initializes UrlRequestContextConfig from commandLine args.
        mConfig = initializeContextConfig();
        Log.i(TAG, "Using Config: " + mConfig.toString());

        String initString = getCommandLineArg(LIBRARY_INIT_KEY);
        if (LIBRARY_INIT_SKIP.equals(initString)) {
            return;
        }

        if (LIBRARY_INIT_WRAPPER.equals(initString)) {
            mStreamHandlerFactory =
                new CronetURLStreamHandlerFactory(this, mConfig);
        }

        mUrlRequestContext = initRequestContext();
        mHistogramManager = HistogramManager.createHistogramManager();

        if (LIBRARY_INIT_CRONET_ONLY.equals(initString)) {
            return;
        }

        mRequestFactory = initRequestFactory();
        String appUrl = getUrlFromIntent(getIntent());
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
        return PathUtils.getDataDirectory(getApplicationContext()) + "/test_storage";
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

    UrlRequestContextConfig getContextConfig() {
        return mConfig;
    }

    private UrlRequestContextConfig initializeContextConfig() {
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.enableHTTP2(true).enableQUIC(true);

        // Override config if it is passed from the launcher.
        String configString = getCommandLineArg(CONFIG_KEY);
        if (configString != null) {
            try {
                config = new UrlRequestContextConfig(configString);
            } catch (org.json.JSONException e) {
                Log.e(TAG, "Invalid Config.", e);
                finish();
                return null;
            }
        }

        String cacheString = getCommandLineArg(CACHE_KEY);
        if (CACHE_DISK.equals(cacheString)) {
            config.setStoragePath(getTestStorage());
            config.enableHttpCache(UrlRequestContextConfig.HTTP_CACHE_DISK, 1000 * 1024);
        } else if (CACHE_DISK_NO_HTTP.equals(cacheString)) {
            config.setStoragePath(getTestStorage());
            config.enableHttpCache(UrlRequestContextConfig.HTTP_CACHE_DISK_NO_HTTP, 1000 * 1024);
        } else if (CACHE_IN_MEMORY.equals(cacheString)) {
            config.enableHttpCache(UrlRequestContextConfig.HTTP_CACHE_IN_MEMORY, 100 * 1024);
        }

        String sdchString = getCommandLineArg(SDCH_KEY);
        if (SDCH_ENABLE.equals(sdchString)) {
            config.enableSDCH(true);
        }

        // Setting this here so it isn't overridden on the command line
        config.setLibraryName("cronet_tests");
        return config;
    }

    // Helper function to initialize request context. Also used in testing.
    public UrlRequestContext initRequestContext() {
        return UrlRequestContext.createContext(this, mConfig);
    }

    // Helper function to initialize request factory. Also used in testing.
    public HttpUrlRequestFactory initRequestFactory() {
        return HttpUrlRequestFactory.createFactory(this, mConfig);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    private String getCommandLineArg(String key) {
        Intent intent = getIntent();
        Bundle extras = intent.getExtras();
        if (extras != null) {
            String[] commandLine = extras.getStringArray(COMMAND_LINE_ARGS_KEY);
            if (commandLine != null) {
                for (int i = 0; i < commandLine.length; ++i) {
                    if (commandLine[i].equals(key)) {
                        return commandLine[++i];
                    }
                }
            }
        }
        return null;
    }

    private void applyCommandLineToHttpUrlRequest(HttpUrlRequest request) {
        String postData = getCommandLineArg(POST_DATA_KEY);
        if (postData != null) {
            InputStream dataStream = new ByteArrayInputStream(
                    postData.getBytes());
            ReadableByteChannel dataChannel = Channels.newChannel(dataStream);
            request.setUploadChannel("text/plain", dataChannel,
                    postData.length());
            request.setHttpMethod("POST");
        }
    }

    public void startWithURL(String url) {
        Log.i(TAG, "Cronet started: " + url);
        mUrl = url;
        mLoading = true;

        HashMap<String, String> headers = new HashMap<String, String>();
        HttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = mRequestFactory.createRequest(
                url, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        applyCommandLineToHttpUrlRequest(request);
        request.start();
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
        if (mUrlRequestContext != null) {
            mUrlRequestContext.startNetLogToFile(Environment.getExternalStorageDirectory().getPath()
                    + "/cronet_sample_netlog_new_api.json",
                    false);
        }
    }

    public void stopNetLog() {
        if (mRequestFactory != null) {
            mRequestFactory.stopNetLog();
        }
        if (mUrlRequestContext != null) {
            mUrlRequestContext.stopNetLog();
        }
    }
}
