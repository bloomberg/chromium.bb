// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Parcel;
import android.os.ParcelUuid;
import android.os.Parcelable;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.ContentBitmapCallback;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.accessibility.AXTextStyle;

import java.util.UUID;

/**
 * The WebContentsImpl Java wrapper to allow communicating with the native WebContentsImpl
 * object.
 */
@JNINamespace("content")
//TODO(tedchoc): Remove the package restriction once this class moves to a non-public content
//               package whose visibility will be enforced via DEPS.
/* package */ class WebContentsImpl implements WebContents {
    private static final String PARCEL_VERSION_KEY = "version";
    private static final String PARCEL_WEBCONTENTS_KEY = "webcontents";
    private static final String PARCEL_PROCESS_GUARD_KEY = "processguard";

    private static final long PARCELABLE_VERSION_ID = 0;
    // Non-final for testing purposes, so resetting of the UUID can happen.
    private static UUID sParcelableUUID = UUID.randomUUID();

    /**
     * Used to reset the internal tracking for whether or not a serialized {@link WebContents}
     * was created in this process or not.
     */
    @VisibleForTesting
    public static void invalidateSerializedWebContentsForTesting() {
        sParcelableUUID = UUID.randomUUID();
    }

    /**
     * A {@link android.os.Parcelable.Creator} instance that is used to build
     * {@link WebContentsImpl} objects from a {@link Parcel}.
     */
    public static final Parcelable.Creator<WebContents> CREATOR =
            new Parcelable.Creator<WebContents>() {
                @Override
                public WebContents createFromParcel(Parcel source) {
                    Bundle bundle = source.readBundle();

                    // Check the version.
                    if (bundle.getLong(PARCEL_VERSION_KEY, -1) != 0) return null;

                    // Check that we're in the same process.
                    ParcelUuid parcelUuid = bundle.getParcelable(PARCEL_PROCESS_GUARD_KEY);
                    if (sParcelableUUID.compareTo(parcelUuid.getUuid()) != 0) return null;

                    // Attempt to retrieve the WebContents object from the native pointer.
                    return nativeFromNativePtr(bundle.getLong(PARCEL_WEBCONTENTS_KEY));
                }

                @Override
                public WebContents[] newArray(int size) {
                    return new WebContents[size];
                }
            };

    private long mNativeWebContentsAndroid;
    private NavigationController mNavigationController;

    // Lazily created proxy observer for handling all Java-based WebContentsObservers.
    private WebContentsObserverProxy mObserverProxy;

    private boolean mContextMenuOpened;

    private WebContentsImpl(
            long nativeWebContentsAndroid, NavigationController navigationController) {
        mNativeWebContentsAndroid = nativeWebContentsAndroid;
        mNavigationController = navigationController;
    }

    @CalledByNative
    private static WebContentsImpl create(
            long nativeWebContentsAndroid, NavigationController navigationController) {
        return new WebContentsImpl(nativeWebContentsAndroid, navigationController);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeWebContentsAndroid = 0;
        mNavigationController = null;
        if (mObserverProxy != null) {
            mObserverProxy.destroy();
            mObserverProxy = null;
        }
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        // This is wrapped in a Bundle so that failed deserialization attempts don't corrupt the
        // overall Parcel.  If we failed a UUID or Version check and didn't read the rest of the
        // fields it would corrupt the serialized stream.
        Bundle data = new Bundle();
        data.putLong(PARCEL_VERSION_KEY, PARCELABLE_VERSION_ID);
        data.putParcelable(PARCEL_PROCESS_GUARD_KEY, new ParcelUuid(sParcelableUUID));
        data.putLong(PARCEL_WEBCONTENTS_KEY, mNativeWebContentsAndroid);
        dest.writeBundle(data);
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeWebContentsAndroid;
    }

    @Override
    public void destroy() {
        if (mNativeWebContentsAndroid != 0) nativeDestroyWebContents(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isDestroyed() {
        return mNativeWebContentsAndroid == 0;
    }

    @Override
    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    @Override
    public String getTitle() {
        return nativeGetTitle(mNativeWebContentsAndroid);
    }

    @Override
    public String getVisibleUrl() {
        return nativeGetVisibleURL(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isLoading() {
        return nativeIsLoading(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isLoadingToDifferentDocument() {
        return nativeIsLoadingToDifferentDocument(mNativeWebContentsAndroid);
    }

    @Override
    public void stop() {
        nativeStop(mNativeWebContentsAndroid);
    }

    @Override
    public void cut() {
        nativeCut(mNativeWebContentsAndroid);
    }

    @Override
    public void copy() {
        nativeCopy(mNativeWebContentsAndroid);
    }

    @Override
    public void paste() {
        nativePaste(mNativeWebContentsAndroid);
    }

    @Override
    public void replace(String word) {
        nativeReplace(mNativeWebContentsAndroid, word);
    }

    @Override
    public void selectAll() {
        nativeSelectAll(mNativeWebContentsAndroid);
    }

    @Override
    public void unselect() {
        // Unselect may get triggered when certain selection-related widgets
        // are destroyed. As the timing for such destruction is unpredictable,
        // safely guard against this case.
        if (isDestroyed()) return;
        nativeUnselect(mNativeWebContentsAndroid);
    }

    @Override
    public void insertCSS(String css) {
        if (isDestroyed()) return;
        nativeInsertCSS(mNativeWebContentsAndroid, css);
    }

    @Override
    public void onHide() {
        nativeOnHide(mNativeWebContentsAndroid);
    }

    @Override
    public void onShow() {
        nativeOnShow(mNativeWebContentsAndroid);
    }

    @Override
    public void suspendAllMediaPlayers() {
        nativeSuspendAllMediaPlayers(mNativeWebContentsAndroid);
    }

    @Override
    public void setAudioMuted(boolean mute) {
        nativeSetAudioMuted(mNativeWebContentsAndroid, mute);
    }

    @Override
    public int getBackgroundColor() {
        return nativeGetBackgroundColor(mNativeWebContentsAndroid);
    }

    @Override
    public void showInterstitialPage(
            String url, long interstitialPageDelegateAndroid) {
        nativeShowInterstitialPage(mNativeWebContentsAndroid, url, interstitialPageDelegateAndroid);
    }

    @Override
    public boolean isShowingInterstitialPage() {
        return nativeIsShowingInterstitialPage(mNativeWebContentsAndroid);
    }

    @Override
    public boolean focusLocationBarByDefault() {
        return nativeFocusLocationBarByDefault(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isReady() {
        return nativeIsRenderWidgetHostViewReady(mNativeWebContentsAndroid);
    }

    @Override
    public void exitFullscreen() {
        nativeExitFullscreen(mNativeWebContentsAndroid);
    }

    @Override
    public void updateTopControlsState(boolean enableHiding, boolean enableShowing,
            boolean animate) {
        nativeUpdateTopControlsState(mNativeWebContentsAndroid, enableHiding,
                enableShowing, animate);
    }

    @Override
    public void showImeIfNeeded() {
        nativeShowImeIfNeeded(mNativeWebContentsAndroid);
    }

    @Override
    public void scrollFocusedEditableNodeIntoView() {
        // The native side keeps track of whether the zoom and scroll actually occurred. It is
        // more efficient to do it this way and sometimes fire an unnecessary message rather
        // than synchronize with the renderer and always have an additional message.
        nativeScrollFocusedEditableNodeIntoView(mNativeWebContentsAndroid);
    }

    @Override
    public void selectWordAroundCaret() {
        nativeSelectWordAroundCaret(mNativeWebContentsAndroid);
    }

    @Override
    public void adjustSelectionByCharacterOffset(int startAdjust, int endAdjust) {
        nativeAdjustSelectionByCharacterOffset(
                mNativeWebContentsAndroid, startAdjust, endAdjust);
    }

    @Override
    public String getUrl() {
        return nativeGetURL(mNativeWebContentsAndroid);
    }

    @Override
    public String getLastCommittedUrl() {
        return nativeGetLastCommittedURL(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isIncognito() {
        return nativeIsIncognito(mNativeWebContentsAndroid);
    }

    @Override
    public void resumeLoadingCreatedWebContents() {
        nativeResumeLoadingCreatedWebContents(mNativeWebContentsAndroid);
    }

    @Override
    public void evaluateJavaScript(String script, JavaScriptCallback callback) {
        if (isDestroyed()) return;
        nativeEvaluateJavaScript(mNativeWebContentsAndroid, script, callback);
    }

    @Override
    @VisibleForTesting
    public void evaluateJavaScriptForTests(String script, JavaScriptCallback callback) {
        nativeEvaluateJavaScriptForTests(mNativeWebContentsAndroid, script, callback);
    }

    @Override
    public void addMessageToDevToolsConsole(int level, String message) {
        nativeAddMessageToDevToolsConsole(mNativeWebContentsAndroid, level, message);
    }

    @Override
    public void sendMessageToFrame(String frameName, String message, String targetOrigin) {
        nativeSendMessageToFrame(mNativeWebContentsAndroid, frameName, message, targetOrigin);
    }

    @Override
    public boolean hasAccessedInitialDocument() {
        return nativeHasAccessedInitialDocument(mNativeWebContentsAndroid);
    }

    @CalledByNative
    private static void onEvaluateJavaScriptResult(
            String jsonResult, JavaScriptCallback callback) {
        callback.handleJavaScriptResult(jsonResult);
    }

    @Override
    public int getThemeColor(int defaultColor) {
        int color = nativeGetThemeColor(mNativeWebContentsAndroid);
        if (color == Color.TRANSPARENT) return defaultColor;

        return (color | 0xFF000000);
    }

    @Override
    public void requestAccessibilitySnapshot(AccessibilitySnapshotCallback callback,
            float offsetY, float scrollX) {
        nativeRequestAccessibilitySnapshot(mNativeWebContentsAndroid, callback,
                offsetY, scrollX);
    }

    @Override
    public void resumeMediaSession() {
        nativeResumeMediaSession(mNativeWebContentsAndroid);
    }

    @Override
    public void suspendMediaSession() {
        nativeSuspendMediaSession(mNativeWebContentsAndroid);
    }

    @Override
    public void stopMediaSession() {
        nativeStopMediaSession(mNativeWebContentsAndroid);
    }

    @Override
    public String getEncoding() {
        return nativeGetEncoding(mNativeWebContentsAndroid);
    }

    // root node can be null if parsing fails.
    @CalledByNative
    private static void onAccessibilitySnapshot(AccessibilitySnapshotNode root,
            AccessibilitySnapshotCallback callback) {
        callback.onAccessibilitySnapshot(root);
    }

    @CalledByNative
    private static void addAccessibilityNodeAsChild(AccessibilitySnapshotNode parent,
            AccessibilitySnapshotNode child) {
        parent.addChild(child);
    }

    @CalledByNative
    private static AccessibilitySnapshotNode createAccessibilitySnapshotNode(int x,
            int y, int scrollX, int scrollY, int width, int height, String text,
            int color, int bgcolor, float size, int textStyle, String className) {

        AccessibilitySnapshotNode node = new AccessibilitySnapshotNode(x, y, scrollX,
                scrollY, width, height, text, className);
        // if size is smaller than 0, then style information does not exist.
        if (size >= 0.0) {
            boolean bold = (textStyle & AXTextStyle.text_style_bold) > 0;
            boolean italic = (textStyle & AXTextStyle.text_style_italic) > 0;
            boolean underline = (textStyle & AXTextStyle.text_style_underline) > 0;
            boolean lineThrough = (textStyle & AXTextStyle.text_style_line_through) > 0;
            node.setStyle(color, bgcolor, size, bold, italic, underline, lineThrough);
        }
        return node;
    }

    @CalledByNative
    private static void setAccessibilitySnapshotSelection(
            AccessibilitySnapshotNode node, int start, int end) {
        node.setSelection(start, end);
    }

    @Override
    public void addObserver(WebContentsObserver observer) {
        assert mNativeWebContentsAndroid != 0;
        if (mObserverProxy == null) mObserverProxy = new WebContentsObserverProxy(this);
        mObserverProxy.addObserver(observer);
    }

    @Override
    public void removeObserver(WebContentsObserver observer) {
        if (mObserverProxy == null) return;
        mObserverProxy.removeObserver(observer);
    }

    @VisibleForTesting
    @Override
    public ObserverList.RewindableIterator<WebContentsObserver> getObserversForTesting() {
        return mObserverProxy.getObserversForTesting();
    }

    @Override
    public void getContentBitmapAsync(Bitmap.Config config, float scale, Rect srcRect,
            ContentBitmapCallback callback) {
        nativeGetContentBitmap(mNativeWebContentsAndroid, callback, config, scale,
                srcRect.top, srcRect.left, srcRect.width(), srcRect.height());
    }

    @Override
    public void onContextMenuOpened() {
        mContextMenuOpened = true;
    }

    @Override
    public void onContextMenuClosed() {
        if (!mContextMenuOpened) {
            return;
        } else {
            mContextMenuOpened = false;
        }

        if (mNativeWebContentsAndroid != 0) {
            nativeOnContextMenuClosed(mNativeWebContentsAndroid);
        }
    }

    @CalledByNative
    private void onGetContentBitmapFinished(ContentBitmapCallback callback, Bitmap bitmap,
            int response) {
        callback.onFinishGetBitmap(bitmap, response);
    }

    @Override
    public void reloadLoFiImages() {
        nativeReloadLoFiImages(mNativeWebContentsAndroid);
    }

    // This is static to avoid exposing a public destroy method on the native side of this class.
    private static native void nativeDestroyWebContents(long webContentsAndroidPtr);

    private static native WebContents nativeFromNativePtr(long webContentsAndroidPtr);

    private native String nativeGetTitle(long nativeWebContentsAndroid);
    private native String nativeGetVisibleURL(long nativeWebContentsAndroid);
    private native boolean nativeIsLoading(long nativeWebContentsAndroid);
    private native boolean nativeIsLoadingToDifferentDocument(long nativeWebContentsAndroid);
    private native void nativeStop(long nativeWebContentsAndroid);
    private native void nativeCut(long nativeWebContentsAndroid);
    private native void nativeCopy(long nativeWebContentsAndroid);
    private native void nativePaste(long nativeWebContentsAndroid);
    private native void nativeReplace(long nativeWebContentsAndroid, String word);
    private native void nativeSelectAll(long nativeWebContentsAndroid);
    private native void nativeUnselect(long nativeWebContentsAndroid);
    private native void nativeInsertCSS(long nativeWebContentsAndroid, String css);
    private native void nativeOnHide(long nativeWebContentsAndroid);
    private native void nativeOnShow(long nativeWebContentsAndroid);
    private native void nativeSuspendAllMediaPlayers(long nativeWebContentsAndroid);
    private native void nativeSetAudioMuted(long nativeWebContentsAndroid, boolean mute);
    private native int nativeGetBackgroundColor(long nativeWebContentsAndroid);
    private native void nativeShowInterstitialPage(long nativeWebContentsAndroid,
            String url, long nativeInterstitialPageDelegateAndroid);
    private native boolean nativeIsShowingInterstitialPage(long nativeWebContentsAndroid);
    private native boolean nativeFocusLocationBarByDefault(long nativeWebContentsAndroid);
    private native boolean nativeIsRenderWidgetHostViewReady(long nativeWebContentsAndroid);
    private native void nativeExitFullscreen(long nativeWebContentsAndroid);
    private native void nativeUpdateTopControlsState(long nativeWebContentsAndroid,
            boolean enableHiding, boolean enableShowing, boolean animate);
    private native void nativeShowImeIfNeeded(long nativeWebContentsAndroid);
    private native void nativeScrollFocusedEditableNodeIntoView(long nativeWebContentsAndroid);
    private native void nativeSelectWordAroundCaret(long nativeWebContentsAndroid);
    private native void nativeAdjustSelectionByCharacterOffset(
            long nativeWebContentsAndroid, int startAdjust, int endAdjust);
    private native String nativeGetURL(long nativeWebContentsAndroid);
    private native String nativeGetLastCommittedURL(long nativeWebContentsAndroid);
    private native boolean nativeIsIncognito(long nativeWebContentsAndroid);
    private native void nativeResumeLoadingCreatedWebContents(long nativeWebContentsAndroid);
    private native void nativeEvaluateJavaScript(long nativeWebContentsAndroid,
            String script, JavaScriptCallback callback);
    private native void nativeEvaluateJavaScriptForTests(long nativeWebContentsAndroid,
            String script, JavaScriptCallback callback);
    private native void nativeAddMessageToDevToolsConsole(
            long nativeWebContentsAndroid, int level, String message);
    private native void nativeSendMessageToFrame(long nativeWebContentsAndroid,
            String frameName, String message, String targetOrigin);
    private native boolean nativeHasAccessedInitialDocument(
            long nativeWebContentsAndroid);
    private native int nativeGetThemeColor(long nativeWebContentsAndroid);
    private native void nativeRequestAccessibilitySnapshot(long nativeWebContentsAndroid,
            AccessibilitySnapshotCallback callback, float offsetY, float scrollX);
    private native void nativeResumeMediaSession(long nativeWebContentsAndroid);
    private native void nativeSuspendMediaSession(long nativeWebContentsAndroid);
    private native void nativeStopMediaSession(long nativeWebContentsAndroid);
    private native String nativeGetEncoding(long nativeWebContentsAndroid);
    private native void nativeGetContentBitmap(long nativeWebContentsAndroid,
            ContentBitmapCallback callback, Bitmap.Config config, float scale,
            float x, float y, float width, float height);
    private native void nativeOnContextMenuClosed(long nativeWebContentsAndroid);
    private native void nativeReloadLoFiImages(long nativeWebContentsAndroid);
}
