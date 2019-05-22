// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.annotation.CallSuper;
import android.view.KeyEvent;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogPresenter;
import org.chromium.chrome.browser.touchless.snackbar.BlackHoleSnackbarManager;
import org.chromium.chrome.browser.touchless.ui.iph.KeyFunctionsIPHCoordinator;
import org.chromium.chrome.browser.touchless.ui.progressbar.ProgressBarCoordinator;
import org.chromium.chrome.browser.touchless.ui.progressbar.ProgressBarView;
import org.chromium.chrome.browser.touchless.ui.tooltip.TooltipView;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/** A coordinator for touchless UI. */
public class TouchlessUiCoordinatorImpl implements Destroyable, NativeInitObserver,
                                                   InflationObserver, PauseResumeWithNativeObserver,
                                                   TouchlessUiCoordinator {
    private ChromeActivity mActivity;

    private TouchlessModelCoordinator mModelCoordinator;
    private KeyFunctionsIPHCoordinator mKeyFunctionsIPHCoordinator;

    /** The snackbar manager for this activity that drops all snackbar requests. */
    private BlackHoleSnackbarManager mSnackbarManager;

    private ProgressBarCoordinator mProgressBarCoordinator;
    private TooltipView mTooltipView;
    private ProgressBarView mProgressBarView;

    /** The class that enables zooming for all websites and handles touchless zooming. */
    private TouchlessZoomHelper mTouchlessZoomHelper;

    public TouchlessUiCoordinatorImpl(ChromeActivity activity) {
        mActivity = activity;
        mActivity.getLifecycleDispatcher().register(this);
        mModelCoordinator = AppHooks.get().createTouchlessModelCoordinator(mActivity);
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        // Only NoTouchActivity wants the progress bar and tooltips.
        if (!(mActivity instanceof NoTouchActivity)) return;
        ViewGroup coordinatorLayout = (ViewGroup) mActivity.findViewById(R.id.coordinator);
        mTooltipView = new TooltipView(mActivity);
        mProgressBarView = new ProgressBarView(mActivity);
        coordinatorLayout.addView(mTooltipView);
        coordinatorLayout.addView(mProgressBarView);

        mProgressBarCoordinator =
                new ProgressBarCoordinator(mProgressBarView, mActivity.getActivityTabProvider());
    }

    @Override
    public void onFinishNativeInitialization() {
        // Only NoTouchActivity wants the tooltips.
        if (mActivity instanceof NoTouchActivity) {
            mKeyFunctionsIPHCoordinator = new KeyFunctionsIPHCoordinator(
                    mTooltipView, mActivity.getActivityTabProvider());
        }
        mTouchlessZoomHelper = new TouchlessZoomHelper(mActivity.getActivityTabProvider());
    }

    @Override
    public void onResumeWithNative() {
        if (mProgressBarCoordinator != null) mProgressBarCoordinator.onActivityResume();
    }

    @Override
    public void onPauseWithNative() {}

    @Override
    public KeyEvent processKeyEvent(KeyEvent event) {
        if (mProgressBarCoordinator != null) mProgressBarCoordinator.onKeyEvent();
        if (mModelCoordinator == null) return event;
        return mModelCoordinator.onKeyEvent(event);
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        if (mSnackbarManager == null) {
            mSnackbarManager = new BlackHoleSnackbarManager(mActivity);
        }
        return mSnackbarManager;
    }

    @Override
    public ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(
                new TouchlessDialogPresenter(mActivity, mModelCoordinator), ModalDialogType.APP);
    }

    protected TouchlessModelCoordinator getModelCoordinator() {
        return mModelCoordinator;
    }

    protected ChromeActivity getActivity() {
        return mActivity;
    }

    @Override
    @CallSuper
    public void destroy() {
        mActivity = null;
        if (mKeyFunctionsIPHCoordinator != null) mKeyFunctionsIPHCoordinator.destroy();
        if (mTouchlessZoomHelper != null) mTouchlessZoomHelper.destroy();
        if (mProgressBarCoordinator != null) mProgressBarCoordinator.destroy();
    }
}
