// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.registerCategory;

import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;

/**
 * Junit4 rule for tests testing suggestions on the Chrome Home bottom sheet.
 * @deprecated Use {@link BottomSheetTestRule} directly for general Chrome setup and
 * {@link SuggestionsDependenciesRule} for dependencies setup.
 */
@Deprecated
public class SuggestionsBottomSheetTestRule extends BottomSheetTestRule {
    /**
     * Initialises the test dependency factory with dummy data for the bottom sheet tests.
     * @deprecated To be removed or refactored (see http://crrev.com/c/541442/). Use
     * {@link SuggestionsDependenciesRule} directly in the meantime.
     */
    @Deprecated
    public void initDependencies(SuggestionsDependenciesRule.TestFactory testFactory) {
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        registerCategory(suggestionsSource, /* category = */ 42, /* count = */ 5);
        testFactory.suggestionsSource = suggestionsSource;

        testFactory.eventReporter = new DummySuggestionsEventReporter();
    }
}
