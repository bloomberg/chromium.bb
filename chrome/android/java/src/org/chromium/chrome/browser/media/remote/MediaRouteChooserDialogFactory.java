// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v7.app.MediaRouteChooserDialog;
import android.support.v7.app.MediaRouteChooserDialogFragment;
import android.support.v7.app.MediaRouteDialogFactory;
import android.support.v7.media.MediaRouter;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.media.remote.MediaRouteController.MediaStateListener;

/**
 * The Chrome implementation of the dialog factory so custom behavior can
 * be injected for the disconnect button.
 */
public class MediaRouteChooserDialogFactory extends MediaRouteDialogFactory {

    private final MediaRouteController mController;
    private final Context mContext;
    private final MediaStateListener mPlayer;

    MediaRouteChooserDialogFactory(MediaStateListener player, MediaRouteController controller,
            Context context) {
        mPlayer = player;
        mController = controller;
        mContext = context;
    }

    private static class SystemVisibilitySaver {
        private int mSystemVisibility;
        private boolean mRestoreSystemVisibility;

        void saveSystemVisibility(Activity activity) {
            // If we are in fullscreen we may have also have hidden the system UI. This
            // is overridden when we display the dialog. Save the system UI visibility
            // state so we can restore it.
            FrameLayout decor = (FrameLayout) activity.getWindow().getDecorView();
            mSystemVisibility = decor.getSystemUiVisibility();
            mRestoreSystemVisibility = (
                    (mSystemVisibility & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0);
        }

        void restoreSystemVisibility(Activity activity) {
            if (mRestoreSystemVisibility) {
                FrameLayout decor = (FrameLayout) activity.getWindow().getDecorView();
                // In some cases we come out of fullscreen before closing this dialog. In these
                // cases we don't want to restore the system UI visibility state.
                int systemVisibility = decor.getSystemUiVisibility();
                if ((systemVisibility & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0) {
                    decor.setSystemUiVisibility(mSystemVisibility);
                }
            }
        }
    }

    @Override
    public MediaRouteChooserDialogFragment onCreateChooserDialogFragment() {
        return new MediaRouteChooserDialogFragment() {
            final SystemVisibilitySaver mVisibilitySaver = new SystemVisibilitySaver();
            boolean mCancelled = false;

            @Override
            public MediaRouteChooserDialog onCreateChooserDialog(
                    Context context, Bundle savedInstanceState) {
                mVisibilitySaver.saveSystemVisibility(getActivity());
                return new MediaRouteChooserDialog(context);
            }

            @Override
            public void onStop() {
                super.onStop();
                mVisibilitySaver.restoreSystemVisibility(getActivity());
            }

            @Override
            public void onCancel(DialogInterface dialog) {
                mCancelled = true;

                super.onCancel(dialog);
            }

            @Override
            public void onDismiss(DialogInterface dialog) {
                super.onDismiss(dialog);

                if (mCancelled) return;
                MediaRouter router = MediaRouter.getInstance(mContext);
                mController.onRouteSelected(mPlayer, router, router.getSelectedRoute());
            }
        };
    }
}
