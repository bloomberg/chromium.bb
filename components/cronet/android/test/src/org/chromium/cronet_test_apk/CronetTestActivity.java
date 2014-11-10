// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

import org.chromium.base.PathUtils;
import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestFactory;
import org.chromium.net.HttpUrlRequestFactoryConfig;
import org.chromium.net.HttpUrlRequestListener;
import org.chromium.net.UrlRequestContext;

import java.io.ByteArrayInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

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
    public static final String SKIP_FACTORY_INIT_KEY = "skipFactoryInit";

    HttpUrlRequestFactory mRequestFactory;
    UrlRequestContext mUrlRequestContext;

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

        if (!loadTestFiles()) {
            Log.e(TAG, "Loading test files failed");
            return;
        }

        try {
            System.loadLibrary("cronet_tests");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "libcronet_test initialization failed.", e);
            finish();
            return;
        }

        String skipInitString = getCommandLineArg(SKIP_FACTORY_INIT_KEY);
        if (skipInitString != null) {
            return;
        }
        mRequestFactory = initRequestFactory();
        if (mRequestFactory == null) {
            return;
        }

        String appUrl = getUrlFromIntent(getIntent());
        if (appUrl != null) {
            startWithURL(appUrl);
        }
    }

    // Helper function to initialize request factory. Also used in testing.
    public HttpUrlRequestFactory initRequestFactory() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableHttpCache(HttpUrlRequestFactoryConfig.HttpCache.IN_MEMORY,
                               100 * 1024)
              .enableSPDY(true)
              .enableQUIC(true);

        // Override config if it is passed from the launcher.
        String configString = getCommandLineArg(CONFIG_KEY);
        if (configString != null) {
            try {
                Log.i(TAG, "Using Config: " + configString);
                config = new HttpUrlRequestFactoryConfig(configString);
            } catch (org.json.JSONException e) {
                Log.e(TAG, "Invalid Config.", e);
                finish();
                return null;
            }
        }

        // Setting this here so it isn't overridden on the command line
        config.setLibraryName("cronet_tests");

        mUrlRequestContext = UrlRequestContext.createContext(
                getApplicationContext(), config);

        return HttpUrlRequestFactory.createFactory(getApplicationContext(),
                                                   config);
    }

    private boolean loadTestFiles() {
        String testFilePath = "test";
        String toPath = PathUtils.getDataDirectory(getApplicationContext());
        AssetManager assetManager = getAssets();
        try {
            String[] files = assetManager.list(testFilePath);
            Log.i(TAG, "Begin loading " + files.length + " test files.");
            for (String file : files) {
                Log.i(TAG, "Loading " + file);
                if (!copyTestFile(assetManager,
                                  testFilePath + "/" + file,
                                  toPath + "/" + file)) {
                    return false;
                }
            }
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    // Helper function to copy a file to a destination.
    private static boolean copyTestFile(AssetManager assetManager,
                                        String testFile,
                                        String testFileCopy) {
        OutputStream out;
        try {
            out = new FileOutputStream(testFileCopy);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        try {
            InputStream in = assetManager.open(testFile);

            byte[] buffer = new byte[1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            in.close();
            out.flush();
            out.close();
            return true;
        } catch (Exception e) {
            try {
                out.close();
            } catch (Exception closeException) {
                closeException.printStackTrace();
            }
            e.printStackTrace();
            return false;
        }
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
