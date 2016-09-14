// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch.action;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.contextualsearch.gesture.SearchGestureHost;
import org.chromium.content_public.browser.WebContents;

/**
 * Represents an abstract action to do a Contextual Search, and supports native C++ functionality.
 * Subclasses will exist for a Resolved search action that determines the search based on page text,
 * and Verbatim search action that just searches for the literal selection without providing
 * context.
 * This is part of the 2016-refactoring (crbug.com/624609, go/cs-refactoring-2016).
 */
public abstract class SearchAction {
    private long mNativePointer;

    protected final SearchActionListener mListener;
    protected final SearchGestureHost mHost;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * Constructs an action that knows how to Search.  Current implementation is limited to
     * gathering the text on a Tap gesture in order to determine whether the Tap should be
     * suppressed or a search should be done or not, implemented by the
     * {@class ResolvedSearchAction} subclass.
     * @param listener The object to notify when the {@link SearchAction} state changes.
     * @param host The host object, which provides environment data.
     */
    public SearchAction(SearchActionListener listener, SearchGestureHost host) {
        mHost = host;
        mNativePointer = nativeInit();

        mListener = listener;
    }

    // ============================================================================================
    // Abstract
    // ============================================================================================

    /**
     * Extracts the context for the current search -- text surrounding the location of the Tap
     * gesture.
     */
    public abstract void extractContext();

    // ============================================================================================
    //
    // ============================================================================================

    /**
     * Called when the system determines that this action will not be acted upon.
     */
    public void dismissAction() {
        mHost.dismissGesture();
    }

    /**
     * Should be called when this object is no longer needed to clean up storage.
     */
    public void destroyAction() {
        onActionEnded();

        if (mNativePointer != 0L) {
            nativeDestroy(mNativePointer);
        }
    }

    // ============================================================================================
    // Suppression
    // ============================================================================================

    /**
     * @return Whether this action should be suppressed.
     */
    protected boolean shouldSuppressAction() {
        // TODO(donnd): integrate with native tap suppression.
        return false;
    }

    // ============================================================================================
    // State notification
    // ============================================================================================

    /**
     * Sends notification that the context is ready for use now.
     */
    protected void notifyContextReady() {
        onContextReady();
    }

    // ============================================================================================
    // Surrounding Text
    // ============================================================================================

    /**
     * Requests text surrounding the location of the caret.
     */
    protected void requestSurroundingText() {
        WebContents webContents = mHost.getTabWebContents();
        if (webContents != null) {
            nativeRequestSurroundingText(mNativePointer, webContents);
            // TODO(donnd): consider reusing this surrounding text for the resolve action too.
            // Currently we make an additional request for the surroundings after the UX selects the
            // word tapped, in order to resolve the search term based on that selection.
        } else {
            notifyContextReady();
        }
    }

    @CalledByNative
    protected void onSurroundingTextReady() {
        // No base class action here, subclass may override and take action.
    }

    // ============================================================================================
    // SearchAction states
    // ============================================================================================

    /**
     * Called to notify that the current context is ready.
     */
    private void onContextReady() {
        mListener.onContextReady(this);

        if (shouldSuppressAction()) {
            onActionSuppressed();
        } else {
            onActionAccepted();
        }
    }

    /**
     * Called when an action has been accepted to notify the listener.
     */
    private void onActionAccepted() {
        mListener.onActionAccepted(this);
    }

    /**
     * Called when an action has been suppressed to notify the listener.
     */
    private void onActionSuppressed() {
        mListener.onActionSuppressed(this);

        dismissAction();
    }

    /**
     * Called when an action has ended to notify the listener.
     */
    private void onActionEnded() {
        mListener.onActionEnded(this);
    }

    // ============================================================================================
    // Internals
    // ============================================================================================

    @CalledByNative
    private void clearNativePointer() {
        assert mNativePointer != 0;
        mNativePointer = 0;
    }

    // ============================================================================================
    // Native methods.
    // ============================================================================================

    // Native calls.
    private native long nativeInit();
    private native void nativeDestroy(long nativeSearchAction);

    private native void nativeRequestSurroundingText(
            long nativeSearchAction, WebContents webContents);
}
