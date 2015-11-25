// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;

/**
 * The base class for all InfoBar classes.
 * Note that infobars expire by default when a new navigation occurs.
 * Make sure to use setExpireOnNavigation(false) if you want an infobar to be sticky.
 */
public abstract class InfoBar implements InfoBarView {
    private static final String TAG = "InfoBar";

    private final int mIconDrawableId;
    private final Bitmap mIconBitmap;
    private final CharSequence mMessage;

    private InfoBarListeners.Dismiss mListener;
    private InfoBarContainer mContainer;
    private View mView;
    private Context mContext;

    private boolean mExpireOnNavigation;
    private boolean mIsDismissed;
    private boolean mControlsEnabled = true;
    private boolean mIsJavaOnlyInfoBar = true;

    // This points to the InfoBarAndroid class not any of its subclasses.
    private long mNativeInfoBarPtr;

    // Used by tests to reference infobars.
    private final int mId;
    private static int sIdCounter = 0;
    private static int generateId() {
        return sIdCounter++;
    }

    /**
     * @param listener Listens to when buttons have been clicked on the InfoBar.
     * @param iconDrawableId ID of the resource to use for the Icon.  If 0, no icon will be shown.
     * @param message The message to show in the infobar.
     */
    public InfoBar(InfoBarListeners.Dismiss listener, int iconDrawableId, Bitmap iconBitmap,
            CharSequence message) {
        mListener = listener;
        mId = generateId();
        mIconDrawableId = iconDrawableId;
        mIconBitmap = iconBitmap;
        mMessage = message;
        mExpireOnNavigation = true;
    }

    /**
     * Stores a pointer to the native-side counterpart of this InfoBar.
     * @param nativeInfoBarPtr Pointer to the native InfoBarAndroid, not to its subclass.
     */
    @CalledByNative
    private void setNativeInfoBar(long nativeInfoBarPtr) {
        if (nativeInfoBarPtr != 0) {
            // The native code takes care of expiring infobars on navigations.
            mExpireOnNavigation = false;
            mNativeInfoBarPtr = nativeInfoBarPtr;
            mIsJavaOnlyInfoBar = false;
        }
    }

    @CalledByNative
    protected void onNativeDestroyed() {
        mNativeInfoBarPtr = 0;
    }

    /**
     * Determine if the infobar should be dismissed when a new page starts loading. Calling
     * setExpireOnNavigation(true/false) causes this method always to return true/false.
     * This only applies to java-only infobars. C++ infobars will use the same logic
     * as other platforms so they are not attempted to be dismissed twice.
     * It should really be removed once all infobars have a C++ counterpart.
     */
    public final boolean shouldExpire() {
        return mExpireOnNavigation && mIsJavaOnlyInfoBar;
    }

    // Sets whether the bar should be dismissed when a navigation occurs.
    public void setExpireOnNavigation(boolean expireOnNavigation) {
        mExpireOnNavigation = expireOnNavigation;
    }

    /**
     * Sets the Context used when creating the InfoBar.
     */
    protected void setContext(Context context) {
        mContext = context;
    }

    /**
     * @return The Context used to create the InfoBar.  This will be null until the InfoBar is added
     *         to the InfoBarContainer, and should never be null afterward.
     */
    protected Context getContext() {
        return mContext;
    }

    /**
     * Creates the View that represents the InfoBar.
     * @return The View representing the InfoBar.
     */
    protected final View createView() {
        assert mContext != null;

        InfoBarLayout layout =
                new InfoBarLayout(mContext, this, mIconDrawableId, mIconBitmap, mMessage);
        createContent(layout);
        layout.onContentCreated();
        mView = layout;
        return mView;
    }

    /**
     * Replaces the View currently shown in the infobar with the given View. Triggers the swap
     * animation via the InfoBarContainer.
     */
    protected void replaceView(View newView) {
        mView = newView;
        mContainer.notifyInfoBarViewChanged();
    }

    /**
     * Returns the View shown in this infobar. Only valid after createView() has been called.
     */
    @Override
    public View getView() {
        return mView;
    }

    @Override
    public CharSequence getAccessibilityText() {
        if (mView == null) return "";
        TextView messageView = (TextView) mView.findViewById(R.id.infobar_message);
        if (messageView == null) return "";
        return messageView.getText() + mContext.getString(R.string.infobar_screen_position);
    }

    /**
     * Used to close a java only infobar.
     */
    public void dismissJavaOnlyInfoBar() {
        assert mNativeInfoBarPtr == 0;
        if (closeInfoBar() && mListener != null) {
            mListener.onInfoBarDismissed(this);
        }
    }

    /**
     * @return whether the infobar actually needed closing.
     */
    @CalledByNative
    public boolean closeInfoBar() {
        if (!mIsDismissed) {
            mIsDismissed = true;
            if (!mContainer.hasBeenDestroyed()) {
                // If the container was destroyed, it's already been emptied of all its infobars.
                mContainer.removeInfoBar(this);
            }
            return true;
        }
        return false;
    }

    void setInfoBarContainer(InfoBarContainer container) {
        mContainer = container;
    }

    @Override
    public boolean areControlsEnabled() {
        return mControlsEnabled;
    }

    @Override
    public void setControlsEnabled(boolean state) {
        mControlsEnabled = state;
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
    }

    @Override
    public void onLinkClicked() {
        if (mNativeInfoBarPtr != 0) nativeOnLinkClicked(mNativeInfoBarPtr);
    }

    /**
     * Performs some action related to the button being clicked.
     * @param action The type of action defined as ACTION_* in this class.
     */
    protected void onButtonClicked(int action) {
        if (mNativeInfoBarPtr != 0) nativeOnButtonClicked(mNativeInfoBarPtr, action);
    }

    @Override
    public void onCloseButtonClicked() {
        if (mIsJavaOnlyInfoBar) {
            dismissJavaOnlyInfoBar();
        } else {
            if (mNativeInfoBarPtr != 0) nativeOnCloseButtonClicked(mNativeInfoBarPtr);
        }
    }

    @Override
    public void createContent(InfoBarLayout layout) {
    }

    @VisibleForTesting
    public int getId() {
        return mId;
    }

    @VisibleForTesting
    public void setDismissedListener(InfoBarListeners.Dismiss listener) {
        mListener = listener;
    }

    private native void nativeOnLinkClicked(long nativeInfoBarAndroid);
    private native void nativeOnButtonClicked(long nativeInfoBarAndroid, int action);
    private native void nativeOnCloseButtonClicked(long nativeInfoBarAndroid);
}
