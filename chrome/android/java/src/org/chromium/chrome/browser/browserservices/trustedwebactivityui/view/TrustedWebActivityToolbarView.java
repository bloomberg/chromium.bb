// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.view;

import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.TOOLBAR_HIDDEN;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyObservable;

import javax.inject.Inject;

/**
 * Hides and shows the toolbar according to the state of the Trusted Web Activity.
 *
 * TODO(pshmakov): Since we now have CustomTabToolbarCoordinator representing toolbar as a separate
 * feature, this class doesn't make sense any more. Remove it, and let TrustedWebActivityCoordinator
 * talk to CustomTabToolbarCoordinator.
 */
@ActivityScope
public class TrustedWebActivityToolbarView implements
        PropertyObservable.PropertyObserver<PropertyKey> {
    private final CustomTabToolbarCoordinator mCustomTabToolbarCoordinator;
    private final TrustedWebActivityModel mModel;
    private final ChromeActivity mActivity;

    @Inject
    public TrustedWebActivityToolbarView(CustomTabToolbarCoordinator customTabToolbarCoordinator,
            TrustedWebActivityModel model,
            ChromeActivity activity) {
        mCustomTabToolbarCoordinator = customTabToolbarCoordinator;
        mModel = model;
        mActivity = activity;
        mModel.addObserver(this);
    }

    @Override
    public void onPropertyChanged(PropertyObservable<PropertyKey> observable, PropertyKey key) {
        if (mActivity.isActivityFinishingOrDestroyed()) {
            assert false : "Tried to change toolbar visibility when activity is destroyed";
            return;
        }
        if (key != TOOLBAR_HIDDEN) return;

        boolean hide = mModel.get(TOOLBAR_HIDDEN);

        mCustomTabToolbarCoordinator.setToolbarHidden(hide);

        if (!hide) {
            // Force showing the controls for a bit when leaving Trusted Web Activity mode.
            mCustomTabToolbarCoordinator.showToolbarTemporarily();
        }
    }
}
