// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.suggestions;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.browser.download.ui.ThumbnailProvider;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;

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
    // TODO(dgn): Warning emitted while all the exposed fields are not used, this should become
    // unnecessary as we start using the rule.
    @SuppressFBWarnings("UWF_UNWRITTEN_PUBLIC_OR_PROTECTED_FIELD")
    public static class TestFactory extends SuggestionsDependencyFactory {
        public SuggestionsSource suggestionsSource;
        public MostVisitedSites mostVisitedSites;
        public LargeIconBridge largeIconBridge;
        public SuggestionsEventReporter eventReporter;
        public ThumbnailProvider thumbnailProvider;
        public FaviconHelper faviconHelper;

        @Override
        public SuggestionsSource createSuggestionSource(Profile profile) {
            if (suggestionsSource != null) return suggestionsSource;
            return super.createSuggestionSource(profile);
        }

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
        public ThumbnailProvider createThumbnailProvider(DiscardableReferencePool referencePool) {
            if (thumbnailProvider != null) return thumbnailProvider;
            return super.createThumbnailProvider(referencePool);
        }

        @Override
        public FaviconHelper createFaviconHelper() {
            if (faviconHelper != null) return faviconHelper;
            return super.createFaviconHelper();
        }
    }
}
