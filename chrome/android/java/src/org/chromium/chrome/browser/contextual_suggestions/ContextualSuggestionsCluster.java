// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

import java.util.ArrayList;
import java.util.List;

/** Class to group contextual suggestions in a cluser of related items. */
public class ContextualSuggestionsCluster {
    private final String mTitle;
    private final List<SnippetArticle> mSuggestions = new ArrayList<>();

    /** Creates a new contextual suggestions cluster with provided title. */
    public ContextualSuggestionsCluster(String title) {
        mTitle = title;
    }

    /** @return a title related to this cluster */
    public String getTitle() {
        return mTitle;
    }

    /** @return a list of suggestions in this cluster */
    public List<SnippetArticle> getSuggestions() {
        return mSuggestions;
    }
}
