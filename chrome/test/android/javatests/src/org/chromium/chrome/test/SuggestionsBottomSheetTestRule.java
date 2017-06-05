// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.registerCategory;

import org.chromium.chrome.browser.suggestions.SuggestionsBottomSheetContent;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;

/**
 * Junit4 rule for tests testing suggestions on the Chrome Home bottom sheet.
 */
public class SuggestionsBottomSheetTestRule extends BottomSheetTestRule {
    @Override
    protected void beforeStartingActivity() {
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        registerCategory(suggestionsSource, /* category = */ 42, /* count = */ 5);
        SuggestionsBottomSheetContent.setSuggestionsSourceForTesting(suggestionsSource);
        SuggestionsBottomSheetContent.setEventReporterForTesting(
                new DummySuggestionsEventReporter());
        super.beforeStartingActivity();
    }

    @Override
    protected void afterActivityFinished() {
        super.afterActivityFinished();
        SuggestionsBottomSheetContent.setSuggestionsSourceForTesting(null);
        SuggestionsBottomSheetContent.setEventReporterForTesting(null);
    }
}
