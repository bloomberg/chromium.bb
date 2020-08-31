// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.suggestions;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.thumbnail.generator.ThumbnailProvider;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;

/**
 * Rule that allows mocking native dependencies of the suggestions package.
 *
 * The Factory members to override should be set before the main test rule is called to initialise
 * the test activity.
 *
 * @see SuggestionsDependencyFactory
 */
public class SuggestionsDependenciesRule extends TestWatcher {
    private TestFactory mFactory;

    public TestFactory getFactory() {
        return mFactory;
    }

    public SuggestionsDependenciesRule(TestFactory factory) {
        mFactory = factory;
    }

    public SuggestionsDependenciesRule() {
        this(new TestFactory());
    }

    @Override
    protected void starting(Description description) {
        SuggestionsDependencyFactory.setInstanceForTesting(mFactory);
    }

    @Override
    protected void finished(Description description) {
        SuggestionsDependencyFactory.setInstanceForTesting(null);
    }

    /**
     * SuggestionsDependencyFactory that exposes and allows modifying the instances to be injected.
     */
    public static class TestFactory extends SuggestionsDependencyFactory {
        public MostVisitedSites mostVisitedSites;
        public LargeIconBridge largeIconBridge;
        public SuggestionsEventReporter eventReporter;
        public ThumbnailProvider thumbnailProvider;
        public OfflinePageBridge offlinePageBridge;

        @Override
        public SuggestionsEventReporter createEventReporter() {
            if (eventReporter != null) return eventReporter;
            return super.createEventReporter();
        }

        @Override
        public MostVisitedSites createMostVisitedSites(Profile profile) {
            if (mostVisitedSites != null) return mostVisitedSites;
            return super.createMostVisitedSites(profile);
        }

        @Override
        public LargeIconBridge createLargeIconBridge(Profile profile) {
            if (largeIconBridge != null) return largeIconBridge;
            return new LargeIconBridge(profile);
        }

        @Override
        public OfflinePageBridge getOfflinePageBridge(Profile profile) {
            if (offlinePageBridge != null) return offlinePageBridge;
            return super.getOfflinePageBridge(profile);
        }
    }
}
