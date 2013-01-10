// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.webkit.WebSettings.PluginState;
import android.webkit.WebView;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

/**
 * Manages settings state for a ContentView. A ContentSettings instance is obtained
 * from ContentView.getContentSettings(). If ContentView is used in the
 * ContentView.PERSONALITY_VIEW role, all settings are read / write. If ContentView
 * is in the ContentView.PERSONALITY_CHROME role, setting can only be read.
 */
@JNINamespace("content")
public class ContentSettings {
    // This enum corresponds to WebSettings.LayoutAlgorithm. We use our own to be
    // able to extend it.
    public enum LayoutAlgorithm {
        NORMAL,
        SINGLE_COLUMN,
        NARROW_COLUMNS,
        TEXT_AUTOSIZING,
    }

    private static final String TAG = "ContentSettings";

    // This class must be created on the UI thread. Afterwards, it can be
    // used from any thread. Internally, the class uses a message queue
    // to call native code on the UI thread only.

    // The native side of this object. Ownership is retained native-side by the WebContents
    // instance that backs the associated ContentViewCore.
    private int mNativeContentSettings = 0;

    private ContentViewCore mContentViewCore;

    // When ContentView is used in PERSONALITY_CHROME mode, settings can't
    // be modified through the ContentSettings instance.
    private boolean mCanModifySettings;

    // A flag to avoid sending superfluous synchronization messages.
    private boolean mIsSyncMessagePending = false;
    // Custom handler that queues messages to call native code on the UI thread.
    private final EventHandler mEventHandler;

    // Protects access to settings fields.
    private final Object mContentSettingsLock = new Object();

    private static final int MINIMUM_FONT_SIZE = 1;
    private static final int MAXIMUM_FONT_SIZE = 72;

    // Private settings so we don't have to go into native code to
    // retrieve the values. After setXXX, mEventHandler.syncSettingsLocked() needs to be called.
    //
    // TODO(mnaganov): populate with the complete set of legacy WebView settings.
    // Note: If adding a new setting to this class, make sure to add it to the initFrom()
    // method defined below.

    private LayoutAlgorithm mLayoutAlgorithm = LayoutAlgorithm.NARROW_COLUMNS;
    private int mTextSizePercent = 100;
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
    private boolean mImagesEnabled = true;
    private boolean mJavaScriptEnabled = false;
    private boolean mAllowUniversalAccessFromFileURLs = false;
    private boolean mAllowFileAccessFromFileURLs = false;
    private boolean mJavaScriptCanOpenWindowsAutomatically = false;
    private boolean mSupportMultipleWindows = false;
    private PluginState mPluginState = PluginState.OFF;
    private boolean mAppCacheEnabled = false;
    private boolean mDomStorageEnabled = false;
    private boolean mUseWideViewport = false;

    // Not accessed by the native side.
    private boolean mSupportZoom = true;
    private boolean mBuiltInZoomControls = false;
    private boolean mDisplayZoomControls = true;
    static class LazyDefaultUserAgent {
        // Lazy Holder pattern
        private static final String sInstance = nativeGetDefaultUserAgent();
    }

    // Protects access to settings global fields.
    private static final Object sGlobalContentSettingsLock = new Object();
    // For compatibility with the legacy WebView, we can only enable AppCache when the path is
    // provided. However, we don't use the path, so we just check if we have received it from the
    // client.
    private static boolean sAppCachePathIsSet = false;

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
            mHandler = mContentViewCore.isPersonalityView() ?
                    new Handler(Looper.getMainLooper()) {
                        @Override
                        public void handleMessage(Message msg) {
                            switch (msg.what) {
                                case SYNC:
                                    synchronized (mContentSettingsLock) {
                                        syncToNativeOnUiThread();
                                        mIsSyncMessagePending = false;
                                        mContentSettingsLock.notifyAll();
                                    }
                                    break;
                                case UPDATE_UA:
                                    if (mContentViewCore.isAlive()) {
                                        mContentViewCore.setAllUserAgentOverridesInHistory();
                                    }
                                    break;
                                case UPDATE_MULTI_TOUCH:
                                    if (mContentViewCore.isAlive()) {
                                        mContentViewCore.updateMultiTouchZoomSupport();
                                    }
                                    break;
                            }
                        }
                    } :
                    new Handler(Looper.getMainLooper()) {
                        @Override
                        public void handleMessage(Message msg) {
                            switch (msg.what) {
                                case SYNC:
                                    synchronized (mContentSettingsLock) {
                                        syncFromNativeOnUiThread();
                                        mIsSyncMessagePending = false;
                                    }
                                    break;
                            }
                        }
                    };
        }

        private void syncSettingsLocked() {
            assert Thread.holdsLock(mContentSettingsLock);
            if (mNativeContentSettings == 0) return;
            if (mContentViewCore.isPersonalityView()) {
                if (Looper.myLooper() == mHandler.getLooper()) {
                    syncToNativeOnUiThread();
                } else {
                    // We're being called on a background thread, so post a message.
                    if (mIsSyncMessagePending) {
                        return;
                    }
                    mIsSyncMessagePending = true;
                    mHandler.sendMessage(Message.obtain(null, SYNC));
                    // When used in PERSONALITY_VIEW mode, we must block
                    // until the settings have been sync'd to native to
                    // ensure that they have taken effect.
                    try {
                        while (mIsSyncMessagePending) {
                            mContentSettingsLock.wait();
                        }
                    } catch (InterruptedException e) {}
                }
            } else {
                if (mIsSyncMessagePending) {
                    return;
                }
                mIsSyncMessagePending = true;
                mHandler.sendMessage(Message.obtain(null, SYNC));
            }
        }

        private void sendUpdateUaMessageLocked() {
            assert Thread.holdsLock(mContentSettingsLock);
            if (mNativeContentSettings == 0) return;
            mHandler.sendMessage(Message.obtain(null, UPDATE_UA));
        }

        private void sendUpdateMultiTouchMessageLocked() {
            assert Thread.holdsLock(mContentSettingsLock);
            if (mNativeContentSettings == 0) return;
            mHandler.sendMessage(Message.obtain(null, UPDATE_MULTI_TOUCH));
        }
    }

    /**
     * Package constructor to prevent clients from creating a new settings
     * instance. Must be called on the UI thread.
     */
    ContentSettings(ContentViewCore contentViewCore, int nativeContentView,
            boolean isAccessFromFileURLsGrantedByDefault) {
        ThreadUtils.assertOnUiThread();
        mContentViewCore = contentViewCore;
        mCanModifySettings = mContentViewCore.isPersonalityView();
        mNativeContentSettings = nativeInit(nativeContentView, mCanModifySettings);
        assert mNativeContentSettings != 0;

        if (isAccessFromFileURLsGrantedByDefault) {
            mAllowUniversalAccessFromFileURLs = true;
            mAllowFileAccessFromFileURLs = true;
        }

        mEventHandler = new EventHandler();
        if (mCanModifySettings) {
            // PERSONALITY_VIEW
            mUserAgent = LazyDefaultUserAgent.sInstance;
            syncToNativeOnUiThread();
        } else {
            // PERSONALITY_CHROME
            // Chrome has zooming enabled by default. These settings are not
            // set by the native code.
            mBuiltInZoomControls = true;
            mDisplayZoomControls = false;
            syncFromNativeOnUiThread();
        }
    }

    /**
     * Notification from the native side that it is being destroyed.
     * @param nativeContentSettings the native instance that is going away.
     */
    @CalledByNative
    private void onNativeContentSettingsDestroyed(int nativeContentSettings) {
        assert mNativeContentSettings == nativeContentSettings;
        mNativeContentSettings = 0;
    }

    /**
     * @returns the default User-Agent used by each ContentViewCore instance, i.e. unless
     * overridden by {@link #setUserAgentString()}
     */
    public static String getDefaultUserAgent() {
        return LazyDefaultUserAgent.sInstance;
    }

    /**
     * Set the WebView's user-agent string. If the string "ua" is null or empty,
     * it will use the system default user-agent string.
     */
    public void setUserAgentString(String ua) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            final String oldUserAgent = mUserAgent;
            if (ua == null || ua.length() == 0) {
                mUserAgent = LazyDefaultUserAgent.sInstance;
            } else {
                mUserAgent = ua;
            }
            if (!oldUserAgent.equals(mUserAgent)) {
                mEventHandler.sendUpdateUaMessageLocked();
            }
        }
    }

    /**
     * Gets the WebView's user-agent string.
     */
    public String getUserAgentString() {
        // TODO(mnaganov): Doesn't reflect changes made by ChromeNativePreferences.
        synchronized (mContentSettingsLock) {
            return mUserAgent;
        }
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
        synchronized (mContentSettingsLock) {
            mSupportZoom = support;
            mEventHandler.sendUpdateMultiTouchMessageLocked();
        }
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
        synchronized (mContentSettingsLock) {
            mBuiltInZoomControls = enabled;
            mEventHandler.sendUpdateMultiTouchMessageLocked();
        }
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
        synchronized (mContentSettingsLock) {
            mDisplayZoomControls = enabled;
            mEventHandler.sendUpdateMultiTouchMessageLocked();
        }
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
     * Sets the text zoom of the page in percent. Default is 100.
     *
     * @param textZoom the percent value for increasing or decreasing the text
     */
    public void setTextZoom(int textZoom) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mTextSizePercent != textZoom) {
                mTextSizePercent = textZoom;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Gets the text zoom of the page in percent.
     *
     * @return a percent value describing the text zoom
     * @see #setTextSizeZoom
     */
    public int getTextZoom() {
        synchronized (mContentSettingsLock) {
            return mTextSizePercent;
        }
    }

    /**
     * Set the standard font family name.
     * @param font A font family name.
     */
    public void setStandardFontFamily(String font) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (font != null && !mStandardFontFamily.equals(font)) {
                mStandardFontFamily = font;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the standard font family name. The default is "sans-serif".
     * @return The standard font family name as a string.
     */
    public String getStandardFontFamily() {
        synchronized (mContentSettingsLock) {
            return mStandardFontFamily;
        }
    }

    /**
     * Set the fixed font family name.
     * @param font A font family name.
     */
    public void setFixedFontFamily(String font) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (font != null && !mFixedFontFamily.equals(font)) {
                mFixedFontFamily = font;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the fixed font family name. The default is "monospace".
     * @return The fixed font family name as a string.
     */
    public String getFixedFontFamily() {
        synchronized (mContentSettingsLock) {
            return mFixedFontFamily;
        }
    }

    /**
     * Set the sans-serif font family name.
     * @param font A font family name.
     */
    public void setSansSerifFontFamily(String font) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (font != null && !mSansSerifFontFamily.equals(font)) {
                mSansSerifFontFamily = font;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the sans-serif font family name.
     * @return The sans-serif font family name as a string.
     */
    public String getSansSerifFontFamily() {
        synchronized (mContentSettingsLock) {
            return mSansSerifFontFamily;
        }
    }

    /**
     * Set the serif font family name. The default is "sans-serif".
     * @param font A font family name.
     */
    public void setSerifFontFamily(String font) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (font != null && !mSerifFontFamily.equals(font)) {
                mSerifFontFamily = font;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the serif font family name. The default is "serif".
     * @return The serif font family name as a string.
     */
    public String getSerifFontFamily() {
        synchronized (mContentSettingsLock) {
            return mSerifFontFamily;
        }
    }

    /**
     * Set the cursive font family name.
     * @param font A font family name.
     */
    public void setCursiveFontFamily(String font) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (font != null && !mCursiveFontFamily.equals(font)) {
                mCursiveFontFamily = font;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the cursive font family name. The default is "cursive".
     * @return The cursive font family name as a string.
     */
    public String getCursiveFontFamily() {
        synchronized (mContentSettingsLock) {
            return mCursiveFontFamily;
        }
    }

    /**
     * Set the fantasy font family name.
     * @param font A font family name.
     */
    public void setFantasyFontFamily(String font) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (font != null && !mFantasyFontFamily.equals(font)) {
                mFantasyFontFamily = font;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the fantasy font family name. The default is "fantasy".
     * @return The fantasy font family name as a string.
     */
    public String getFantasyFontFamily() {
        synchronized (mContentSettingsLock) {
            return mFantasyFontFamily;
        }
    }

    /**
     * Set the minimum font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public void setMinimumFontSize(int size) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            size = clipFontSize(size);
            if (mMinimumFontSize != size) {
                mMinimumFontSize = size;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the minimum font size. The default is 8.
     * @return A non-negative integer between 1 and 72.
     */
    public int getMinimumFontSize() {
        synchronized (mContentSettingsLock) {
            return mMinimumFontSize;
        }
    }

    /**
     * Set the minimum logical font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public void setMinimumLogicalFontSize(int size) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            size = clipFontSize(size);
            if (mMinimumLogicalFontSize != size) {
                mMinimumLogicalFontSize = size;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the minimum logical font size. The default is 8.
     * @return A non-negative integer between 1 and 72.
     */
    public int getMinimumLogicalFontSize() {
        synchronized (mContentSettingsLock) {
            return mMinimumLogicalFontSize;
        }
    }

    /**
     * Set the default font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public void setDefaultFontSize(int size) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            size = clipFontSize(size);
            if (mDefaultFontSize != size) {
                mDefaultFontSize = size;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the default font size. The default is 16.
     * @return A non-negative integer between 1 and 72.
     */
    public int getDefaultFontSize() {
        synchronized (mContentSettingsLock) {
            return mDefaultFontSize;
        }
    }

    /**
     * Set the default fixed font size.
     * @param size A non-negative integer between 1 and 72.
     * Any number outside the specified range will be pinned.
     */
    public void setDefaultFixedFontSize(int size) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            size = clipFontSize(size);
            if (mDefaultFixedFontSize != size) {
                mDefaultFixedFontSize = size;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the default fixed font size. The default is 16.
     * @return A non-negative integer between 1 and 72.
     */
    public int getDefaultFixedFontSize() {
        synchronized (mContentSettingsLock) {
            return mDefaultFixedFontSize;
        }
    }

    /**
     * Tell the WebView to enable JavaScript execution.
     *
     * @param flag True if the WebView should execute JavaScript.
     */
    public void setJavaScriptEnabled(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mJavaScriptEnabled != flag) {
                mJavaScriptEnabled = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Sets whether JavaScript running in the context of a file scheme URL
     * should be allowed to access content from any origin. This includes
     * access to content from other file scheme URLs. See
     * {@link #setAllowFileAccessFromFileURLs}. To enable the most restrictive,
     * and therefore secure policy, this setting should be disabled.
     * <p>
     * The default value is true for API level
     * {@link android.os.Build.VERSION_CODES#ICE_CREAM_SANDWICH_MR1} and below,
     * and false for API level {@link android.os.Build.VERSION_CODES#JELLY_BEAN}
     * and above.
     *
     * @param flag whether JavaScript running in the context of a file scheme
     *             URL should be allowed to access content from any origin
     */
    public void setAllowUniversalAccessFromFileURLs(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mAllowUniversalAccessFromFileURLs != flag) {
                mAllowUniversalAccessFromFileURLs = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Sets whether JavaScript running in the context of a file scheme URL
     * should be allowed to access content from other file scheme URLs. To
     * enable the most restrictive, and therefore secure policy, this setting
     * should be disabled. Note that the value of this setting is ignored if
     * the value of {@link #getAllowUniversalAccessFromFileURLs} is true.
     * <p>
     * The default value is true for API level
     * {@link android.os.Build.VERSION_CODES#ICE_CREAM_SANDWICH_MR1} and below,
     * and false for API level {@link android.os.Build.VERSION_CODES#JELLY_BEAN}
     * and above.
     *
     * @param flag whether JavaScript running in the context of a file scheme
     *             URL should be allowed to access content from other file
     *             scheme URLs
     */
    public void setAllowFileAccessFromFileURLs(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mAllowFileAccessFromFileURLs != flag) {
                mAllowFileAccessFromFileURLs = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Tell the WebView to load image resources automatically.
     * Note that setting this flag to false this does not block image loads
     * from WebCore cache.
     * @param flag True if the WebView should load images automatically.
     */
    public void setLoadsImagesAutomatically(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mLoadsImagesAutomatically != flag) {
                mLoadsImagesAutomatically = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Return true if the WebView will load image resources automatically.
     * The default is true.
     * @return True if the WebView loads images automatically.
     */
    public boolean getLoadsImagesAutomatically() {
        synchronized (mContentSettingsLock) {
            return mLoadsImagesAutomatically;
        }
    }

    /**
     * Sets whether images are enabled for this WebView. Setting this from
     * false to true will reload the blocked images in place.
     * Note that unlike {@link #setLoadsImagesAutomatically}, setting this
     * flag to false this will block image loads from WebCore cache as well.
     * The default is true.
     * @param flag whether the WebView should enable images.
     */
    public void setImagesEnabled(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mImagesEnabled != flag) {
                mImagesEnabled = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Gets whether images are enabled for this WebView.
     * @return true if the WebView has images eanbled
     */
    public boolean getImagesEnabled() {
        synchronized (mContentSettingsLock) {
            return mImagesEnabled;
        }
    }

    /**
     * Return true if JavaScript is enabled. <b>Note: The default is false.</b>
     *
     * @return True if JavaScript is enabled.
     */
    public boolean getJavaScriptEnabled() {
        synchronized (mContentSettingsLock) {
            return mJavaScriptEnabled;
        }
    }

    /**
     * Gets whether JavaScript running in the context of a file scheme URL can
     * access content from any origin. This includes access to content from
     * other file scheme URLs.
     *
     * @return whether JavaScript running in the context of a file scheme URL
     *         can access content from any origin
     * @see #setAllowUniversalAccessFromFileURLs
     */
    public boolean getAllowUniversalAccessFromFileURLs() {
        synchronized (mContentSettingsLock) {
            return mAllowUniversalAccessFromFileURLs;
        }
    }

    /**
     * Gets whether JavaScript running in the context of a file scheme URL can
     * access content from other file scheme URLs.
     *
     * @return whether JavaScript running in the context of a file scheme URL
     *         can access content from other file scheme URLs
     * @see #setAllowFileAccessFromFileURLs
     */
    public boolean getAllowFileAccessFromFileURLs() {
        synchronized (mContentSettingsLock) {
            return mAllowFileAccessFromFileURLs;
        }
    }

    /**
     * Tell the WebView to enable plugins.
     * @param flag True if the WebView should load plugins.
     * @deprecated This method has been deprecated in favor of
     *             {@link #setPluginState}
     */
    @Deprecated
    public void setPluginsEnabled(boolean flag) {
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
    public void setPluginState(PluginState state) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mPluginState != state) {
                mPluginState = state;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Return true if plugins are enabled.
     * @return True if plugins are enabled.
     * @deprecated This method has been replaced by {@link #getPluginState}
     */
    @Deprecated
    public boolean getPluginsEnabled() {
        synchronized (mContentSettingsLock) {
            return mPluginState == PluginState.ON;
        }
    }

    /**
     * Return true if plugins are disabled.
     * @return True if plugins are disabled.
     * @hide
     */
    @CalledByNative
    private boolean getPluginsDisabled() {
        // This should only be called from SyncToNative, which is called
        // either from the constructor, or with mContentSettingsLock being held.
        return mPluginState == PluginState.OFF;
    }

    /**
     * Sets if plugins are disabled.
     * @return True if plugins are disabled.
     * @hide
     */
    @CalledByNative
    private void setPluginsDisabled(boolean disabled) {
        // This should only be called from SyncFromToNative, which is called
        // either from the constructor, or with mContentSettingsLock being held.
        mPluginState = disabled ? PluginState.OFF : PluginState.ON;
    }

    /**
     * Return the current plugin state.
     * @return A value corresponding to the enum PluginState.
     */
    public PluginState getPluginState() {
        synchronized (mContentSettingsLock) {
            return mPluginState;
        }
    }


    /**
     * Tell javascript to open windows automatically. This applies to the
     * javascript function window.open().
     * @param flag True if javascript can open windows automatically.
     */
    public void setJavaScriptCanOpenWindowsAutomatically(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mJavaScriptCanOpenWindowsAutomatically != flag) {
                mJavaScriptCanOpenWindowsAutomatically = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Return true if javascript can open windows automatically. The default
     * is false.
     * @return True if javascript can open windows automatically during
     *         window.open().
     */
    public boolean getJavaScriptCanOpenWindowsAutomatically() {
        synchronized (mContentSettingsLock) {
            return mJavaScriptCanOpenWindowsAutomatically;
        }
    }

    /**
     * Sets the underlying layout algorithm. The default is
     * {@link LayoutAlgorithm#NARROW_COLUMNS}.
     *
     * @param l the layout algorithm to use, as a {@link LayoutAlgorithm} value
     */
    public void setLayoutAlgorithm(LayoutAlgorithm l) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mLayoutAlgorithm != l) {
                mLayoutAlgorithm = l;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Gets the current layout algorithm.
     *
     * @return the layout algorithm in use, as a {@link LayoutAlgorithm} value
     * @see #setLayoutAlgorithm
     */
    public LayoutAlgorithm getLayoutAlgorithm() {
        synchronized (mContentSettingsLock) {
            return mLayoutAlgorithm;
        }
    }

    /**
     * Sets whether Text Auto-sizing layout algorithm is enabled.
     *
     * @param enabled whether Text Auto-sizing layout algorithm is enabled
     * @hide
     */
    @CalledByNative
    private void setTextAutosizingEnabled(boolean enabled) {
        // This should only be called from SyncFromNative, which is called
        // either from the constructor, or with mContentSettingsLock being held.
        mLayoutAlgorithm = enabled ?
                LayoutAlgorithm.TEXT_AUTOSIZING : LayoutAlgorithm.NARROW_COLUMNS;
    }

    /**
     * Gets whether Text Auto-sizing layout algorithm is enabled.
     *
     * @return true if Text Auto-sizing layout algorithm is enabled
     * @hide
     */
    @CalledByNative
    private boolean getTextAutosizingEnabled() {
        return mLayoutAlgorithm == LayoutAlgorithm.TEXT_AUTOSIZING;
    }

    /**
     * Tells the WebView whether it supports multiple windows. True means
     * that {@link WebChromeClient#onCreateWindow(WebView, boolean,
     * boolean, Message)} is implemented by the host application.
     */
    public void setSupportMultipleWindows(boolean support) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mSupportMultipleWindows != support) {
                mSupportMultipleWindows = support;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Gets whether the WebView is supporting multiple windows.
     *
     * @return true if the WebView is supporting multiple windows. This means
     *         that {@link WebChromeClient#onCreateWindow(WebView, boolean,
     *         boolean, Message)} is implemented by the host application.
     */
    public boolean supportMultipleWindows() {
        synchronized (mContentSettingsLock) {
            return mSupportMultipleWindows;
        }
    }

    /**
     * Sets whether the WebView should enable support for the &quot;viewport&quot;
     * HTML meta tag or should use a wide viewport.
     * When the value of the setting is false, the layout width is always set to the
     * width of the WebView control in device-independent (CSS) pixels.
     * When the value is true and the page contains the viewport meta tag, the value
     * of the width specified in the tag is used. If the page does not contain the tag or
     * does not provide a width, then a wide viewport will be used.
     *
     * @param use whether to enable support for the viewport meta tag
     */
    public void setUseWideViewPort(boolean use) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mUseWideViewport != use) {
                mUseWideViewport = use;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Gets whether the WebView supports the &quot;viewport&quot;
     * HTML meta tag or will use a wide viewport.
     *
     * @return true if the WebView supports the viewport meta tag
     * @see #setUseWideViewPort
     */
    public boolean getUseWideViewPort() {
        synchronized (mContentSettingsLock) {
            return mUseWideViewport;
        }
    }

    /**
     * Sets whether the Application Caches API should be enabled. The default
     * is false. Note that in order for the Application Caches API to be
     * enabled, a non-empty database path must also be supplied to
     * {@link #setAppCachePath} (this is done for compatibility with the
     * legacy implementation).
     *
     * @param flag true if the WebView should enable Application Caches
     */
    public void setAppCacheEnabled(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mAppCacheEnabled != flag) {
                mAppCacheEnabled = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Sets the path to the Application Caches files. In order for the
     * Application Caches API to be enabled, this method must be called with a
     * non-empty path. This method should only be called once: repeated calls
     * are ignored.
     *
     * @param path a non empty-string
     */
    public void setAppCachePath(String path) {
        assert mCanModifySettings;
        boolean needToSync = false;
        synchronized (sGlobalContentSettingsLock) {
            // AppCachePath can only be set once.
            if (!sAppCachePathIsSet && path != null && !path.isEmpty()) {
                sAppCachePathIsSet = true;
                needToSync = true;
            }
        }
        // The obvious problem here is that other WebViews will not be updated,
        // until they execute synchronization from Java to the native side.
        // But this is the same behaviour as it was in the legacy WebView.
        if (needToSync) {
            synchronized (mContentSettingsLock) {
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Gets whether Application Cache is enabled.
     *
     * @return true if Application Cache is enabled
     * @hide
     */
    @CalledByNative
    private boolean getAppCacheEnabled() {
        // This should only be called from SyncToNative, which is called
        // either from the constructor, or with mContentSettingsLock being held.
        if (!mAppCacheEnabled) {
            return false;
        }
        synchronized (sGlobalContentSettingsLock) {
            return sAppCachePathIsSet;
        }
    }

    /**
     * Sets whether the DOM storage API is enabled. The default value is false.
     *
     * @param flag true if the ContentView should use the DOM storage API
     */
    public void setDomStorageEnabled(boolean flag) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (mDomStorageEnabled != flag) {
                mDomStorageEnabled = flag;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Gets whether the DOM Storage APIs are enabled.
     *
     * @return true if the DOM Storage APIs are enabled
     * @see #setDomStorageEnabled
     */
    public boolean getDomStorageEnabled() {
       synchronized (mContentSettingsLock) {
           return mDomStorageEnabled;
       }
    }

    /**
     * Set the default text encoding name to use when decoding html pages.
     * @param encoding The text encoding name.
     */
    public void setDefaultTextEncodingName(String encoding) {
        assert mCanModifySettings;
        synchronized (mContentSettingsLock) {
            if (encoding != null && !mDefaultTextEncoding.equals(encoding)) {
                mDefaultTextEncoding = encoding;
                mEventHandler.syncSettingsLocked();
            }
        }
    }

    /**
     * Get the default text encoding name. The default is "Latin-1".
     * @return The default text encoding name as a string.
     */
    public String getDefaultTextEncodingName() {
        synchronized (mContentSettingsLock) {
            return mDefaultTextEncoding;
        }
    }

    private int clipFontSize(int size) {
        if (size < MINIMUM_FONT_SIZE) {
            return MINIMUM_FONT_SIZE;
        } else if (size > MAXIMUM_FONT_SIZE) {
            return MAXIMUM_FONT_SIZE;
        }
        return size;
    }

    /**
     * Sets the settings in this object to those from another
     * ContentSettings.
     * Required by WebView when we swap a in a new ContentViewCore
     * to an existing AwContents (i.e. to support displaying popup
     * windows in an already created WebView)
     */
    public void initFrom(ContentSettings settings) {
        setLayoutAlgorithm(settings.getLayoutAlgorithm());
        setTextZoom(settings.getTextZoom());
        setStandardFontFamily(settings.getStandardFontFamily());
        setFixedFontFamily(settings.getFixedFontFamily());
        setSansSerifFontFamily(settings.getSansSerifFontFamily());
        setSerifFontFamily(settings.getSerifFontFamily());
        setCursiveFontFamily(settings.getCursiveFontFamily());
        setFantasyFontFamily(settings.getFantasyFontFamily());
        setDefaultTextEncodingName(settings.getDefaultTextEncodingName());
        setUserAgentString(settings.getUserAgentString());
        setMinimumFontSize(settings.getMinimumFontSize());
        setMinimumLogicalFontSize(settings.getMinimumLogicalFontSize());
        setDefaultFontSize(settings.getDefaultFontSize());
        setDefaultFixedFontSize(settings.getDefaultFixedFontSize());
        setLoadsImagesAutomatically(settings.getLoadsImagesAutomatically());
        setImagesEnabled(settings.getImagesEnabled());
        setJavaScriptEnabled(settings.getJavaScriptEnabled());
        setAllowUniversalAccessFromFileURLs(settings.getAllowUniversalAccessFromFileURLs());
        setAllowFileAccessFromFileURLs(settings.getAllowFileAccessFromFileURLs());
        setJavaScriptCanOpenWindowsAutomatically(
                settings.getJavaScriptCanOpenWindowsAutomatically());
        setSupportMultipleWindows(settings.supportMultipleWindows());
        setPluginState(settings.getPluginState());
        setAppCacheEnabled(settings.mAppCacheEnabled);
        setDomStorageEnabled(settings.getDomStorageEnabled());
        setSupportZoom(settings.supportZoom());
        setBuiltInZoomControls(settings.getBuiltInZoomControls());
        setDisplayZoomControls(settings.getDisplayZoomControls());
    }

    /**
     * Synchronize java side and native side settings. When ContentView
     * is running in PERSONALITY_VIEW mode, this needs to be done after
     * any java side setting is changed to sync them to native. In
     * PERSONALITY_CHROME mode, this needs to be called whenever native
     * settings are changed to sync them to java.
     */
    void syncSettings() {
        synchronized (mContentSettingsLock) {
            mEventHandler.syncSettingsLocked();
        }
    }

    void syncToNativeOnUiThread() {
        if (mNativeContentSettings != 0) nativeSyncToNative(mNativeContentSettings);
    }

    void syncFromNativeOnUiThread() {
        if (mNativeContentSettings != 0) nativeSyncFromNative(mNativeContentSettings);
    }

    // Initialize the ContentSettings native side.
    private native int nativeInit(int contentViewPtr, boolean isMasterMode);

    private static native String nativeGetDefaultUserAgent();

    // Synchronize Java settings from native settings.
    private native void nativeSyncFromNative(int nativeContentSettings);

    // Synchronize native settings from Java settings.
    private native void nativeSyncToNative(int nativeContentSettings);
}
