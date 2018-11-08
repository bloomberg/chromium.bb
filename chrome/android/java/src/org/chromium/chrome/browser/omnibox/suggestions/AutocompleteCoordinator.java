// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ListView;

import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsList.OmniboxSuggestionListEmbedder;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionListViewBinder.SuggestionListViewHolder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * Coordinator that handles the interactions with the autocomplete system.
 */
public class AutocompleteCoordinator implements UrlFocusChangeListener, UrlTextChangeListener {
    private final ViewGroup mParent;
    private final AutocompleteMediator mMediator;
    private final SuggestionListViewHolder mListViewHolder;

    private final OmniboxResultsAdapter mSuggestionListAdapter;

    /**
     * Provides the additional functionality to trigger and interact with autocomplete suggestions.
     */
    public interface AutocompleteDelegate {
        /**
         * Notified that the URL text has changed.
         */
        void onUrlTextChanged();

        /**
         * Notified that suggestions have changed.
         * @param autocompleteText The inline autocomplete text that can be appended to the
         *                         currently entered user text.
         */
        void onSuggestionsChanged(String autocompleteText);

        /**
         * Notified that the suggestions have been hidden.
         */
        void onSuggestionsHidden();

        /**
         * Requests the keyboard be hidden.
         */
        void hideKeyboard();

        /**
         * Requests that the given URL be loaded in the current tab.
         *
         * @param url The URL to be loaded.
         * @param transition The transition type associated with the url load.
         * @param inputStart The time the input started for the load request.
         */
        void loadUrl(String url, @PageTransition int transition, long inputStart);

        /**
         * Requests that the specified text be set as the current editing text in the omnibox.
         */
        void setOmniboxEditingText(String text);

        /**
         * @return Whether the omnibox was focused via the NTP fakebox.
         */
        boolean didFocusUrlFromFakebox();

        /**
         * @return Whether the URL currently has focus.
         */
        boolean isUrlBarFocused();
    }

    /**
     * Constructs a coordinator for the autocomplete system.
     *
     * @param parent The UI parent component for the autocomplete UI.
     * @param delegate The delegate to fulfill additional autocomplete requirements.
     * @param listEmbedder The embedder for controlling the display constraints of the suggestions
     *                     list.
     * @param urlBarEditingTextProvider Provider of editing text state from the UrlBar.
     */
    public AutocompleteCoordinator(ViewGroup parent, AutocompleteDelegate delegate,
            OmniboxSuggestionListEmbedder listEmbedder,
            UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
        mParent = parent;
        Context context = parent.getContext();

        mSuggestionListAdapter = new OmniboxResultsAdapter(context);

        Supplier<ViewStub> containerStubSupplier = () -> {
            return (ViewStub) mParent.getRootView().findViewById(
                    R.id.omnibox_results_container_stub);
        };
        mListViewHolder = new SuggestionListViewHolder(
                context, containerStubSupplier, mSuggestionListAdapter);
        PropertyModel listModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        // TODO(tedchoc): Investigate replacing with LazyConstructionPropertyMcp to simplify the
        //                view binder handling property updates when the view hasn't been created
        //                yet.
        PropertyModelChangeProcessor.create(
                listModel, mListViewHolder, SuggestionListViewBinder::bind);
        listModel.set(SuggestionListProperties.EMBEDDER, listEmbedder);

        mMediator =
                new AutocompleteMediator(context, delegate, urlBarEditingTextProvider, listModel);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mMediator.onUrlFocusChange(hasFocus);
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        mMediator.onUrlAnimationFinished(hasFocus);
    }

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mMediator.setToolbarDataProvider(toolbarDataProvider);
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    public void setAutocompleteProfile(Profile profile) {
        mMediator.setAutocompleteProfile(profile);
    }

    /**
     * Whether omnibox autocomplete should currently be prevented from generating suggestions.
     */
    public void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mMediator.setShouldPreventOmniboxAutocomplete(prevent);
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mMediator.getSuggestionCount();
    }

    /**
     * Retrieve the omnibox suggestion at the specified index.  The index represents the ordering
     * in the underlying model.  The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public OmniboxSuggestion getSuggestionAt(int index) {
        return mMediator.getSuggestionAt(index);
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mMediator.onNativeInitialized();
    }

    /**
     * @return The suggestion list popup containing the omnibox results (or null if it has not yet
     *         been created).
     */
    @VisibleForTesting
    public OmniboxSuggestionsList getSuggestionList() {
        return mListViewHolder.mListView;
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    public void onVoiceResults(
            @Nullable List<LocationBarVoiceRecognitionHandler.VoiceResult> results) {
        mMediator.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results.
     */
    // TODO(tedchoc): Figure out how to remove this.
    public long getCurrentNativeAutocompleteResult() {
        return mMediator.getCurrentNativeAutocompleteResult();
    }

    /**
     * Update the layout direction of the suggestion list based on the parent layout direction.
     */
    public void updateSuggestionListLayoutDirection() {
        mMediator.setLayoutDirection(ViewCompat.getLayoutDirection(mParent));
    }

    /**
     * Update the visuals of the autocomplete UI.
     * @param useDarkColors Whether dark colors should be applied to the UI.
     */
    public void updateVisualsForState(boolean useDarkColors) {
        mMediator.setUseDarkColors(useDarkColors);
    }

    /**
     * Sets to show cached zero suggest results. This will start both caching zero suggest results
     * in shared preferences and also attempt to show them when appropriate without needing native
     * initialization.
     * @param showCachedZeroSuggestResults Whether cached zero suggest should be shown.
     */
    public void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults) {
        mMediator.setShowCachedZeroSuggestResults(showCachedZeroSuggestResults);
    }

    /**
     * Handle the key events associated with the suggestion list.
     *
     * @param keyCode The keycode representing what key was interacted with.
     * @param event The key event containing all meta-data associated with the event.
     * @return Whether the key event was handled.
     */
    public boolean handleKeyEvent(int keyCode, KeyEvent event) {
        OmniboxSuggestionsList suggestionList = mListViewHolder.mListView;
        if (KeyNavigationUtil.isGoDown(event) && suggestionList != null
                && suggestionList.isShown()) {
            int suggestionCount = mSuggestionListAdapter.getCount();
            if (suggestionList.getSelectedItemPosition() < suggestionCount - 1) {
                if (suggestionCount > 0) mMediator.allowPendingItemSelection();
            } else {
                // Do not pass down events when the last item is already selected as it will
                // dismiss the suggestion list.
                return true;
            }

            if (suggestionList.getSelectedItemPosition() == ListView.INVALID_POSITION) {
                // When clearing the selection after a text change, state is not reset
                // correctly so hitting down again will cause it to start from the previous
                // selection point. We still have to send the key down event to let the list
                // view items take focus, but then we select the first item explicitly.
                boolean result = suggestionList.onKeyDown(keyCode, event);
                suggestionList.setSelection(0);
                return result;
            } else {
                return suggestionList.onKeyDown(keyCode, event);
            }
        } else if (KeyNavigationUtil.isGoUp(event) && suggestionList != null
                && suggestionList.isShown()) {
            if (suggestionList.getSelectedItemPosition() != 0
                    && mSuggestionListAdapter.getCount() > 0) {
                mMediator.allowPendingItemSelection();
            }
            return suggestionList.onKeyDown(keyCode, event);
        } else if (KeyNavigationUtil.isGoRight(event) && suggestionList != null
                && suggestionList.isShown()
                && suggestionList.getSelectedItemPosition() != ListView.INVALID_POSITION) {
            mMediator.onSetUrlToSuggestion(
                    mMediator.getSuggestionAt(suggestionList.getSelectedItemPosition()));
            onTextChangedForAutocomplete();
            suggestionList.setSelection(0);
            return true;
        } else if (KeyNavigationUtil.isEnter(event) && mParent.getVisibility() == View.VISIBLE) {
            int selectedItemPosition = ListView.INVALID_POSITION;
            if (suggestionList != null && suggestionList.isShown()) {
                selectedItemPosition = suggestionList.getSelectedItemPosition();
            }
            mMediator.loadSuggestionAtIndex(selectedItemPosition, event.getEventTime());
            return true;
        }
        return false;
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     */
    @Override
    public void onTextChangedForAutocomplete() {
        if (!mParent.isInTouchMode() && mListViewHolder.mListView != null) {
            mListViewHolder.mListView.setSelection(0);
        }
        mMediator.onTextChangedForAutocomplete();
    }

    /**
     * Cancels the queued task to start the autocomplete controller, if any.
     */
    @VisibleForTesting
    public void cancelPendingAutocompleteStart() {
        mMediator.cancelPendingAutocompleteStart();
    }

    /**
     * Trigger autocomplete for the given query.
     */
    public void startAutocompleteForQuery(String query) {
        mMediator.startAutocompleteForQuery(query);
    }

    /**
     * Sets the autocomplete controller for the location bar.
     *
     * @param controller The controller that will handle autocomplete/omnibox suggestions.
     * @note Only used for testing.
     */
    @VisibleForTesting
    public void setAutocompleteController(AutocompleteController controller) {
        mMediator.setAutocompleteController(controller);
    }

    /** Allows injecting autocomplete suggestions for testing. */
    @VisibleForTesting
    public OnSuggestionsReceivedListener getSuggestionsReceivedListenerForTest() {
        return mMediator;
    }
}
