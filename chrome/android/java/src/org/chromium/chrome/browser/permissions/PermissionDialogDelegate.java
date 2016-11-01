// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.ui.base.WindowAndroid;

/**
 * Delegate class for modal permission dialogs. Contains all of the data displayed in a prompt,
 * including the button strings, message text, link text, the icon, and whether or not to display
 * a persistence toggle.
 *
 * This class is also the interface to the native-side permissions code. When the user responds to
 * the permission dialog, the decision is conveyed across the JNI so that the native code can
 * respond appropriately.
 */
public class PermissionDialogDelegate {
    /** The native-side counterpart of this class */
    private long mNativeDelegatePtr;

    /** The activity for which to create the dialog. */
    private Activity mActivity;

    /** The icon to display in the dialog. */
    private int mDrawableId;

    /** Text shown in the dialog. */
    private String mMessageText;

    /** Text shown on the link, e.g. "Learn more". */
    private String mLinkText;

    /** Text shown on the primary button, e.g. "Allow". */
    private String mPrimaryButtonText;

    /** Text shown on the secondary button, e.g. "Block". */
    private String mSecondaryButtonText;

    /** Whether or not to show a toggle for opting out of persisting the decision. */
    private boolean mShowPersistenceToggle;

    /** Whether or not a decision has been made for this delegate */
    private boolean mDecided;

    public Activity getActivity() {
        return mActivity;
    }

    public int getDrawableId() {
        return mDrawableId;
    }

    public String getMessageText() {
        return mMessageText;
    }

    public String getLinkText() {
        return mLinkText;
    }

    public String getPrimaryButtonText() {
        return mPrimaryButtonText;
    }

    public String getSecondaryButtonText() {
        return mSecondaryButtonText;
    }

    public boolean shouldShowPersistenceToggle() {
        return mShowPersistenceToggle;
    }

    public void onAccept(boolean persist) {
        assert mNativeDelegatePtr != 0;
        mDecided = true;
        nativeAccept(mNativeDelegatePtr, persist);
    }

    public void onCancel(boolean persist) {
        assert mNativeDelegatePtr != 0;
        mDecided = true;
        nativeCancel(mNativeDelegatePtr, persist);
    }

    public void onDismiss() {
        assert mNativeDelegatePtr != 0;

        // Do not call the native dismiss handler if an accept or cancel action is already recorded.
        if (!mDecided) nativeDismissed(mNativeDelegatePtr);
        nativeDestroy(mNativeDelegatePtr);
        mNativeDelegatePtr = 0;
    }

    public void onLinkClicked() {
        assert mNativeDelegatePtr != 0;
        nativeLinkClicked(mNativeDelegatePtr);
    }

    /**
     * Called from C++ by |nativeDelegatePtr| to instantiate this class.
     *
     * @param nativeDelegatePtr     The native counterpart that this object owns.
     * @param window                The window to create the dialog for.
     * @param iconResourceId        The id of the icon to display in the dialog.
     * @param message               The message to display in the dialog.
     * @param linkText              The text to display in the link (if any).
     * @param primaryTextButton     The text to display on the primary button.
     * @param secondaryTextButton   The text to display on the primary button.
     * @param showPersistenceToggle Whether or not to display a persistence toggle.
     */
    @CalledByNative
    private static PermissionDialogDelegate create(long nativeDelegatePtr, WindowAndroid window,
            int enumeratedIconId, String message, String linkText, String primaryButtonText,
            String secondaryButtonText, boolean showPersistenceToggle) {
        return new PermissionDialogDelegate(nativeDelegatePtr, window, enumeratedIconId, message,
                linkText, primaryButtonText, secondaryButtonText, showPersistenceToggle);
    }

    /**
     * Upon construction, this class takes ownership of the passed in native delegate.
     */
    private PermissionDialogDelegate(long nativeDelegatePtr, WindowAndroid window,
            int enumeratedIconId, String message, String linkText, String primaryButtonText,
            String secondaryButtonText, boolean showPersistenceToggle) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mActivity = window.getActivity().get();
        mDrawableId = ResourceId.mapToDrawableId(enumeratedIconId);
        mMessageText = message;
        mLinkText = linkText;
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mShowPersistenceToggle = showPersistenceToggle;
    }

    private native void nativeAccept(long nativePermissionDialogDelegate, boolean persist);
    private native void nativeCancel(long nativePermissionDialogDelegate, boolean persist);
    private native void nativeDismissed(long nativePermissionDialogDelegate);
    private native void nativeLinkClicked(long nativePermissionDialogDelegate);
    private native void nativeDestroy(long nativePermissionDialogDelegate);
}
