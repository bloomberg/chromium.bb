// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.SparseArray;

import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnKeyListener;
import android.view.inputmethod.InputMethodManager;

import android.webkit.GeolocationPermissions;
import android.webkit.PermissionRequest;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import android.widget.EditText;
import android.widget.LinearLayout.LayoutParams;
import android.widget.PopupMenu;
import android.widget.TextView;

import org.chromium.base.Log;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import java.net.URI;
import java.net.URISyntaxException;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This activity is designed for starting a "mini-browser" for manual testing of WebView.
 * It takes an optional URL as an argument, and displays the page. There is a URL bar
 * on top of the webview for manually specifying URLs to load.
 */
public class WebViewBrowserActivity extends Activity implements PopupMenu.OnMenuItemClickListener {
    private static final String TAG = "WebViewShell";

    // Our imaginary Android permission to associate with the WebKit geo permission
    private static final String RESOURCE_GEO = "RESOURCE_GEO";
    // Our imaginary WebKit permission to request when loading a file:// URL
    private static final String RESOURCE_FILE_URL = "RESOURCE_FILE_URL";
    // WebKit permissions with no corresponding Android permission can always be granted
    private static final String NO_ANDROID_PERMISSION = "NO_ANDROID_PERMISSION";

    // Map from WebKit permissions to Android permissions
    private static final HashMap<String, String> sPermissions;
    static {
        sPermissions = new HashMap<String, String>();
        sPermissions.put(RESOURCE_GEO, Manifest.permission.ACCESS_FINE_LOCATION);
        sPermissions.put(RESOURCE_FILE_URL, Manifest.permission.READ_EXTERNAL_STORAGE);
        sPermissions.put(PermissionRequest.RESOURCE_AUDIO_CAPTURE,
                Manifest.permission.RECORD_AUDIO);
        sPermissions.put(PermissionRequest.RESOURCE_MIDI_SYSEX, NO_ANDROID_PERMISSION);
        sPermissions.put(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID, NO_ANDROID_PERMISSION);
        sPermissions.put(PermissionRequest.RESOURCE_VIDEO_CAPTURE,
                Manifest.permission.CAMERA);
    }

    private static final Pattern WEBVIEW_VERSION_PATTERN =
            Pattern.compile("(Chrome/)([\\d\\.]+)\\s");

    private EditText mUrlBar;
    private WebView mWebView;
    private String mWebViewVersion;

    // Each time we make a request, store it here with an int key. onRequestPermissionsResult will
    // look up the request in order to grant the approprate permissions.
    private SparseArray<PermissionRequest> mPendingRequests = new SparseArray<PermissionRequest>();
    private int mNextRequestKey = 0;

    // Work around our wonky API by wrapping a geo permission prompt inside a regular
    // PermissionRequest.
    private static class GeoPermissionRequest extends PermissionRequest {
        private String mOrigin;
        private GeolocationPermissions.Callback mCallback;

        public GeoPermissionRequest(String origin, GeolocationPermissions.Callback callback) {
            mOrigin = origin;
            mCallback = callback;
        }

        public Uri getOrigin() {
            return Uri.parse(mOrigin);
        }

        public String[] getResources() {
            return new String[] { WebViewBrowserActivity.RESOURCE_GEO };
        }

        public void grant(String[] resources) {
            assert resources.length == 1;
            assert WebViewBrowserActivity.RESOURCE_GEO.equals(resources[0]);
            mCallback.invoke(mOrigin, true, false);
        }

        public void deny() {
            mCallback.invoke(mOrigin, false, false);
        }
    }

    // For simplicity, also treat the read access needed for file:// URLs as a regular
    // PermissionRequest.
    private class FilePermissionRequest extends PermissionRequest {
        private String mOrigin;

        public FilePermissionRequest(String origin) {
            mOrigin = origin;
        }

        public Uri getOrigin() {
            return Uri.parse(mOrigin);
        }

        public String[] getResources() {
            return new String[] { WebViewBrowserActivity.RESOURCE_FILE_URL };
        }

        public void grant(String[] resources) {
            assert resources.length == 1;
            assert WebViewBrowserActivity.RESOURCE_FILE_URL.equals(resources[0]);
            // Try again now that we have read access.
            WebViewBrowserActivity.this.mWebView.loadUrl(mOrigin);
        }

        public void deny() {
            // womp womp
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            WebView.setWebContentsDebuggingEnabled(true);
        }
        setContentView(R.layout.activity_webview_browser);
        mWebView = (WebView) findViewById(R.id.webview);
        WebSettings settings = mWebView.getSettings();
        initializeSettings(settings);

        Matcher matcher = WEBVIEW_VERSION_PATTERN.matcher(settings.getUserAgentString());
        if (matcher.find()) {
            mWebViewVersion = matcher.group(2);
        } else {
            mWebViewVersion = "-";
        }
        setTitle(getResources().getString(R.string.title_activity_browser) + " " + mWebViewVersion);

        mWebView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                return false;
            }

            @Override
            public void onReceivedError(WebView view, int errorCode, String description,
                    String failingUrl) {
                setUrlFail(true);
            }
        });

        mWebView.setWebChromeClient(new WebChromeClient() {
            @Override
            public Bitmap getDefaultVideoPoster() {
                return Bitmap.createBitmap(
                        new int[] {Color.TRANSPARENT}, 1, 1, Bitmap.Config.ARGB_8888);
            }

            @Override
            public void onGeolocationPermissionsShowPrompt(String origin,
                    GeolocationPermissions.Callback callback) {
                onPermissionRequest(new GeoPermissionRequest(origin, callback));
            }

            @Override
            public void onPermissionRequest(PermissionRequest request) {
                WebViewBrowserActivity.this.requestPermissionsForPage(request);
            }
        });

        mUrlBar = (EditText) findViewById(R.id.url_field);
        mUrlBar.setOnKeyListener(new OnKeyListener() {
            public boolean onKey(View view, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_UP) {
                    loadUrlFromUrlBar(view);
                    return true;
                }
                return false;
            }
        });

        String url = getUrlFromIntent(getIntent());
        if (url != null) {
            setUrlBarText(url);
            setUrlFail(false);
            loadUrlFromUrlBar(mUrlBar);
        }
    }

    // WebKit permissions which can be granted because either they have no associated Android
    // permission or the associated Android permission has been granted
    private boolean canGrant(String webkitPermission) {
        String androidPermission = sPermissions.get(webkitPermission);
        if (androidPermission == NO_ANDROID_PERMISSION) {
            return true;
        }
        return PackageManager.PERMISSION_GRANTED == checkSelfPermission(androidPermission);
    }

    private void requestPermissionsForPage(PermissionRequest request) {
        // Deny any unrecognized permissions.
        for (String webkitPermission : request.getResources()) {
            if (!sPermissions.containsKey(webkitPermission)) {
                Log.w(TAG, "Unrecognized WebKit permission: " + webkitPermission);
                request.deny();
                return;
            }
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            request.grant(request.getResources());
            return;
        }

        // Find what Android permissions we need before we can grant these WebKit permissions.
        ArrayList<String> androidPermissionsNeeded = new ArrayList<String>();
        for (String webkitPermission : request.getResources()) {
            if (!canGrant(webkitPermission)) {
                // We already checked for unrecognized permissions, and canGrant will skip over
                // NO_ANDROID_PERMISSION cases, so this is guaranteed to be a regular Android
                // permission.
                String androidPermission = sPermissions.get(webkitPermission);
                androidPermissionsNeeded.add(androidPermission);
            }
        }

        // If there are no such Android permissions, grant the WebKit permissions immediately.
        if (androidPermissionsNeeded.isEmpty()) {
            request.grant(request.getResources());
            return;
        }

        // Otherwise, file a new request
        if (mNextRequestKey == Integer.MAX_VALUE) {
            Log.e(TAG, "Too many permission requests");
            return;
        }
        int requestCode = mNextRequestKey;
        mNextRequestKey++;
        mPendingRequests.append(requestCode, request);
        requestPermissions(androidPermissionsNeeded.toArray(new String[0]), requestCode);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
            String permissions[], int[] grantResults) {
        // Verify that we can now grant all the requested permissions. Note that although grant()
        // takes a list of permissions, grant() is actually all-or-nothing. If there are any
        // requested permissions not included in the granted permissions, all will be denied.
        PermissionRequest request = mPendingRequests.get(requestCode);
        for (String webkitPermission : request.getResources()) {
            if (!canGrant(webkitPermission)) {
                request.deny();
                return;
            }
        }
        request.grant(request.getResources());
        mPendingRequests.delete(requestCode);
    }

    public void loadUrlFromUrlBar(View view) {
        String url = mUrlBar.getText().toString();
        try {
            URI uri = new URI(url);
            url = (uri.getScheme() == null) ? "http://" + uri.toString() : uri.toString();
        } catch (URISyntaxException e) {
            String message = "<html><body>URISyntaxException: " + e.getMessage() + "</body></html>";
            mWebView.loadData(message, "text/html", "UTF-8");
            setUrlFail(true);
            return;
        }

        setUrlBarText(url);
        setUrlFail(false);
        loadUrl(url);
        hideKeyboard(mUrlBar);
    }

    public void showPopup(View v) {
        PopupMenu popup = new PopupMenu(this, v);
        popup.setOnMenuItemClickListener(this);
        popup.inflate(R.menu.main_menu);
        popup.show();
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        switch(item.getItemId()) {
            case R.id.menu_about:
                about();
                hideKeyboard(mUrlBar);
                return true;
            default:
                return false;
        }
    }

    private void initializeSettings(WebSettings settings) {
        settings.setJavaScriptEnabled(true);

        // configure local storage apis and their database paths.
        settings.setAppCachePath(getDir("appcache", 0).getPath());
        settings.setGeolocationDatabasePath(getDir("geolocation", 0).getPath());
        settings.setDatabasePath(getDir("databases", 0).getPath());

        settings.setAppCacheEnabled(true);
        settings.setGeolocationEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setDomStorageEnabled(true);
    }

    private void about() {
        WebSettings settings = mWebView.getSettings();
        StringBuilder summary = new StringBuilder();
        summary.append("WebView version : " + mWebViewVersion + "\n");

        for (Method method : settings.getClass().getMethods()) {
            if (!methodIsSimpleInspector(method)) continue;
            try {
                summary.append(method.getName() + " : " + method.invoke(settings) + "\n");
            } catch (IllegalAccessException e) {
            } catch (InvocationTargetException e) { }
        }

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(getResources().getString(R.string.menu_about))
                .setMessage(summary)
                .setPositiveButton("OK", null)
                .create();
        dialog.show();
        dialog.getWindow().setLayout(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT);
    }

    // Returns true is a method has no arguments and returns either a boolean or a String.
    private boolean methodIsSimpleInspector(Method method) {
        Class<?> returnType = method.getReturnType();
        return ((returnType.equals(boolean.class) || returnType.equals(String.class))
                && method.getParameterTypes().length == 0);
    }

    private void loadUrl(String url) {
        // Request read access if necessary
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && "file".equals(Uri.parse(url).getScheme())
                && PackageManager.PERMISSION_DENIED
                        == checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)) {
            requestPermissionsForPage(new FilePermissionRequest(url));
        }

        // If it is file:// and we don't have permission, they'll get the "Webpage not available"
        // "net::ERR_ACCESS_DENIED" page. When we get permission, FilePermissionRequest.grant()
        // will reload.
        mWebView.loadUrl(url);
        mWebView.requestFocus();
    }

    private void setUrlBarText(String url) {
        mUrlBar.setText(url, TextView.BufferType.EDITABLE);
    }

    private void setUrlFail(boolean fail) {
        mUrlBar.setTextColor(fail ? Color.RED : Color.BLACK);
    }

    /**
     * Hides the keyboard.
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    private static boolean hideKeyboard(View view) {
        InputMethodManager imm = (InputMethodManager) view.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
