// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.share.ShareMenuActionHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class controls the interaction of the "edit url" suggestion item with the rest of the
 * suggestions list. This class also serves as a mediator, containing logic that interacts with
 * the rest of Chrome.
 */
public class EditUrlSuggestionCoordinator implements OnClickListener {
    /** An interface for handling taps on the suggestion rather than its buttons. */
    public interface SuggestionSelectionHandler {
        /**
         * Handle the edit URL suggestion selection.
         * @param suggestion The selected suggestion.
         */
        void onEditUrlSuggestionSelected(OmniboxSuggestion suggestion);
    }

    /** An interface for modifying the location bar and it's contents. */
    public interface LocationBarDelegate {
        /** Remove focus from the omnibox. */
        void clearOmniboxFocus();

        /**
         * Set the text in the omnibox.
         * @param text The text that should be displayed in the omnibox.
         */
        void setOmniboxEditingText(String text);
    }

    /** The name of the parameter for getting the experiment variation. */
    private static final String FIELD_TRIAL_PARAM_NAME = "variation";

    /** The name of the experiment variation that shows the copy icon. */
    private static final String COPY_ICON_VARIATION_NAME = "copy_icon";

    /** The suggestion's model. */
    private final PropertyModel mModel;

    /** The delegate for accessing the location bar for observation and modification. */
    private final LocationBarDelegate mLocationBarDelegate;

    /** A means of accessing the activity's tab. */
    private ActivityTabProvider mTabProvider;

    /** The suggestion view. */
    private ViewGroup mView;

    /** Whether the omnibox has already cleared its content for the focus event. */
    private boolean mHasClearedOmniboxForFocus;

    /** The last omnibox suggestion to be processed. */
    private OmniboxSuggestion mLastProcessedSuggestion;

    /** A handler for suggestion selection. */
    private SuggestionSelectionHandler mSelectionHandler;

    /**
     * @param tabProvider A means of accessing the active tab.
     * @param locationBarDelegate A means of modifying the location bar.
     * @param selectionHandler A mechanism for handling selection of the edit URL suggestion item.
     */
    public EditUrlSuggestionCoordinator(ActivityTabProvider tabProvider,
            LocationBarDelegate locationBarDelegate,
            SuggestionSelectionHandler selectionHandler) {
        mTabProvider = tabProvider;
        mLocationBarDelegate = locationBarDelegate;
        mModel = new PropertyModel(EditUrlSuggestionProperties.ALL_KEYS);
        mSelectionHandler = selectionHandler;
    }

    /**
     * Handle a specific omnibox suggestion type and determine whether a custom suggestion item
     * should be shown.
     * @param suggestion The omnibox suggestion being processed.
     * @return Whether the suggestion item should be shown.
     */
    public boolean maybeReplaceOmniboxSuggestion(OmniboxSuggestion suggestion) {
        Tab activeTab = mTabProvider != null ? mTabProvider.getActivityTab() : null;
        if (OmniboxSuggestionType.URL_WHAT_YOU_TYPED != suggestion.getType() || activeTab == null
                || activeTab.isIncognito()) {
            return false;
        }
        mLastProcessedSuggestion = suggestion;
        if (!mHasClearedOmniboxForFocus) {
            mHasClearedOmniboxForFocus = true;
            mLocationBarDelegate.setOmniboxEditingText("");
        }
        updateUrlDisplayInfo();
        return true;
    }

    /**
     * @param provider A means of getting the activity's tab.
     */
    public void setActivityTabProvider(ActivityTabProvider provider) {
        mTabProvider = provider;
    }

    /**
     * @return The view to insert into the list.
     */
    public ViewGroup getView() {
        return mView;
    }

    /**
     * @return The model for the view.
     */
    public PropertyModel getModel() {
        return mModel;
    }

    /**
     * Clean up any state that this coordinator has.
     */
    public void destroy() {
        mLastProcessedSuggestion = null;
        mSelectionHandler = null;
    }

    /**
     * @return The experiment variation for the Search Ready Omnibox.
     */
    private static String getSearchReadyOmniboxVariation() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.SEARCH_READY_OMNIBOX, FIELD_TRIAL_PARAM_NAME);
    }

    /**
     * A notification that the omnibox focus state has changed.
     * @param hasFocus Whether the omnibox has focus.
     */
    public void onUrlFocusChange(boolean hasFocus) {
        if (!hasFocus) {
            mHasClearedOmniboxForFocus = false;
            mLastProcessedSuggestion = null;
            mView = null;
            return;
        }

        // When the omnibox is focused, create a new version of the suggestion item.
        LayoutInflater inflater = mTabProvider.getActivityTab().getActivity().getLayoutInflater();
        mView = (ViewGroup) inflater.inflate(R.layout.edit_url_suggestion_layout, null);
        mModel.set(EditUrlSuggestionProperties.TEXT_CLICK_LISTENER, this);

        // Check which variation of the experiment is being run.
        String variation = getSearchReadyOmniboxVariation();
        if (COPY_ICON_VARIATION_NAME.equals(variation)) {
            mModel.set(EditUrlSuggestionProperties.COPY_ICON_VISIBLE, true);
            mModel.set(EditUrlSuggestionProperties.SHARE_ICON_VISIBLE, false);
        } else {
            mModel.set(EditUrlSuggestionProperties.COPY_ICON_VISIBLE, false);
            mModel.set(EditUrlSuggestionProperties.SHARE_ICON_VISIBLE, true);
        }
        mModel.set(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER, this);
    }

    /**
     * Update the URL info displayed in this view.
     */
    private void updateUrlDisplayInfo() {
        Tab tab = mTabProvider.getActivityTab();
        if (tab == null || mLastProcessedSuggestion == null) return;

        // Only update the title if the displayed URL matches the tab's URL.
        if (TextUtils.equals(tab.getUrl(), mLastProcessedSuggestion.getUrl())) {
            mModel.set(EditUrlSuggestionProperties.TITLE_TEXT, tab.getTitle());
        } else {
            mModel.set(EditUrlSuggestionProperties.TITLE_TEXT, mLastProcessedSuggestion.getUrl());
        }
        mModel.set(EditUrlSuggestionProperties.URL_TEXT, mLastProcessedSuggestion.getUrl());
    }

    @Override
    public void onClick(View view) {
        Tab activityTab = mTabProvider.getActivityTab();
        assert activityTab != null : "A tab is required to make changes to the location bar.";

        if (R.id.url_copy_icon == view.getId()) {
            Clipboard.getInstance().copyUrlToClipboard(mLastProcessedSuggestion.getUrl());
        } else if (R.id.url_share_icon == view.getId()) {
            mLocationBarDelegate.clearOmniboxFocus();
            // TODO(mdjones): This should only share the displayed URL instead of the background
            //                tab.
            ShareMenuActionHandler.getInstance().onShareMenuItemSelected(
                    activityTab.getActivity(), activityTab, false, activityTab.isIncognito());
        } else if (R.id.url_edit_icon == view.getId()) {
            mLocationBarDelegate.setOmniboxEditingText(mLastProcessedSuggestion.getUrl());
        } else {
            // If the event wasn't on any of the buttons, treat is as a tap on the general
            // suggestion.
            if (mSelectionHandler != null) {
                mSelectionHandler.onEditUrlSuggestionSelected(mLastProcessedSuggestion);
            }
        }
    }
}
