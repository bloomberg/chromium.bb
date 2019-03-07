// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.support.annotation.StringRes;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Helper class to handle tab groups related utilities.
 */
public class TabGroupUtils {
    private static TabModelSelectorTabObserver sTabModelSelectorTabObserver;

    public static void maybeShowIPH(@FeatureConstants String featureName, View view) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)) return;

        @StringRes
        int textId;
        @StringRes
        int accessibilityTextId;
        switch (featureName) {
            case FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE:
                textId = R.string.iph_tab_groups_quickly_compare_pages_text;
                accessibilityTextId = textId;
                break;
            case FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE:
                textId = R.string.iph_tab_groups_tap_to_see_another_tab_text;
                accessibilityTextId =
                        R.string.iph_tab_groups_tap_to_see_another_tab_accessibility_text;
                break;
            case FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE:
                textId = R.string.iph_tab_groups_your_tabs_together_text;
                accessibilityTextId = textId;
                break;
            default:
                assert false;
                return;
        }

        final Tracker tracker = TrackerFactory.getTrackerForProfile(
                Profile.getLastUsedProfile().getOriginalProfile());
        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(view);

        TextBubble textBubble = new TextBubble(
                view.getContext(), view, textId, accessibilityTextId, true, rectProvider);
        textBubble.setDismissOnTouchInteraction(true);
        textBubble.addOnDismissListener(() -> tracker.dismissed(featureName));
        textBubble.show();
    }

    /**
     * Start a TabModelSelectorTabObserver to show IPH for TabGroups.
     * @param selector The selector that owns the Tabs that should be observed.
     */
    public static void startObservingForTabGroupsIPH(TabModelSelector selector) {
        if (sTabModelSelectorTabObserver != null) return;
        sTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
                if (!navigationHandle.isInMainFrame()) return;
                if (tab.isIncognito()) return;

                int coreTransitionType =
                        navigationHandle.pageTransition() & PageTransition.CORE_MASK;
                // Searching from omnibox results in PageTransition.GENERATED.
                if (navigationHandle.isValidSearchFormUrl()
                        || coreTransitionType == PageTransition.GENERATED) {
                    maybeShowIPH(FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE,
                            tab.getView());
                    sTabModelSelectorTabObserver.destroy();
                }
            }
        };
    }
}
