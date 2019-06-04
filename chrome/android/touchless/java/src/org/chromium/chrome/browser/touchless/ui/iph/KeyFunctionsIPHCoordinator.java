// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import android.view.LayoutInflater;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.touchless.ui.tooltip.TooltipView;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A helper class that controls the UI for key functions In-Product Help in NoTouch mode.
 */
public class KeyFunctionsIPHCoordinator {
    private KeyFunctionsIPHMediator mMediator;

    public KeyFunctionsIPHCoordinator(
            TooltipView tooltipView, ActivityTabProvider activityTabProvider) {
        PropertyModel model = new PropertyModel.Builder(KeyFunctionsIPHProperties.ALL_KEYS)
                                      .with(KeyFunctionsIPHProperties.TOOLTIP_VIEW, tooltipView)
                                      .build();
        KeyFunctionsIPHView view =
                (KeyFunctionsIPHView) LayoutInflater.from(tooltipView.getContext())
                        .inflate(R.layout.notouch_key_functions_view, null);
        PropertyModelChangeProcessor.create(model, view, KeyFunctionsIPHViewBinder::bind);
        mMediator = new KeyFunctionsIPHMediator(model, activityTabProvider,
                ChromePreferenceManager.getInstance(),
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile()));
    }

    public void destroy() {
        mMediator.destroy();
    }
}
