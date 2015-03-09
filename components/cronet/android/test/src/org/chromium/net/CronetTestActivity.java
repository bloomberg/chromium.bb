// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

import org.chromium.net.urlconnection.CronetURLStreamHandlerFactory;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.util.HashMap;

/**
 * Activity for managing the Cronet Test.
 */
public class CronetTestActivity extends Activity {
    private static final String TAG = "CronetTestActivity";

    public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";
    public static final String POST_DATA_KEY = "postData";
    public static final String CONFIG_KEY = "config";
    public static final String LIBRARY_INIT_KEY = "libraryInit";
    /**
      * Skip library initialization.
      */
    public static final String LIBRARY_INIT_SKIP = "skip";

    /**
      * Initialize Cronet Async API only.
      */
    public static final String LIBRARY_INIT_CRONET_ONLY = "cronetOnly";

    public CronetURLStreamHandlerFactory mStreamHandlerFactory;
    public UrlRequestContext mUrlRequestContext;
    HttpUrlRequestFactory mRequestFactory;
    HistogramManager mHistogramManager;

    String mUrl;

    boolean mLoading = false;

    int mHttpStatusCode = 0;

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

        String initString = getCommandLineArg(LIBRARY_INIT_KEY);
        if (LIBRARY_INIT_SKIP.equals(initString)) {
            return;
        }

        mUrlRequestContext = initRequestContext();
        mStreamHandlerFactory = new CronetURLStreamHandlerFactory(this, getContextConfig());
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

    UrlRequestContextConfig getContextConfig() {
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.enableHttpCache(UrlRequestContextConfig.HttpCache.IN_MEMORY,
                               100 * 1024)
              .enableSPDY(true)
              .enableQUIC(true);

        // Override config if it is passed from the launcher.
        String configString = getCommandLineArg(CONFIG_KEY);
        if (configString != null) {
            try {
                Log.i(TAG, "Using Config: " + configString);
                config = new UrlRequestContextConfig(configString);
            } catch (org.json.JSONException e) {
                Log.e(TAG, "Invalid Config.", e);
                finish();
                return null;
            }
        }

        // Setting this here so it isn't overridden on the command line
        config.setLibraryName("cronet_tests");
        return config;
    }

    // Helper function to initialize request context. Also used in testing.
    public UrlRequestContext initRequestContext() {
        return UrlRequestContext.createContext(this, getContextConfig());
    }

    // Helper function to initialize request factory. Also used in testing.
    public HttpUrlRequestFactory initRequestFactory() {
        return HttpUrlRequestFactory.createFactory(this, getContextConfig());
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    private String getCommandLineArg(String key) {
        Intent intent = getIntent();
        Bundle extras = intent.getExtras();
        Log.i(TAG, "Cronet extras: " + extras);
        if (extras != null) {
            String[] commandLine = extras.getStringArray(COMMAND_LINE_ARGS_KEY);
            if (commandLine != null) {
                for (int i = 0; i < commandLine.length; ++i) {
                    Log.i(TAG,
                            "Cronet commandLine[" + i + "]=" + commandLine[i]);
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
        mRequestFactory.startNetLogToFile(
                Environment.getExternalStorageDirectory().getPath()
                        + "/cronet_sample_netlog.json");
    }

    public void stopNetLog() {
        mRequestFactory.stopNetLog();
    }
}
