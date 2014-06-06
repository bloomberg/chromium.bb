// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.TextUtils;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to the native AutocompleteControllerAndroid.
 */
public class AutocompleteController {
    private long mNativeAutocompleteControllerAndroid;
    private long mCurrentNativeAutocompleteResult;
    private final OnSuggestionsReceivedListener mListener;

    /**
     * Listener for receiving OmniboxSuggestions.
     */
    public static interface OnSuggestionsReceivedListener {
        void onSuggestionsReceived(List<OmniboxSuggestion> suggestions,
                String inlineAutocompleteText);
    }

    public AutocompleteController(OnSuggestionsReceivedListener listener) {
        this(null, listener);
    }

    public AutocompleteController(Profile profile, OnSuggestionsReceivedListener listener) {
        if (profile != null) {
            mNativeAutocompleteControllerAndroid = nativeInit(profile);
        }
        mListener = listener;
    }

    /**
     * Resets the underlying autocomplete controller based on the specified profile.
     *
     * <p>This will implicitly stop the autocomplete suggestions, so
     * {@link #start(Profile, String, String, boolean)} must be called again to start them flowing
     * again.  This should not be an issue as changing profiles should not normally occur while
     * waiting on omnibox suggestions.
     *
     * @param profile The profile to reset the AutocompleteController with.
     */
    public void setProfile(Profile profile) {
        stop(true);
        if (profile == null) {
            mNativeAutocompleteControllerAndroid = 0;
            return;
        }

        mNativeAutocompleteControllerAndroid = nativeInit(profile);
    }

    /**
     * Starts querying for omnibox suggestions for a given text.
     *
     * @param profile The profile to use for starting the AutocompleteController
     * @param url The URL of the current tab, used to suggest query refinements.
     * @param text The text to query autocomplete suggestions for.
     * @param preventInlineAutocomplete Whether autocomplete suggestions should be prevented.
     */
    public void start(Profile profile, String url, String text, boolean preventInlineAutocomplete) {
        if (profile == null || TextUtils.isEmpty(url)) return;

        mNativeAutocompleteControllerAndroid = nativeInit(profile);
        // Initializing the native counterpart might still fail.
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeStart(mNativeAutocompleteControllerAndroid, text, null, url,
                    preventInlineAutocomplete, false, false, true);
        }
    }

    /**
     * Given some string |text| that the user wants to use for navigation, determines how it should
     * be interpreted. This is a fallback in case the user didn't select a visible suggestion (e.g.
     * the user pressed enter before omnibox suggestions had been shown).
     *
     * Note: this updates the internal state of the autocomplete controller just as start() does.
     * Future calls that reference autocomplete results by index, e.g. onSuggestionSelected(),
     * should reference the returned suggestion by index 0.
     *
     * @param text The user's input text to classify (i.e. what they typed in the omnibox)
     * @return The OmniboxSuggestion specifying where to navigate, the transition type, etc. May
     *         be null if the input is invalid.
     */
    public OmniboxSuggestion classify(String text) {
        return nativeClassify(mNativeAutocompleteControllerAndroid, text);
    }

    /**
     * Starts a query for suggestions before any input is available from the user.
     *
     * @param profile The profile to use for starting the AutocompleteController.
     * @param omniboxText The text displayed in the omnibox.
     * @param url The url of the currently loaded web page.
     * @param isQueryInOmnibox Whether the location bar is currently showing a search query.
     * @param focusedFromFakebox Whether the user entered the omnibox by tapping the fakebox on the
     *                           native NTP. This should be false on all other pages.
     */
    public void startZeroSuggest(Profile profile, String omniboxText, String url,
            boolean isQueryInOmnibox, boolean focusedFromFakebox) {
        if (profile == null || TextUtils.isEmpty(url)) return;
        mNativeAutocompleteControllerAndroid = nativeInit(profile);
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeStartZeroSuggest(mNativeAutocompleteControllerAndroid, omniboxText, url,
                    isQueryInOmnibox, focusedFromFakebox);
        }
    }

    /**
     * Stops generating autocomplete suggestions for the currently specified text from
     * {@link #start(Profile,String, String, boolean)}.
     *
     * <p>
     * Calling this method with {@code false}, will result in
     * {@link #onSuggestionsReceived(List, String, long)} being called with an empty
     * result set.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    public void stop(boolean clear) {
        mCurrentNativeAutocompleteResult = 0;
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeStop(mNativeAutocompleteControllerAndroid, clear);
        }
    }

    /**
     * Resets session for autocomplete controller. This happens every time we start typing
     * new input into the omnibox.
     */
    public void resetSession() {
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeResetSession(mNativeAutocompleteControllerAndroid);
        }
    }

    /**
     * Deletes an omnibox suggestion, if possible.
     * @param position The position at which the suggestion is located.
     */
    public void deleteSuggestion(int position) {
        if (mNativeAutocompleteControllerAndroid != 0) {
            nativeDeleteSuggestion(mNativeAutocompleteControllerAndroid, position);
        }
    }

    /**
     * @return Native pointer to current autocomplete results.
     */
    @VisibleForTesting
    public long getCurrentNativeAutocompleteResult() {
        return mCurrentNativeAutocompleteResult;
    }

    @CalledByNative
    protected void onSuggestionsReceived(
            List<OmniboxSuggestion> suggestions,
            String inlineAutocompleteText,
            long currentNativeAutocompleteResult) {
        mCurrentNativeAutocompleteResult = currentNativeAutocompleteResult;

        // Notify callbacks of suggestions.
        mListener.onSuggestionsReceived(suggestions, inlineAutocompleteText);
    }

    @CalledByNative
    private void notifyNativeDestroyed() {
        mNativeAutocompleteControllerAndroid = 0;
    }

    /**
     * Called whenever a navigation happens from the omnibox to record metrics about the user's
     * interaction with the omnibox.
     *
     * @param selectedIndex The index of the suggestion that was selected.
     * @param type The type of the selected suggestion.
     * @param currentPageUrl The URL of the current page.
     * @param focusedFromFakebox Whether the user entered the omnibox by tapping the fakebox on the
     *                           native NTP. This should be false on all other pages.
     * @param elapsedTimeSinceModified The number of ms that passed between the user first
     *                                 modifying text in the omnibox and selecting a suggestion.
     * @param webContents The web contents for the tab where the selected suggestion will be shown.
     */
    protected void onSuggestionSelected(int selectedIndex, OmniboxSuggestion.Type type,
            String currentPageUrl, boolean isQueryInOmnibox, boolean focusedFromFakebox,
            long elapsedTimeSinceModified, WebContents webContents) {
        nativeOnSuggestionSelected(mNativeAutocompleteControllerAndroid, selectedIndex,
                currentPageUrl, isQueryInOmnibox, focusedFromFakebox, elapsedTimeSinceModified,
                webContents);
    }

    @CalledByNative
    private static List<OmniboxSuggestion> createOmniboxSuggestionList(int size) {
        return new ArrayList<OmniboxSuggestion>(size);
    }

    @CalledByNative
    private static void addOmniboxSuggestionToList(List<OmniboxSuggestion> suggestionList,
            OmniboxSuggestion suggestion) {
        suggestionList.add(suggestion);
    }

    @CalledByNative
    private static OmniboxSuggestion buildOmniboxSuggestion(int nativeType, int relevance,
            int transition, String text, String description, String answerContents,
            String answerType, String fillIntoEdit, String url, String formattedUrl,
            boolean isStarred, boolean isDeletable) {
        return new OmniboxSuggestion(nativeType, relevance, transition, text, description,
                answerContents, answerType, fillIntoEdit, url, formattedUrl, isStarred,
                isDeletable);
    }

    /**
     * Updates aqs parameters on the selected match that we will navigate to and returns the
     * updated URL. |selected_index| is the position of the selected match and
     * |elapsed_time_since_input_change| is the time in ms between the first typed input and match
     * selection.
     *
     * @param selectedIndex The index of the autocomplete entry selected.
     * @param elapsedTimeSinceInputChange The number of ms between the time the user started
     *                                    typing in the omnibox and the time the user has selected
     *                                    a suggestion.
     * @return The url to navigate to for this match with aqs parameter updated, if we are
     *         making a Google search query.
     */
    public String updateMatchDestinationUrl(int selectedIndex, long elapsedTimeSinceInputChange) {
        return nativeUpdateMatchDestinationURL(mNativeAutocompleteControllerAndroid, selectedIndex,
                elapsedTimeSinceInputChange);
    }

    /**
     * @param query User input text.
     * @return The top synchronous suggestion from the autocomplete controller.
     */
    public OmniboxSuggestion getTopSynchronousMatch(String query) {
        return nativeGetTopSynchronousMatch(mNativeAutocompleteControllerAndroid, query);
    }

    @VisibleForTesting
    protected native long nativeInit(Profile profile);
    private native void nativeStart(long nativeAutocompleteControllerAndroid, String text,
            String desiredTld, String currentUrl, boolean preventInlineAutocomplete,
            boolean preferKeyword, boolean allowExactKeywordMatch, boolean wantAsynchronousMatches);
    private native OmniboxSuggestion nativeClassify(long nativeAutocompleteControllerAndroid,
            String text);
    private native void nativeStop(long nativeAutocompleteControllerAndroid, boolean clearResults);
    private native void nativeResetSession(long nativeAutocompleteControllerAndroid);
    private native void nativeOnSuggestionSelected(long nativeAutocompleteControllerAndroid,
            int selectedIndex, String currentPageUrl, boolean isQueryInOmnibox,
            boolean focusedFromFakebox, long elapsedTimeSinceModified, WebContents webContents);
    private native void nativeStartZeroSuggest(long nativeAutocompleteControllerAndroid,
            String omniboxText, String currentUrl, boolean isQueryInOmnibox,
            boolean focusedFromFakebox);
    private native void nativeDeleteSuggestion(long nativeAutocompleteControllerAndroid,
            int selectedIndex);
    private native String nativeUpdateMatchDestinationURL(long nativeAutocompleteControllerAndroid,
            int selectedIndex, long elapsedTimeSinceInputChange);
    private native OmniboxSuggestion nativeGetTopSynchronousMatch(
            long nativeAutocompleteControllerAndroid, String query);

    /**
     * Given a search query, this will attempt to see if the query appears to be portion of a
     * properly formed URL.  If it appears to be a URL, this will return the fully qualified
     * version (i.e. including the scheme, etc...).  If the query does not appear to be a URL,
     * this will return null.
     *
     * @param query The query to be expanded into a fully qualified URL if appropriate.
     * @return The fully qualified URL or null.
     */
    public static native String nativeQualifyPartialURLQuery(String query);

    /**
     * Sends a zero suggest request to the server in order to pre-populate the result cache.
     */
    public static native void nativePrefetchZeroSuggestResults();
}
