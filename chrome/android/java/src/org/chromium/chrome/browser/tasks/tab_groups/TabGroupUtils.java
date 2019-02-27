// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.res.Resources;
import android.support.annotation.StringRes;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Helper class to handle tab groups related utilities.
 */
public class TabGroupUtils {
    public static void maybeShowIPH(@FeatureConstants String featureName, View view) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)) return;

        Resources res = view.getContext().getResources();
        @StringRes
        int textId;
        if (featureName.equals(FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE)) {
            textId = R.string.iph_tab_groups_quickly_compare_pages_text;
        } else if (featureName.equals(FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE)) {
            textId = R.string.iph_tab_groups_tap_to_see_another_tab_text;
        } else if (featureName.equals(FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE)) {
            textId = R.string.iph_tab_groups_your_tabs_together_text;
        } else {
            assert false;
            return;
        }

        final Tracker tracker = TrackerFactory.getTrackerForProfile(
                Profile.getLastUsedProfile().getOriginalProfile());
        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(view);

        TextBubble textBubble =
                new TextBubble(view.getContext(), view, textId, textId, true, rectProvider);
        textBubble.setDismissOnTouchInteraction(true);
        textBubble.addOnDismissListener(() -> tracker.dismissed(featureName));
        textBubble.show();
    }
}
