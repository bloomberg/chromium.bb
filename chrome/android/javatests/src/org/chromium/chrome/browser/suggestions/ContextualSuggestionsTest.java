// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.ChromeHome;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for Contextual suggestions.
 */
@DisabledTest(message = "https://crbug.com/805160")
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class ContextualSuggestionsTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    private static final List<SnippetArticle> FAKE_CONTEXTUAL_SUGGESTIONS = Arrays.asList(
            new SnippetArticle(KnownCategories.CONTEXTUAL, // category
                    "suggestion0", // idWithinCategory
                    "James Roderick to step down as conductor for Laville orchestra", // title
                    "The Curious One", // publisher
                    "http://example.com", // url
                    0, // publishTimestamp
                    0.0f, // score
                    0L, // fetchTimestamp
                    false, // isVideoSuggestion
                    null), // thumbnailDominantColor
            new SnippetArticle(KnownCategories.CONTEXTUAL, // category
                    "suggestion1", // idWithinCategory
                    "Boy raises orphaned goat", // title
                    "Meme feed", // publisher
                    "http://example.com", // url
                    0, // publishTimestamp
                    0.0f, // score
                    0L, // fetchTimestamp
                    false, // isVideoSuggestion
                    null), // thumbnailDominantColor
            new SnippetArticle(KnownCategories.CONTEXTUAL, // category
                    "suggestion2", // idWithinCategory
                    "Top gigs this week", // title
                    "Hello World", // publisher
                    "http://example.com", // url
                    0, // publishTimestamp
                    0.0f, // score
                    0L, // fetchTimestamp
                    false, // isVideoSuggestion
                    null)); // thumbnailDominantColor
    @Rule
    public TestRule mChromeHomeStateRule = new ChromeHome.Processor();
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();
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

        Assert.assertTrue(FeatureUtilities.isChromeHomeEnabled());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @ChromeHome.Enable
    public void testCarouselIsNotShownWhenFlagIsDisabled() {
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
    @ChromeHome.Enable
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL)
    public void testCarouselIsNotShownWhenFlagsEnabledButNoSuggestions() {
        Assert.assertTrue(
                ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL));

        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        View carouselRecyclerView = getCarouselRecyclerView();
        Assert.assertNull(carouselRecyclerView);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @ChromeHome.Enable
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL)
    public void testCarouselIsShownWhenItReceivedSuggestions()
            throws InterruptedException, TimeoutException {
        mSuggestionsSource.setContextualSuggestions(FAKE_CONTEXTUAL_SUGGESTIONS);

        String testUrl = mTestServerRule.getServer().getURL(TEST_PAGE);
        mActivityRule.loadUrl(testUrl);
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mActivityRule.getObserver().mOpenedCallbackHelper.waitForCallback(0);

        View carouselRecyclerView = getCarouselRecyclerView();
        Assert.assertNotNull(carouselRecyclerView);

        RecyclerView.Adapter<?> carouselAdapter =
                ((RecyclerView) carouselRecyclerView).getAdapter();

        Assert.assertEquals(FAKE_CONTEXTUAL_SUGGESTIONS.size(), carouselAdapter.getItemCount());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions", "RenderTest"})
    @ChromeHome.Enable
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL)
    public void testCardAppearance() throws InterruptedException, TimeoutException, IOException {
        mSuggestionsSource.setContextualSuggestions(FAKE_CONTEXTUAL_SUGGESTIONS);

        String testUrl = mTestServerRule.getServer().getURL(TEST_PAGE);
        mActivityRule.loadUrl(testUrl);
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mActivityRule.getObserver().mOpenedCallbackHelper.waitForCallback(0);

        RecyclerView recyclerView = getCarouselRecyclerView();

        mRenderTestRule.render(recyclerView.getChildAt(0), "contextual_suggestions_card");
    }

    @Nullable
    private RecyclerView getCarouselRecyclerView() {
        SuggestionsRecyclerView mainRecyclerView = mActivityRule.getRecyclerView();
        int position = mainRecyclerView.getNewTabPageAdapter().getFirstPositionForType(
                ItemViewType.CAROUSEL);
        if (position == RecyclerView.NO_POSITION) return null;

        return (RecyclerView) mainRecyclerView.getChildAt(position);
    }
}
