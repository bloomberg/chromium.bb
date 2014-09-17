// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.widget.EditText;
import android.widget.Toast;

import org.chromium.net.ChromiumUrlRequestFactory;
import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestFactory;
import org.chromium.net.HttpUrlRequestFactoryConfig;
import org.chromium.net.HttpUrlRequestListener;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;
import java.util.HashMap;

/**
 * Activity for managing the Cronet Sample.
 */
public class CronetSampleActivity extends Activity {
    private static final String TAG = "CronetSampleActivity";

    public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";

    public static final String POST_DATA_KEY = "postData";
    public static final String CONFIG_KEY = "config";

    ChromiumUrlRequestFactory mChromiumRequestFactory;
    HttpUrlRequestFactory mRequestFactory;

    String mUrl;

    boolean mLoading = false;

    int mHttpStatusCode = 0;

    class SampleHttpUrlRequestListener implements HttpUrlRequestListener {
        public SampleHttpUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(HttpUrlRequest request) {
            Log.i(TAG, "****** Response Started, content length is "
                    + request.getContentLength());
            Log.i(TAG, "*** Headers Are *** " + request.getAllHeaders());
        }

        @Override
        public void onRequestComplete(HttpUrlRequest request) {
            Log.i(TAG, "****** Request Complete, status code is "
                    + getHttpStatusCode());
            Intent intent = new Intent(getApplicationContext(),
                    CronetSampleActivity.class);
            startActivity(intent);
            final String url = request.getUrl();
            final CharSequence text = "Completed " + request.getUrl() + " ("
                    + request.getHttpStatusCode() + ")";
            mHttpStatusCode = request.getHttpStatusCode();
            CronetSampleActivity.this.runOnUiThread(new Runnable() {
                public void run() {
                    mLoading = false;
                    Toast toast = Toast.makeText(getApplicationContext(), text,
                            Toast.LENGTH_SHORT);
                    toast.show();
                    promptForURL(url);
                }
            });
        }
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

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
                return;
            }
        }

        mRequestFactory = HttpUrlRequestFactory.createFactory(
                getApplicationContext(), config);

        mChromiumRequestFactory = new ChromiumUrlRequestFactory(
                getApplicationContext(), config);

        String appUrl = getUrlFromIntent(getIntent());
        if (appUrl == null) {
            promptForURL("https://");
        } else {
            startWithURL(appUrl);
        }
    }

    private void promptForURL(String url) {
        Log.i(TAG, "No URL provided via intent, prompting user...");
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Enter a URL");
        alert.setMessage("Enter a URL");
        final EditText input = new EditText(this);
        input.setText(url);
        alert.setView(input);
        alert.setPositiveButton("Load", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int button) {
                String url = input.getText().toString();
                startWithURL(url);
            }
        });
        alert.show();
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
        HttpUrlRequestListener listener = new SampleHttpUrlRequestListener();
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
        mChromiumRequestFactory.getRequestContext().startNetLogToFile(
                Environment.getExternalStorageDirectory().getPath() +
                        "/cronet_sample_netlog.json");
    }

    public void stopNetLog() {
        mChromiumRequestFactory.getRequestContext().stopNetLog();
    }
}
