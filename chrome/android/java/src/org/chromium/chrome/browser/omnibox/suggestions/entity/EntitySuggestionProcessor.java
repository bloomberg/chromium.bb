// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.content.Context;

import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for the Entity suggestions. */
public class EntitySuggestionProcessor implements SuggestionProcessor {
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     */
    public EntitySuggestionProcessor(Context context, SuggestionHost suggestionHost) {
        mContext = context;
        mSuggestionHost = suggestionHost;
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.ENTITY_SUGGESTION;
    }

    @Override
    public PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion) {
        return new PropertyModel(EntitySuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void onNativeInitialized() {}

    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        // TODO(ender): Fetch entity icon URL.
        SuggestionViewDelegate delegate =
                mSuggestionHost.createSuggestionViewDelegate(suggestion, position);

        model.set(EntitySuggestionViewProperties.SUBJECT_TEXT, suggestion.getDisplayText());
        model.set(EntitySuggestionViewProperties.DESCRIPTION_TEXT, suggestion.getDescription());
        model.set(EntitySuggestionViewProperties.DELEGATE, delegate);
    }
}
