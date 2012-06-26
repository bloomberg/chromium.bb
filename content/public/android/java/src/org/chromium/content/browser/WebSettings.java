// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;
import android.os.Message;
import android.webkit.WebSettings.PluginState;
import android.webkit.WebView;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

/**
 * Manages settings state for a ContentView. A WebSettings instance is obtained
 * from ContentView.getSettings(). If ContentView is used in the
 * ContentView.PERSONALITY_VIEW role, all settings are read / write. If ContentView
 * is in the ContentView.PERSONALITY_CHROME role, setting can only be read.
 */
@JNINamespace("content")
public class WebSettings {
    private static final String TAG = "WebSettings";

    // This class must be created on the UI thread. Afterwards, it can be
    // used from any thread. Internally, the class uses a message queue
    // to call native code on the UI thread only.

    private int mNativeWebSettings = 0;

    private ContentView mContentView;

    // When ContentView is used in PERSONALITY_CHROME mode, settings can't
    // be modified through the WebSettings instance.
    private boolean mCanModifySettings;

    // A flag to avoid sending superfluous synchronization messages.
    private boolean mIsSyncMessagePending = false;
    // Custom handler that queues messages to call native code on the UI thread.
    private final EventHandler mEventHandler;

    private static final int MINIMUM_FONT_SIZE = 1;
    private static final int MAXIMUM_FONT_SIZE = 72;

    // Private settings so we don't have to go into native code to
    // retrieve the values. After setXXX, sendSyncMessage() needs to be called.
    //
    // TODO(mnaganov): populate with the complete set of legacy WebView settings.

    private String mStandardFontFamily = "sans-serif";
    private String mFixedFontFamily = "monospace";
    private String mSansSerifFontFamily = "sans-serif";
    private String mSerifFontFamily = "serif";
    private String mCursiveFontFamily = "cursive";
    private String mFantasyFontFamily = "fantasy";
    // FIXME: Should be obtained from Android. Problem: it is hidden.
    private String mDefaultTextEncoding = "Latin-1";
    private String mUserAgent;
    private int mMinimumFontSize = 8;
    private int mMinimumLogicalFontSize = 8;
    private int mDefaultFontSize = 16;
    private int mDefaultFixedFontSize = 13;
    private boolean mLoadsImagesAutomatically = true;
    private boolean mJavaScriptEnabled = false;
    private boolean mJavaScriptCanOpenWindowsAutomatically = false;
    private PluginState mPluginState = PluginState.OFF;

    // Not accessed by the native side.
    private String mDefaultUserAgent = "";
    private boolean mSupportZoom = true;
    private boolean mBuiltInZoomControls = false;
    private boolean mDisplayZoomControls = true;

    // Class to handle messages to be processed on the UI thread.
    private class EventHandler {
        // Message id for syncing
        private static final int SYNC = 0;
        // Message id for updating user agent in the view
        private static final int UPDATE_UA = 1;
        // Message id for updating multi-touch zoom state in the view
        private static final int UPDATE_MULTI_TOUCH = 2;
        // Actual UI thread handler
        private Handler mHandler;

        EventHandler() {
            mHandler = mContentView.isPersonalityView() ?
                    new Handler() {
                        @Override
                        public void handleMessage(Message msg) {
                            switch (msg.what) {
                                case SYNC:
                                    synchronized (WebSettings.this) {
                                        nativeSyncToNative(mNativeWebSettings);
                                        mIsSyncMessagePending = false;
                                    }
                                    break;
                                case UPDATE_UA:
                                    synchronized (mContentView) {
                                        mContentView.setAllUserAgentOverridesInHistory();
                                    }
                                    break;
                                case UPDATE_MULTI_TOUCH:
                                    synchronized (mContentView) {
                                        mContentView.updateMultiTouchZoomSupport();
                                    }
                                    break;
                            }
                        }
                    } :
                    new Handler() {
                        @Override
                        public void handleMessage(Message msg) {
                            switch (msg.what) {
                                case SYNC:
                                    synchronized (WebSettings.this) {
                                        nativeSyncFromNative(mNativeWebSettings);
                                        mIsSyncMessagePending = false;
                                    }
                                    break;
                            }
                        }
                    };
        }

        private synchronized void sendSyncMessage() {
            mHandler.sendMessage(Message.obtain(null, SYNC));
        }

        private synchronized void sendUpdateUaMessage() {
            mHandler.sendMessage(Message.obtain(null, UPDATE_UA));
        }

        private synchronized void sendUpdateMultiTouchMessage() {
            mHandler.sendMessage(Message.obtain(null, UPDATE_MULTI_TOUCH));
        }
    }

    /**
     * Package constructor to prevent clients from creating a new settings
     * instance. Must be called on the UI thread.
     */
    WebSettings(ContentView contentView, int nativeContentView) {
        ThreadUtils.assertOnUiThread();
        mContentView = contentView;
        mCanModifySettings = mContentView.isPersonalityView();
        mNativeWebSettings = nativeInit(nativeContentView, mCanModifySettings);
        assert mNativeWebSettings != 0;

        mEventHandler = new EventHandler();
        if (mCanModifySettings) {
            // PERSONALITY_VIEW
            mDefaultUserAgent = nativeGetDefaultUserAgent();
            mUserAgent = mDefaultUserAgent;
            nativeSyncToNative(mNativeWebSettings);
        } else {
            // PERSONALITY_CHROME
            // Chrome has zooming enabled by default. These settings are not
            // set by the native code.
            mBuiltInZoomControls = true;
            mDisplayZoomControls = false;
            nativeSyncFromNative(mNativeWebSettings);
        }
    }

    /**
     * Destroys the native side of the WebSettings. This WebSettings object
     * cannot be used after this method has been called. Should only be called
     * when related ContentView is destroyed.
     */
    void destroy() {
        nativeDestroy(mNativeWebSettings);
        mNativeWebSettings = 0;
    }

    /**
     * Set the WebView's user-agent string. If the string "ua" is null or empty,
     * it will use the system default user-agent string.
     */
    public synchronized void setUserAgentString(String ua) {
        assert mCanModifySettings;
        final String oldUserAgent = mUserAgent;
        if (ua == null || ua.length() == 0) {
            mUserAgent = mDefaultUserAgent;
        } else {
            mUserAgent = ua;
        }
        if (!oldUserAgent.equals(mUserAgent)) {
            mEventHandler.sendUpdateUaMessage();
        }
    }

    /**
     * Gets the WebView's user-agent string.
     */
    public synchronized String getUserAgentString() {
        // TODO(mnaganov): Doesn't reflect changes made by ChromeNativePreferences.
        return mUserAgent;
    }

    /**
     * Sets whether the WebView should support zooming using its on-screen zoom
     * controls and gestures. The particular zoom mechanisms that should be used
     * can be set with {@link #setBuiltInZoomControls}. This setting does not
     * affect zooming performed using the {@link WebView#zoomIn()} and
     * {@link WebView#zoomOut()} methods. The default is true.
     *
     * @param support whether the WebView should support zoom
     */
    public void setSupportZoom(boolean support) {
        mSupportZoom = support;
        mEventHandler.sendUpdateMultiTouchMessage();
    }

    /**
     * Gets whether the WebView supports zoom.
     *
     * @return true if the WebView supports zoom
     * @see #setSupportZoom
     */
    public boolean supportZoom() {
        return mSupportZoom;
    }

   /**
     * Sets whether the WebView should use its built-in zoom mechanisms. The
     * built-in zoom mechanisms comprise on-screen zoom controls, which are
     * displayed over the WebView's content, and the use of a pinch gesture to
     * control zooming. Whether or not these on-screen controls are displayed
     * can be set with {@link #setDisplayZoomControls}. The default is false,
     * due to compatibility reasons.
     * <p>
     * The built-in mechanisms are the only currently supported zoom
     * mechanisms, so it is recommended that this setting is always enabled.
     * In other words, there is no point of calling this method other than
     * with the 'true' parameter.
     *
     * @param enabled whether the WebView should use its built-in zoom mechanisms
     */
     public void setBuiltInZoomControls(boolean enabled) {
        mBuiltInZoomControls = enabled;
        mEventHandler.sendUpdateMultiTouchMessage();
    }

    /**
     * Gets whether the zoom mechanisms built into WebView are being used.
     *
     * @return true if the zoom mechanisms built into WebView are being used
     * @see #setBuiltInZoomControls
     */
    public boolean getBuiltInZoomControls() {
        return mBuiltInZoomControls;
    }

    /**
     * Sets whether the WebView should display on-screen zoom controls when
     * using the built-in zoom mechanisms. See {@link #setBuiltInZoomControls}.
     * The default is true.
     *
     * @param enabled whether the WebView should display on-screen zoom controls
     */
    public void setDisplayZoomControls(boolean enabled) {
        mDisplayZoomControls = enabled;
        mEventHandler.sendUpdateMultiTouchMessage();
    }

    /**
     * Gets whether the WebView displays on-screen zoom controls when using
     * the built-in zoom mechanisms.
     *
     * @return true if the WebView displays on-screen zoom controls when using
     *         the built-in zoom mechanisms
     * @see #setDisplayZoomControls
     */
    public boolean getDisplayZoomControls() {
        return mDisplayZoomControls;
    }

    boolean supportsMultiTouchZoom() {
        return mSupportZoom && mBuiltInZoomControls;
    }

    boolean shouldDisplayZoomControls() {
        return supportsMultiTouchZoom() && mDisplayZoomControls;
    }

    /**
     * Set the standard font family name.
     * @param font A font family name.
     */
    public synchronized void setStandardFontFamily(String font) {
        assert mCanModifySettings;
        if (!mStandardFontFamily.equals(font)) {
            mStandardFontFamily = font;
            sendSyncMessage();
        }
    }

    /**
     * Get the standard font family name. The default is "sans-serif".
     * @return The standard font family name as a string.
     */
    public synchronized String getStandardFontFamily() {
        return mStandardFontFamily;
    }

    /**
     * Set the fixed font family name.
     * @param font A font family name.
     */
    public synchronized void setFixedFontFamily(String font) {
        assert mCanModifySettings;
        if (!mFixedFontFamily.equals(font)) {
            mFixedFontFamily = font;
            sendSyncMessage();
        }
    }

    /**
     * Get the fixed font family name. The default is "monospace".
     * @return The fixed font family name as a string.
     */
    public synchronized String getFixedFontFamily() {
        return mFixedFontFamily;
    }

    /**
     * Set the sans-serif font family name.
     * @param font A font family name.
     */
    public synchronized void setSansSerifFontFamily(String font) {
        assert mCanModifySettings;
        if (!mSansSerifFontFamily.equals(font)) {
            mSansSerifFontFamily = font;
            sendSyncMessage();
        }
    }

    /**
     * Get the sans-serif font family name.
     * @return The sans-serif font family name as a string.
     */
    public synchronized String getSansSerifFontFamily() {
        return mSansSerifFontFamily;
    }

    /**
     * Set the serif font family name. The default is "sans-serif".
     * @param font A font family name.
     */
    public synchronized void setSerifFontFamily(String font) {
        assert mCanModifySettings;
        if (!mSerifFontFamily.equals(font)) {
            mSerifFontFamily = font;
            sendSyncMessage();
        }
    }

    /**
     * Get the serif font family name. The default is "serif".
     * @return The serif font family name as a string.
     */
    public synchronized String getSerifFontFamily() {
        return mSerifFontFamily;
    }

    /**
     * Set the cursive font family name.
     * @param font A font family name.
     */
    public synchronized void setCursiveFontFamily(String font) {
        assert mCanModifySettings;
        if (!mCursiveFontFamily.equals(font)) {
            mCursiveFontFamily = font;
            sendSyncMessage();
        }
    }

    /**
     * Get the cursive font family name. The default is "cursive".
     * @return The cursive font family name as a string.
     */
    public synchronized String getCursiveFontFamily() {
        return mCursiveFontFamily;
    }

    /**
     * Set the fantasy font family name.
     * @param font A font family name.
     */
    public synchronized void setFantasyFontFamily(String font) {
        assert mCanModifySettings;
        if (!mFantasyFontFamily.equals(font)) {
            mFantasyFontFamily = font;
            sendSyncMessage();
        }
    }

    /**
     * Get the fantasy font family name. The default is "fantasy".
     * @return The fantasy font family name as a string.
     */
    public synchronized String getFantasyFontFamily() {
        return mFantasyFontFamily;
    }

    /**
     * Set the minimum font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public synchronized void setMinimumFontSize(int size) {
        assert mCanModifySettings;
        size = clipFontSize(size);
        if (mMinimumFontSize != size) {
            mMinimumFontSize = size;
            sendSyncMessage();
        }
    }

    /**
     * Get the minimum font size. The default is 8.
     * @return A non-negative integer between 1 and 72.
     */
    public synchronized int getMinimumFontSize() {
        return mMinimumFontSize;
    }

    /**
     * Set the minimum logical font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public synchronized void setMinimumLogicalFontSize(int size) {
        assert mCanModifySettings;
        size = clipFontSize(size);
        if (mMinimumLogicalFontSize != size) {
            mMinimumLogicalFontSize = size;
            sendSyncMessage();
        }
    }

    /**
     * Get the minimum logical font size. The default is 8.
     * @return A non-negative integer between 1 and 72.
     */
    public synchronized int getMinimumLogicalFontSize() {
        return mMinimumLogicalFontSize;
    }

    /**
     * Set the default font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public synchronized void setDefaultFontSize(int size) {
        assert mCanModifySettings;
        size = clipFontSize(size);
        if (mDefaultFontSize != size) {
            mDefaultFontSize = size;
            sendSyncMessage();
        }
    }

    /**
     * Get the default font size. The default is 16.
     * @return A non-negative integer between 1 and 72.
     */
    public synchronized int getDefaultFontSize() {
        return mDefaultFontSize;
    }

    /**
     * Set the default fixed font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public synchronized void setDefaultFixedFontSize(int size) {
        assert mCanModifySettings;
        size = clipFontSize(size);
        if (mDefaultFixedFontSize != size) {
            mDefaultFixedFontSize = size;
            sendSyncMessage();
        }
    }

    /**
     * Get the default fixed font size. The default is 16.
     * @return A non-negative integer between 1 and 72.
     */
    public synchronized int getDefaultFixedFontSize() {
        return mDefaultFixedFontSize;
    }

    /**
     * Tell the WebView to enable JavaScript execution.
     *
     * @param flag True if the WebView should execute JavaScript.
     */
    public synchronized void setJavaScriptEnabled(boolean flag) {
        assert mCanModifySettings;
        if (mJavaScriptEnabled != flag) {
            mJavaScriptEnabled = flag;
            sendSyncMessage();
        }
    }

    /**
     * Tell the WebView to load image resources automatically.
     * @param flag True if the WebView should load images automatically.
     */
    public synchronized void setLoadsImagesAutomatically(boolean flag) {
        assert mCanModifySettings;
        if (mLoadsImagesAutomatically != flag) {
            mLoadsImagesAutomatically = flag;
            sendSyncMessage();
        }
    }

    /**
     * Return true if the WebView will load image resources automatically.
     * The default is true.
     * @return True if the WebView loads images automatically.
     */
    public synchronized boolean getLoadsImagesAutomatically() {
        return mLoadsImagesAutomatically;
    }

    /**
     * Return true if JavaScript is enabled. <b>Note: The default is false.</b>
     *
     * @return True if JavaScript is enabled.
     */
    public synchronized boolean getJavaScriptEnabled() {
        return mJavaScriptEnabled;
    }

    /**
     * Tell the WebView to enable plugins.
     * @param flag True if the WebView should load plugins.
     * @deprecated This method has been deprecated in favor of
     *             {@link #setPluginState}
     */
    @Deprecated
    public synchronized void setPluginsEnabled(boolean flag) {
        assert mCanModifySettings;
        setPluginState(flag ? PluginState.ON : PluginState.OFF);
    }

    /**
     * Tell the WebView to enable, disable, or have plugins on demand. On
     * demand mode means that if a plugin exists that can handle the embedded
     * content, a placeholder icon will be shown instead of the plugin. When
     * the placeholder is clicked, the plugin will be enabled.
     * @param state One of the PluginState values.
     */
    public synchronized void setPluginState(PluginState state) {
        assert mCanModifySettings;
        if (mPluginState != state) {
            mPluginState = state;
            sendSyncMessage();
        }
    }

    /**
     * Return true if plugins are enabled.
     * @return True if plugins are enabled.
     * @deprecated This method has been replaced by {@link #getPluginState}
     */
    @Deprecated
    public synchronized boolean getPluginsEnabled() {
        return mPluginState == PluginState.ON;
    }

    /**
     * Return true if plugins are disabled.
     * @return True if plugins are disabled.
     * @hide
     */
    @CalledByNative
    private synchronized boolean getPluginsDisabled() {
        return mPluginState == PluginState.OFF;
    }

    /**
     * Sets if plugins are disabled.
     * @return True if plugins are disabled.
     * @hide
     */
    @CalledByNative
    private synchronized void setPluginsDisabled(boolean disabled) {
        mPluginState = disabled ? PluginState.OFF : PluginState.ON;
    }

    /**
     * Return the current plugin state.
     * @return A value corresponding to the enum PluginState.
     */
    public synchronized PluginState getPluginState() {
        return mPluginState;
    }


    /**
     * Tell javascript to open windows automatically. This applies to the
     * javascript function window.open().
     * @param flag True if javascript can open windows automatically.
     */
    public synchronized void setJavaScriptCanOpenWindowsAutomatically(boolean flag) {
        assert mCanModifySettings;
        if (mJavaScriptCanOpenWindowsAutomatically != flag) {
            mJavaScriptCanOpenWindowsAutomatically = flag;
            sendSyncMessage();
        }
    }

    /**
     * Return true if javascript can open windows automatically. The default
     * is false.
     * @return True if javascript can open windows automatically during
     *         window.open().
     */
    public synchronized boolean getJavaScriptCanOpenWindowsAutomatically() {
        return mJavaScriptCanOpenWindowsAutomatically;
    }

    /**
     * Set the default text encoding name to use when decoding html pages.
     * @param encoding The text encoding name.
     */
    public synchronized void setDefaultTextEncodingName(String encoding) {
        assert mCanModifySettings;
        if (!mDefaultTextEncoding.equals(encoding)) {
            mDefaultTextEncoding = encoding;
            sendSyncMessage();
        }
    }

    /**
     * Get the default text encoding name. The default is "Latin-1".
     * @return The default text encoding name as a string.
     */
    public synchronized String getDefaultTextEncodingName() {
        return mDefaultTextEncoding;
    }

    private int clipFontSize(int size) {
        if (size < MINIMUM_FONT_SIZE) {
            return MINIMUM_FONT_SIZE;
        } else if (size > MAXIMUM_FONT_SIZE) {
            return MAXIMUM_FONT_SIZE;
        }
        return size;
    }

    // Send a synchronization message to handle syncing the native settings.
    // When the ContentView is in the PERSONALITY_VIEW role, called from setter
    // methods.  When the ContentView is in the PERSONALITY_CHROME role, called
    // from ContentView on Chrome preferences updates.
    synchronized void sendSyncMessage() {
        // Only post if a sync is not pending
        if (!mIsSyncMessagePending) {
            mIsSyncMessagePending = true;
            mEventHandler.sendSyncMessage();
        }
    }

    // Initialize the WebSettings native side.
    private native int nativeInit(int contentViewPtr, boolean isMasterMode);

    private native void nativeDestroy(int nativeWebSettings);

    private static native String nativeGetDefaultUserAgent();

    // Synchronize Java settings from native settings.
    private native void nativeSyncFromNative(int nativeWebSettings);

    // Synchronize native settings from Java settings.
    private native void nativeSyncToNative(int nativeWebSettings);
}
