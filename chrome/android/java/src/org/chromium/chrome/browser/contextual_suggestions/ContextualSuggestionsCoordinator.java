// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.support.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegateImpl;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * The coordinator for the contextual suggestions UI component. Manages communication with other
 * parts of the UI-layer and lifecycle of shared component objects.
 *
 * This parent coordinator manages two sub-components, controlled by {@link ContentCoordinator}
 * and {@link ToolbarCoordinator}. These sub-components each have their own views and view binders.
 * They share a {@link ContextualSuggestionsMediator} and {@link ContextualSuggestionsModel}.
 */
public class ContextualSuggestionsCoordinator {
    private final ChromeActivity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final TabModelSelector mTabModelSelector;
    private final Profile mProfile;
    private final ContextualSuggestionsModel mModel;
    private final ContextualSuggestionsMediator mMediator;

    private @Nullable ToolbarCoordinator mToolbarCoordinator;
    private @Nullable ContentCoordinator mContentCoordinator;
    private @Nullable ContextualSuggestionsBottomSheetContent mBottomSheetContent;

    /**
     * Construct a new {@link ContextualSuggestionsCoordinator}.
     * @param activity The containing {@link ChromeActivity}.
     * @param bottomSheetController The {@link BottomSheetController} to request content be shown.
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     */
    public ContextualSuggestionsCoordinator(ChromeActivity activity,
            BottomSheetController bottomSheetController, TabModelSelector tabModelSelector) {
        mActivity = activity;
        mBottomSheetController = bottomSheetController;
        mTabModelSelector = tabModelSelector;
        mProfile = Profile.getLastUsedProfile().getOriginalProfile();

        mModel = new ContextualSuggestionsModel();
        mMediator = new ContextualSuggestionsMediator(mActivity, mProfile, tabModelSelector,
                activity.getFullscreenManager(), this, mModel);
    }

    /** Called when the containing activity is destroyed. */
    public void destroy() {
        mMediator.destroy();

        if (mToolbarCoordinator != null) mToolbarCoordinator.destroy();
        if (mContentCoordinator != null) mContentCoordinator.destroy();
        if (mBottomSheetContent != null) mBottomSheetContent.destroy();
    }

    /**
     * Preload the contextual suggestions in the {@link BottomSheet}; content won't actually be
     * shown until {@link #showSuggestions()} is called.
     *
     * @param suggestionsSource The {@link SuggestionsSource} used to retrieve additional things
     *                          needed to display suggestions (e.g. favicons, thumbnails).
     */
    void preloadSuggestionsInSheet(SuggestionsSource suggestionsSource) {
        SuggestionsNavigationDelegate navigationDelegate = new SuggestionsNavigationDelegateImpl(
                mActivity, mProfile, mBottomSheetController.getBottomSheet(), mTabModelSelector);
        SuggestionsUiDelegateImpl uiDelegate =
                new SuggestionsUiDelegateImpl(suggestionsSource, new DummyEventReporter(),
                        navigationDelegate, mProfile, mBottomSheetController.getBottomSheet(),
                        mActivity.getChromeApplication().getReferencePool(),
                        mActivity.getSnackbarManager());

        // TODO(twellington): Introduce another method that creates bottom sheet content with only
        // a toolbar view when suggestions are fist available, and use this method to construct the
        // content view when the sheet is opened.
        mToolbarCoordinator =
                new ToolbarCoordinator(mActivity, mBottomSheetController.getBottomSheet(), mModel);
        mContentCoordinator = new ContentCoordinator(mActivity,
                mBottomSheetController.getBottomSheet(), mProfile, uiDelegate, mModel,
                mActivity.getWindowAndroid(), mActivity::closeContextMenu);
        mBottomSheetContent = new ContextualSuggestionsBottomSheetContent(
                mContentCoordinator, mToolbarCoordinator);
        // TODO(twellington): Handle the case where preload returns false.
        mBottomSheetController.requestContentPreload(mBottomSheetContent);
    }

    /**
     * Show the contextual suggestions in the {@link BottomSheet}.
     * {@link #preloadSuggestionsInSheet()} must be called prior to calling this method.
     */
    void showSuggestions() {
        // #preloadSuggestionsInSheet will always be called before this method, regardless of
        // whether content was actually put in the sheet (meaning mBottomSheetContent should never
        // be null). If content is not successfully preloaded
        // BottomSheetController#requestContentPreload will return false.
        assert mBottomSheetContent != null;
        mBottomSheetController.requestShowContent(mBottomSheetContent, false);
    }

    /** Removes contextual suggestions from the {@link BottomSheet}. */
    void removeSuggestions() {
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
            mToolbarCoordinator = null;
        }

        if (mContentCoordinator != null) {
            mContentCoordinator.destroy();
            mContentCoordinator = null;
        }

        if (mBottomSheetContent != null) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
            mBottomSheetContent.destroy();
            mBottomSheetContent = null;
        }
    }
}
