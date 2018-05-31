// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Context;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Implementation of the {@link LocationBarLayout} that is displayed for widget searches. */
public class SearchActivityLocationBarLayout extends LocationBarLayout {
    /** Delegates calls out to the containing Activity. */
    public static interface Delegate {
        /** Load a URL in the associated tab. */
        void loadUrl(String url);

        /** The user hit the back button. */
        void backKeyPressed();
    }

    private Delegate mDelegate;
    private boolean mPendingSearchPromoDecision;
    private boolean mPendingBeginQuery;

    public SearchActivityLocationBarLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.layout.location_bar_base);
        setUrlBarFocusable(true);
        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
    }

    /** Set the {@link Delegate}. */
    void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    protected void loadUrl(String url, int transition) {
        mDelegate.loadUrl(url);
        LocaleManager.getInstance().recordLocaleBasedSearchMetrics(true, url, transition);
    }

    @Override
    public void backKeyPressed() {
        mDelegate.backKeyPressed();
    }

    @Override
    public boolean mustQueryUrlBarLocationForSuggestions() {
        return true;
    }

    @Override
    public void setUrlToPageUrl() {
        // Explicitly do nothing.  The tab is invisible, so showing its URL would be confusing.
    }

    @Override
    public void initializeControls(WindowDelegate windowDelegate, WindowAndroid windowAndroid) {
        super.initializeControls(windowDelegate, windowAndroid);
        setShowCachedZeroSuggestResults(true);
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        setAutocompleteProfile(Profile.getLastUsedProfile().getOriginalProfile());

        mPendingSearchPromoDecision = LocaleManager.getInstance().needToCheckForSearchEnginePromo();
    }

    @Override
    public void onSuggestionsReceived(
            List<OmniboxSuggestion> newSuggestions, String inlineAutocompleteText) {
        if (mPendingSearchPromoDecision) return;
        super.onSuggestionsReceived(newSuggestions, inlineAutocompleteText);
    }

    /** Called when the SearchActivity has finished initialization. */
    void onDeferredStartup(boolean isVoiceSearchIntent) {
        if (mVoiceRecognitionHandler != null) {
            SearchWidgetProvider.updateCachedVoiceSearchAvailability(
                    mVoiceRecognitionHandler.isVoiceSearchEnabled());
        }
        if (isVoiceSearchIntent && mUrlBar.isFocused()) onUrlFocusChange(true);
        if (!TextUtils.isEmpty(mUrlBar.getText())) onTextChangedForAutocomplete();

        assert !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        mPendingSearchPromoDecision = false;

        if (mPendingBeginQuery) {
            beginQueryInternal(isVoiceSearchIntent);
            mPendingBeginQuery = false;
        }
    }

    /**
     * Begins a new query.
     * @param isVoiceSearchIntent Whether this is a voice search.
     * @param optionalText Prepopulate with a query, this may be null.
     * */
    void beginQuery(boolean isVoiceSearchIntent, @Nullable String optionalText) {
        // Clear the text regardless of the promo decision.  This allows the user to enter text
        // before native has been initialized and have it not be cleared one the delayed beginQuery
        // logic is performed.
        mUrlBar.setIgnoreTextChangesForAutocomplete(true);
        mUrlBar.setUrl(UrlBarData.forNonUrlText(optionalText == null ? "" : optionalText));
        mUrlBar.setIgnoreTextChangesForAutocomplete(false);

        mUrlBar.setCursorVisible(true);
        mUrlBar.setSelection(0, mUrlBar.getText().length());

        if (mPendingSearchPromoDecision) {
            mPendingBeginQuery = true;
            return;
        }

        beginQueryInternal(isVoiceSearchIntent);
    }

    private void beginQueryInternal(boolean isVoiceSearchIntent) {
        assert !mPendingSearchPromoDecision;

        if (mVoiceRecognitionHandler != null && mVoiceRecognitionHandler.isVoiceSearchEnabled()
                && isVoiceSearchIntent) {
            mVoiceRecognitionHandler.startVoiceRecognition(
                    LocationBarVoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);
        } else {
            focusTextBox();
        }
    }

    @Override
    protected void updateButtonVisibility() {
        super.updateButtonVisibility();
        updateMicButtonVisibility(1.0f);
        findViewById(R.id.url_action_container).setVisibility(View.VISIBLE);
    }

    private void focusTextBox() {
        if (!mUrlBar.hasFocus()) mUrlBar.requestFocus();

        new Handler().post(new Runnable() {
            @Override
            public void run() {
                UiUtils.showKeyboard(mUrlBar);
            }
        });
    }

    @Override
    public boolean useModernDesign() {
        return false;
    }
}
