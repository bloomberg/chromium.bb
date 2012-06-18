// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.util.Log;
import android.webkit.DownloadListener;
import android.widget.FrameLayout;

import org.chromium.content.browser.AndroidBrowserProcess;

public class ContentView extends FrameLayout {
    private static final String TAG = "ContentView";

    /**
     * Automatically decide the number of renderer processes to use based on device memory class.
     * */
    public static final int MAX_RENDERERS_AUTOMATIC = -1;


    // content_view_client.cc depends on ContentView.java holding a ref to the current client
    // instance since the native side only holds a weak pointer to the client. We chose this
    // solution over the managed object owning the C++ object's memory since it's a lot simpler
    // in terms of clean up.
    private ContentViewClient mContentViewClient;

    // Native pointer to C++ ContentView object which will be set by nativeInit()
    private int mNativeContentView = 0;

    // The legacy webview DownloadListener.
    private DownloadListener mDownloadListener;
    // ContentViewDownloadDelegate adds support for authenticated downloads
    // and POST downloads. Embedders should prefer ContentViewDownloadDelegate
    // over DownloadListener.
    private ContentViewDownloadDelegate mDownloadDelegate;

    public void setContentViewClient(ContentViewClient client) {
        if (client == null) {
            throw new IllegalArgumentException("The client can't be null.");
        }
        mContentViewClient = client;
        if (mNativeContentView != 0) {

            // TODO(jrg): upstream this chain.  nativeSetClient(),
            // ContentView::SetClient(), add a content_view_client_,
            // add web_contents_, pass web_contents into native ContentView ctor, ...
            /* nativeSetClient(mNativeContentView, mContentViewClient); */
        }
    }

    ContentViewClient getContentViewClient() {
        if (mContentViewClient == null) {
            // We use the Null Object pattern to avoid having to perform a null check in this class.
            // We create it lazily because most of the time a client will be set almost immediately
            // after ChromeView is created.
            mContentViewClient = new ContentViewClient();
            // We don't set the native ContentViewClient pointer here on purpose. The native
            // implementation doesn't mind a null delegate and using one is better than passing a
            // Null Object, since we cut down on the number of JNI calls.
        }
        return mContentViewClient;
    }

    /**
     * Enable multi-process ContentView. This should be called by the application before
     * constructing any ContentView instances. If enabled, ContentView will run renderers in
     * separate processes up to the number of processes specified by maxRenderProcesses. If this is
     * not called then the default is to run the renderer in the main application on a separate
     * thread.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses Limit on the number of renderers to use. Each tab runs in its own
     * process until the maximum number of processes is reached. The special value of
     * MAX_RENDERERS_SINGLE_PROCESS requests single-process mode where the renderer will run in the
     * application process in a separate thread. If the special value MAX_RENDERERS_AUTOMATIC is
     * used then the number of renderers will be determined based on the device memory class. The
     * maximum number of allowed renderers is capped by MAX_RENDERERS_LIMIT.
     */
    public static void enableMultiProcess(Context context, int maxRendererProcesses) {
        AndroidBrowserProcess.initContentViewProcess(context, maxRendererProcesses);
    }

    /**
     * Registers the drawable to be used for overlaying the popup zoomer contents.  The drawable
     * should be transparent in the middle to allow the zoomed content to show.
     *
     * @param id The id of the drawable to be used to overlay the popup zoomer contents.
     */
    public static void registerPopupOverlayResourceId(int id) {
        // TODO(tedchoc): Implement.
    }

    /**
     * Sets how much to round the corners of the popup contents.
     * @param r The radius of the rounded corners of the popup overlay drawable.
     */
    public static void registerPopupOverlayCornerRadius(float r) {
        // TODO(tedchoc): Implement.
    }

    public ContentView(Context context) {
        super(context, null);
        initialize(context);
    }

    // TODO(jrg): incomplete; upstream the rest of this method.
    private void initialize(Context context) {
        mNativeContentView = nativeInit();
        Log.i(TAG, "mNativeContentView=0x"+ Integer.toHexString(mNativeContentView));
    }

    /**
     * Destroy the internal state of the WebView. This method may only be called
     * after the WebView has been removed from the view system. No other methods
     * may be called on this WebView after this method has been called.
     */
    // TODO(jrg): incomplete; upstream the rest of this method.
    public void destroy() {
        if (mNativeContentView != 0) {
            nativeDestroy(mNativeContentView);
            mNativeContentView = 0;
        }
    }


    /**
     * Load url without fixing up the url string. Calls from Chrome should be not
     * be using this, but should use Tab.loadUrl instead.
     * @param url The url to load.
     */
    public void loadUrlWithoutUrlSanitization(String url) {
        // TODO(tedchoc): Implement.
    }

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    public String getUrl() {
        // TODO(tedchoc): Implement.
        return null;
    }

    /**
     * @return Whether the current WebContents has a previous navigation entry.
     */
    public boolean canGoBack() {
        // TODO(tedchoc): Implement.
        return false;
    }

    /**
     * @return Whether the current WebContents has a navigation entry after the current one.
     */
    public boolean canGoForward() {
        // TODO(tedchoc): Implement.
        return false;
    }

    /**
     * Goes to the navigation entry before the current one.
     */
    public void goBack() {
        // TODO(tedchoc): Implement.
    }

    /**
     * Goes to the navigation entry following the current one.
     */
    public void goForward() {
        // TODO(tedchoc): Implement.
    }

    /**
     * Reload the current page.
     */
    public void reload() {
        // TODO(tedchoc): Implement.
    }

    /**
     * Register the listener to be used when content can not be handled by the
     * rendering engine, and should be downloaded instead. This will replace the
     * current listener.
     * @param listener An implementation of DownloadListener.
     */
    // TODO(nileshagrawal): decide if setDownloadDelegate will be public API. If so,
    // this method should be deprecated and the javadoc should make reference to the
    // fact that a ContentViewDownloadDelegate will be used in preference to a
    // DownloadListener.
    public void setDownloadListener(DownloadListener listener) {
        mDownloadListener = listener;
    }

    // Called by DownloadController.
    DownloadListener downloadListener() {
        return mDownloadListener;
    }

    /**
     * Register the delegate to be used when content can not be handled by
     * the rendering engine, and should be downloaded instead. This will replace
     * the current delegate or existing DownloadListner.
     * Embedders should prefer this over the legacy DownloadListener.
     * @param listener An implementation of ContentViewDownloadDelegate.
     */
    public void setDownloadDelegate(ContentViewDownloadDelegate delegate) {
        mDownloadDelegate = delegate;
    }

    // Called by DownloadController.
    ContentViewDownloadDelegate getDownloadDelegate() {
        return mDownloadDelegate;
    }

    /**
     * Initialize the ContentView native side.
     * Should be called with a valid native WebContents.
     * If nativeInitProcess is never called, the first time this method is called, nativeInitProcess
     * will be called implicitly with the default settings.
     * @param hardwareAccelerated if true, the View uses hardware accelerated rendering.
     * @param nativeWebContents the ContentView does not create a new native WebContents and uses
     *         the provided one.
     * @return a native pointer to the native ContentView object.
     */
    private native int nativeInit();

    private native void nativeDestroy(int nativeContentView);

}
