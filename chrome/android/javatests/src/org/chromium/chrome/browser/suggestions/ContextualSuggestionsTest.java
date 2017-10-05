// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.ChromeHome;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Integration tests for Contextual suggestions.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG)
@CommandLineFlags.Remove(ChromeHome.ENABLE_FLAGS)
public class ContextualSuggestionsTest {
    private static final String ENABLE_CONTEXTUAL_SUGGESTIONS = ChromeHome.ENABLE_FLAGS + ","
            + ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL;
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    private static final List<SnippetArticle> FAKE_CONTEXTUAL_SUGGESTIONS = Arrays.asList(
            new SnippetArticle(KnownCategories.CONTEXTUAL, "suggestion0",
                    "James Roderick to step down as conductor for Laville orchestra",
                    "The Curious One", "http://example.com", 0, 0.0f, 0L, false, null),
            new SnippetArticle(KnownCategories.CONTEXTUAL, "suggestion1",
                    "Boy raises orphaned goat", "Meme feed", "http://example.com", 0, 0.0f, 0L,
                    false, null),
            new SnippetArticle(KnownCategories.CONTEXTUAL, "suggestion2", "Top gigs this week",
                    "Hello World", "http://example.com", 0, 0.0f, 0L, false, null));

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule
    public SuggestionsBottomSheetTestRule mActivityRule = new SuggestionsBottomSheetTestRule();
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();
    @Rule
    public RenderTestRule mRenderTestRule = new RenderTestRule();

    private FakeSuggestionsSource mSuggestionsSource;

    @Before
    public void setUp() throws Exception {
        mSuggestionsSource = new FakeSuggestionsSource();
        mSuggestionsSource.setThumbnailForId("suggestion0", "/android/UiCapture/conductor.jpg");
        mSuggestionsSource.setThumbnailForId("suggestion1", "/android/UiCapture/goat.jpg");
        mSuggestionsSource.setThumbnailForId("suggestion2", "/android/UiCapture/gig.jpg");

        mSuggestionsDeps.getFactory().suggestionsSource = mSuggestionsSource;

        mActivityRule.startMainActivityOnBottomSheet(BottomSheet.SHEET_STATE_PEEK);
    }

    @Test
    //@SmallTest
    @DisabledTest(message = "see crbug.com/758179")
    @Feature({"ContextualSuggestions"})
    @CommandLineFlags.Add(ChromeHome.ENABLE_FLAGS)
    public void testCarouselIsNotShownWhenFlagIsDisabled() throws IOException {
        Assert.assertTrue(FeatureUtilities.isChromeHomeEnabled());
        Assert.assertFalse(
                ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL));

        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        View carouselRecyclerView = getCarouselRecyclerView();
        Assert.assertNull(carouselRecyclerView);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "See crbug.com/758179. The fix there involves setting the"
                    + "SuggestionsCarousel to always be visible inside the NewTabPageAdapter."
                    + "However, the desired behaviour is that it is only visible when there are"
                    + "suggestions to show. This test covers the desired behaviour and should be"
                    + "enabled when this is handled properly.")
    @Feature({"ContextualSuggestions"})
    @CommandLineFlags.Add(ENABLE_CONTEXTUAL_SUGGESTIONS)
    public void testCarouselIsNotShownWhenFlagsEnabledButNoSuggestions() throws Exception {
        Assert.assertTrue(FeatureUtilities.isChromeHomeEnabled());
        Assert.assertTrue(
                ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL));

        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        View carouselRecyclerView = getCarouselRecyclerView();
        Assert.assertNull(carouselRecyclerView);
    }

    @Test
    //@SmallTest
    @DisabledTest(message = "see crbug.com/758179")
    @Feature({"ContextualSuggestions"})
    @CommandLineFlags.Add(ENABLE_CONTEXTUAL_SUGGESTIONS)
    public void testCarouselIsShownWhenItReceivedSuggestions() throws Exception {
        mSuggestionsSource.setContextualSuggestions(FAKE_CONTEXTUAL_SUGGESTIONS);

        String testUrl = mTestServerRule.getServer().getURL(TEST_PAGE);
        mActivityRule.loadUrl(testUrl);
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mActivityRule.getObserver().mOpenedCallbackHelper.waitForCallback(0);

        View carouselRecyclerView = getCarouselRecyclerView();
        Assert.assertNotNull(carouselRecyclerView);

        RecyclerView.Adapter carouselAdapter = ((RecyclerView) carouselRecyclerView).getAdapter();

        Assert.assertEquals(FAKE_CONTEXTUAL_SUGGESTIONS.size(), carouselAdapter.getItemCount());
    }

    @Test
    //@MediumTest
    @DisabledTest(message = "see crbug.com/758179")
    @Feature({"ContextualSuggestions", "RenderTest"})
    @CommandLineFlags.Add(ENABLE_CONTEXTUAL_SUGGESTIONS)
    public void testCardAppearance() throws Exception {
        mSuggestionsSource.setContextualSuggestions(FAKE_CONTEXTUAL_SUGGESTIONS);

        String testUrl = mTestServerRule.getServer().getURL(TEST_PAGE);
        mActivityRule.loadUrl(testUrl);
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mActivityRule.getObserver().mOpenedCallbackHelper.waitForCallback(0);

        RecyclerView carouselRecyclerView = getCarouselRecyclerView();

        mRenderTestRule.render(carouselRecyclerView.getChildAt(0), "contextual_suggestions_card");
    }

    @Nullable
    private RecyclerView getCarouselRecyclerView() {
        SuggestionsRecyclerView mainRecyclerView = mActivityRule.getRecyclerView();
        int position = mainRecyclerView.getNewTabPageAdapter().getFirstPositionForType(
                ItemViewType.CAROUSEL);
        if (position == RecyclerView.NO_POSITION) return null;

        View recyclerView = mainRecyclerView.getChildAt(position);
        Assert.assertTrue(recyclerView instanceof RecyclerView);
        return (RecyclerView) recyclerView;
    }
}
