// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.app.Activity;
import android.content.Context;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.ui.PhotoPickerListener;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * UI for the photo chooser that shows on the Android platform as a result of
 * &lt;input type=file accept=image &gt; form element.
 */
public class PhotoPickerDialog extends AlertDialog {
    // Our context.
    private Context mContext;

    // The category we're showing photos for.
    private PickerCategoryView mCategoryView;

    // A wrapper around the listener object, watching to see if an external intent is launched.
    private PhotoPickerListenerWrapper mListenerWrapper;

    // Whether the wait for an external intent launch is over.
    private boolean mDoneWaitingForExternalIntent;

    /**
     * A wrapper around {@link PhotoPickerListener} that listens for external intents being
     * launched.
     */
    private static class PhotoPickerListenerWrapper implements PhotoPickerListener {
        // The {@link PhotoPickerListener} to forward the events to.
        PhotoPickerListener mListener;

        // Whether the user selected to launch an external intent.
        private boolean mExternalIntentSelected;

        /**
         * The constructor, supplying the {@link PhotoPickerListener} object to encapsulate.
         */
        public PhotoPickerListenerWrapper(PhotoPickerListener listener) {
            mListener = listener;
        }

        // PhotoPickerListener:
        @Override
        public void onPickerUserAction(Action action, String[] photos) {
            mExternalIntentSelected = false;
            if (action == Action.LAUNCH_GALLERY || action == Action.LAUNCH_CAMERA) {
                mExternalIntentSelected = true;
            }

            mListener.onPickerUserAction(action, photos);
        }

        /**
         * Returns whether the user picked an external intent to launch.
         */
        public boolean externalIntentSelected() {
            return mExternalIntentSelected;
        }
    }

    /**
     * The PhotoPickerDialog constructor.
     * @param context The context to use.
     * @param listener The listener object that gets notified when an action is taken.
     * @param multiSelectionAllowed Whether the photo picker should allow multiple items to be
     *                              selected.
     * @param mimeTypes A list of mime types to show in the dialog.
     */
    public PhotoPickerDialog(Context context, PhotoPickerListener listener,
            boolean multiSelectionAllowed, List<String> mimeTypes) {
        super(context, R.style.FullscreenWhite);
        mContext = context;
        mListenerWrapper = new PhotoPickerListenerWrapper(listener);

        // Initialize the main content view.
        mCategoryView = new PickerCategoryView(context);
        mCategoryView.initialize(this, mListenerWrapper, multiSelectionAllowed, mimeTypes);
        setView(mCategoryView);
    }

    @Override
    public void dismiss() {
        if (!mListenerWrapper.externalIntentSelected() || mDoneWaitingForExternalIntent) {
            super.dismiss();
            mCategoryView.onDialogDismissed();
        } else {
            ApplicationStatus.registerStateListenerForActivity(new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    if (newState == ActivityState.STOPPED || newState == ActivityState.DESTROYED) {
                        mDoneWaitingForExternalIntent = true;
                        ApplicationStatus.unregisterActivityStateListener(this);
                        dismiss();
                    }
                }
            }, WindowAndroid.activityFromContext(mContext));
        }
    }

    @VisibleForTesting
    public PickerCategoryView getCategoryViewForTesting() {
        return mCategoryView;
    }
}
