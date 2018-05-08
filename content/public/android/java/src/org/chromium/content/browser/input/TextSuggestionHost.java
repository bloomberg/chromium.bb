// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.PopupController;
import org.chromium.content.browser.PopupController.HideablePopup;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles displaying the Android spellcheck/text suggestion menu (provided by
 * SuggestionsPopupWindow) when requested by the C++ class TextSuggestionHostAndroid and applying
 * the commands in that menu (by calling back to the C++ class).
 */
@JNINamespace("content")
public class TextSuggestionHost implements WindowEventObserver, HideablePopup {
    private long mNativeTextSuggestionHost;
    private final WebContentsImpl mWebContents;

    private Context mContext;
    private ViewAndroidDelegate mViewDelegate;
    private boolean mIsAttachedToWindow;
    private WindowAndroid mWindowAndroid;

    private SpellCheckPopupWindow mSpellCheckPopupWindow;
    private TextSuggestionsPopupWindow mTextSuggestionsPopupWindow;

    private boolean mInitialized;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<TextSuggestionHost> INSTANCE = TextSuggestionHost::new;
    }

    /**
     * Create {@link TextSuggestionHost} instance.
     * @param context Context for action mode.
     * @param webContents WebContents instance.
     * @param windowAndroid The current WindowAndroid instance.
     */
    public static TextSuggestionHost create(
            Context context, WebContents webContents, WindowAndroid windowAndroid) {
        TextSuggestionHost host = webContents.getOrSetUserData(
                TextSuggestionHost.class, UserDataFactoryLazyHolder.INSTANCE);
        assert host != null;
        assert !host.initialized();
        host.init(context, windowAndroid);
        return host;
    }

    /**
     * Get {@link TextSuggestionHost} object used for the give WebContents.
     * {@link #create()} should precede any calls to this.
     * @param webContents {@link WebContents} object.
     * @return {@link TextSuggestionHost} object. {@code null} if not available because
     *         {@link #create()} is not called yet.
     */
    public static TextSuggestionHost fromWebContents(WebContents webContents) {
        return webContents.getOrSetUserData(TextSuggestionHost.class, null);
    }

    /**
     * Create {@link TextSuggestionHost} instance.
     * @param webContents WebContents instance.
     */
    public TextSuggestionHost(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
    }

    private void init(Context context, WindowAndroid windowAndroid) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mViewDelegate = mWebContents.getViewAndroidDelegate();
        assert mViewDelegate != null;
        mNativeTextSuggestionHost = nativeInit(mWebContents);
        PopupController.register(mWebContents, this);
        WindowEventObserverManager.from(mWebContents).addObserver(this);
        mInitialized = true;
    }

    private boolean initialized() {
        return mInitialized;
    }

    private float getContentOffsetYPix() {
        return mWebContents.getRenderCoordinates().getContentOffsetYPix();
    }

    // WindowEventObserver

    @Override
    public void onWindowAndroidChanged(WindowAndroid newWindowAndroid) {
        mWindowAndroid = newWindowAndroid;
        if (mSpellCheckPopupWindow != null) {
            mSpellCheckPopupWindow.updateWindowAndroid(mWindowAndroid);
        }
        if (mTextSuggestionsPopupWindow != null) {
            mTextSuggestionsPopupWindow.updateWindowAndroid(mWindowAndroid);
        }
    }

    @Override
    public void onAttachedToWindow() {
        mIsAttachedToWindow = true;
    }

    @Override
    public void onDetachedFromWindow() {
        mIsAttachedToWindow = false;
    }

    @Override
    public void onRotationChanged(int rotation) {
        hidePopups();
    }

    // HieablePopup
    @Override
    public void hide() {
        hidePopups();
    }

    @CalledByNative
    private void showSpellCheckSuggestionMenu(
            double caretXPx, double caretYPx, String markedText, String[] suggestions) {
        if (!mIsAttachedToWindow) {
            // This can happen if a new browser window is opened immediately after tapping a spell
            // check underline, before the timer to open the menu fires.
            onSuggestionMenuClosed(false);
            return;
        }

        hidePopups();
        mSpellCheckPopupWindow = new SpellCheckPopupWindow(
                mContext, this, mWindowAndroid, mViewDelegate.getContainerView());

        mSpellCheckPopupWindow.show(
                caretXPx, caretYPx + getContentOffsetYPix(), markedText, suggestions);
    }

    @CalledByNative
    private void showTextSuggestionMenu(
            double caretXPx, double caretYPx, String markedText, SuggestionInfo[] suggestions) {
        if (!mIsAttachedToWindow) {
            // This can happen if a new browser window is opened immediately after tapping a spell
            // check underline, before the timer to open the menu fires.
            onSuggestionMenuClosed(false);
            return;
        }

        hidePopups();
        mTextSuggestionsPopupWindow = new TextSuggestionsPopupWindow(
                mContext, this, mWindowAndroid, mViewDelegate.getContainerView());

        mTextSuggestionsPopupWindow.show(
                caretXPx, caretYPx + getContentOffsetYPix(), markedText, suggestions);
    }

    /**
     * Hides the text suggestion menu (and informs Blink that it was closed).
     */
    @CalledByNative
    public void hidePopups() {
        if (mTextSuggestionsPopupWindow != null && mTextSuggestionsPopupWindow.isShowing()) {
            mTextSuggestionsPopupWindow.dismiss();
            mTextSuggestionsPopupWindow = null;
        }

        if (mSpellCheckPopupWindow != null && mSpellCheckPopupWindow.isShowing()) {
            mSpellCheckPopupWindow.dismiss();
            mSpellCheckPopupWindow = null;
        }
    }

    /**
     * Tells Blink to replace the active suggestion range with the specified replacement.
     */
    public void applySpellCheckSuggestion(String suggestion) {
        nativeApplySpellCheckSuggestion(mNativeTextSuggestionHost, suggestion);
    }

    /**
     * Tells Blink to replace the active suggestion range with the specified suggestion on the
     * specified marker.
     */
    public void applyTextSuggestion(int markerTag, int suggestionIndex) {
        nativeApplyTextSuggestion(mNativeTextSuggestionHost, markerTag, suggestionIndex);
    }

    /**
     * Tells Blink to delete the active suggestion range.
     */
    public void deleteActiveSuggestionRange() {
        nativeDeleteActiveSuggestionRange(mNativeTextSuggestionHost);
    }

    /**
     * Tells Blink to remove spelling markers under all instances of the specified word.
     */
    public void onNewWordAddedToDictionary(String word) {
        nativeOnNewWordAddedToDictionary(mNativeTextSuggestionHost, word);
    }

    /**
     * Tells Blink the suggestion menu was closed (and also clears the reference to the
     * SuggestionsPopupWindow instance so it can be garbage collected).
     */
    public void onSuggestionMenuClosed(boolean dismissedByItemTap) {
        if (!dismissedByItemTap) {
            nativeOnSuggestionMenuClosed(mNativeTextSuggestionHost);
        }
        mSpellCheckPopupWindow = null;
        mTextSuggestionsPopupWindow = null;
    }

    @CalledByNative
    private void destroy() {
        hidePopups();
        mNativeTextSuggestionHost = 0;
    }

    /**
     * @return The TextSuggestionsPopupWindow, if one exists.
     */
    @VisibleForTesting
    public SuggestionsPopupWindow getTextSuggestionsPopupWindowForTesting() {
        return mTextSuggestionsPopupWindow;
    }

    /**
     * @return The SpellCheckPopupWindow, if one exists.
     */
    @VisibleForTesting
    public SuggestionsPopupWindow getSpellCheckPopupWindowForTesting() {
        return mSpellCheckPopupWindow;
    }

    private native long nativeInit(WebContents webContents);
    private native void nativeApplySpellCheckSuggestion(
            long nativeTextSuggestionHostAndroid, String suggestion);
    private native void nativeApplyTextSuggestion(
            long nativeTextSuggestionHostAndroid, int markerTag, int suggestionIndex);
    private native void nativeDeleteActiveSuggestionRange(long nativeTextSuggestionHostAndroid);
    private native void nativeOnNewWordAddedToDictionary(
            long nativeTextSuggestionHostAndroid, String word);
    private native void nativeOnSuggestionMenuClosed(long nativeTextSuggestionHostAndroid);
}
