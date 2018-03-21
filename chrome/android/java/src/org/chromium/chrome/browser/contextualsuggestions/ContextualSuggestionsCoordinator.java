// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsuggestions;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegateImpl;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;

/**
 * The coordinator for the contextual suggestions UI component. Manages communication with other
 * parts of the UI-layer and lifecycle of shared component objects.
 *
 * This parent coordinator manages two sub-components, controlled by {@link ContentCoordinator}
 * and {@link ToolbarCoordinator}. These sub-components each have their own views and view binders.
 * They share a {@link ContextualSuggestionsMediator} and {@link ContextualSuggestionsModel}.
 */
public class ContextualSuggestionsCoordinator {
    private ChromeActivity mActivity;
    private BottomSheet mBottomSheet;
    private Profile mProfile;

    private ContextualSuggestionsModel mModel;
    private ContextualSuggestionsMediator mMediator;
    private ContentCoordinator mContentCoordinator;

    private SuggestionsUiDelegateImpl mUiDelegate;

    private ContextualSuggestionsBottomSheetContent mBottomSheetContent;

    /**
     * Construct a new {@link ContextualSuggestionsCoordinator}.
     * @param activity The containing {@link ChromeActivity}.
     * @param bottomSheet The {@link BottomSheet} where contextual suggestions will be displayed.
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     */
    public ContextualSuggestionsCoordinator(
            ChromeActivity activity, BottomSheet bottomSheet, TabModelSelector tabModelSelector) {
        mActivity = activity;
        mBottomSheet = bottomSheet;
        mProfile = Profile.getLastUsedProfile().getOriginalProfile();

        mModel = new ContextualSuggestionsModel();
        mMediator = new ContextualSuggestionsMediator(mProfile, tabModelSelector, this, mModel);

        SuggestionsSource suggestionsSource = mMediator.getSuggestionsSource();
        SuggestionsNavigationDelegate navigationDelegate = new SuggestionsNavigationDelegateImpl(
                mActivity, mProfile, mBottomSheet, tabModelSelector);
        mUiDelegate = new SuggestionsUiDelegateImpl(suggestionsSource, new DummyEventReporter(),
                navigationDelegate, mProfile, mBottomSheet,
                mActivity.getChromeApplication().getReferencePool(),
                mActivity.getSnackbarManager());
    }

    /** Called when the containing activity is destroyed. */
    public void destroy() {
        mMediator.destroy();

        if (mContentCoordinator != null) mContentCoordinator.destroy();
        if (mBottomSheetContent != null) mBottomSheetContent.destroy();
    }

    /**
     * Displays contextual suggestions in the {@link BottomSheet}.
     */
    void displaySuggestions() {
        // TODO(twellington): Introduce another method that creates bottom sheet content with only
        // a toolbar view when suggestions are fist available, and use this method to construct the
        // content view when the sheet is opened.
        mContentCoordinator =
                new ContentCoordinator(mActivity, mBottomSheet, mProfile, mUiDelegate, mModel);
        mBottomSheetContent = new ContextualSuggestionsBottomSheetContent(mContentCoordinator);
        mBottomSheet.showContent(mBottomSheetContent);
    }

    /** Removes contextual suggestions from the {@link BottomSheet}. */
    void removeSuggestions() {
        if (mContentCoordinator != null) {
            mContentCoordinator.destroy();
            mContentCoordinator = null;
        }

        if (mBottomSheetContent == null) return;

        mBottomSheet.showContent(null);

        mBottomSheetContent.destroy();
        mBottomSheetContent = null;
    }
}
