// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites.createSiteSuggestion;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.suggestions.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;

import java.util.Arrays;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Map;

/**
 * Interface for providing test data
 */
public class NtpUiCaptureTestData {
    private static final SiteSuggestion[] SITE_SUGGESTIONS = {
            createSiteSuggestion("Queries", "queries"),
            createSiteSuggestion("Football, cricket, hockey, and more | Sports", "sports"),
            createSiteSuggestion("Meme Feed", "meme"),
            createSiteSuggestion("Facts", "facts"),
            createSiteSuggestion("The Morning News", "news"),
            createSiteSuggestion("Tech", "tech"),
            createSiteSuggestion("Shop.rr", "shop"),
            createSiteSuggestion("Now Entertainment", "movies")};

    private static final int[] FAKE_MOST_VISITED_COLORS = {Color.RED, Color.BLUE, Color.GREEN,
            Color.BLACK, Color.CYAN, Color.DKGRAY, Color.BLUE, Color.YELLOW};

    private static final SnippetArticle[] FAKE_ARTICLE_SUGGESTIONS = new SnippetArticle[] {
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion0",
                    "James Roderick to step down as conductor for Laville orchestra",
                    "The Morning News", "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JUNE, 1), 0.0f, 0L, false),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion1", "Boy raises orphaned goat",
                    "Meme feed", "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion2", "Top gigs this week",
                    "Now Entertainment", "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false)};

    private static final SnippetArticle[] FAKE_BOOKMARK_SUGGESTIONS = new SnippetArticle[] {
            new SnippetArticle(KnownCategories.BOOKMARKS, "bookmark0",
                    "Light pollution worse than ever", "Facts", "summary is not used",
                    "http://example.com", getTimestamp(2017, Calendar.MARCH, 10), 0.0f, 0L, false),
            new SnippetArticle(KnownCategories.BOOKMARKS, "bookmark1",
                    "Emergency services suffering further budget cuts", "The Morning News",
                    "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.FEBRUARY, 20), 0.0f, 0L, false),
            new SnippetArticle(KnownCategories.BOOKMARKS, "bookmark2",
                    "Local election yields surprise winner", "Facts", "summary is not used",
                    "http://example.com", getTimestamp(2017, Calendar.MARCH, 30), 0.0f, 0L, false),
    };

    public static void registerArticleSamples(FakeSuggestionsSource suggestionsSource) {
        ContentSuggestionsTestUtils.registerCategory(suggestionsSource, KnownCategories.ARTICLES);
        suggestionsSource.setSuggestionsForCategory(
                KnownCategories.ARTICLES, Arrays.asList(FAKE_ARTICLE_SUGGESTIONS));
        final Bitmap favicon = BitmapFactory.decodeFile(
                UrlUtils.getTestFilePath("/android/UiCapture/favicon.ico"));
        Assert.assertNotNull(favicon);
        suggestionsSource.setFaviconForId("suggestion0", favicon);
        suggestionsSource.setThumbnailForId("suggestion0",
                BitmapFactory.decodeFile(
                        UrlUtils.getTestFilePath("/android/UiCapture/conductor.jpg")));
        suggestionsSource.setThumbnailForId("suggestion1",
                BitmapFactory.decodeFile(UrlUtils.getTestFilePath("/android/UiCapture/goat.jpg")));
        suggestionsSource.setThumbnailForId("suggestion2",
                BitmapFactory.decodeFile(UrlUtils.getTestFilePath("/android/UiCapture/gig.jpg")));
    }

    public static void registerBookmarkSamples(FakeSuggestionsSource suggestionsSource) {
        ContentSuggestionsTestUtils.registerCategory(suggestionsSource, KnownCategories.BOOKMARKS);
        suggestionsSource.setSuggestionsForCategory(
                KnownCategories.BOOKMARKS, Arrays.asList(FAKE_BOOKMARK_SUGGESTIONS));
        suggestionsSource.setThumbnailForId("bookmark1",
                BitmapFactory.decodeFile(UrlUtils.getTestFilePath("/android/UiCapture/fire.jpg")));
    }

    private static long getTimestamp(int year, int month, int day) {
        Calendar c = Calendar.getInstance();
        c.set(year, month, day);
        return c.getTimeInMillis();
    }

    private static SuggestionsSource createSuggestionsSource() {
        FakeSuggestionsSource fakeSuggestionsSource = new FakeSuggestionsSource();
        registerArticleSamples(fakeSuggestionsSource);
        registerBookmarkSamples(fakeSuggestionsSource);
        return fakeSuggestionsSource;
    }

    private static MostVisitedSites createMostVisitedSites() {
        FakeMostVisitedSites result = new FakeMostVisitedSites();
        result.setTileSuggestions(SITE_SUGGESTIONS);
        return result;
    }

    private static LargeIconBridge createLargeIconBridge() {
        final Map<String, Bitmap> iconMap = new HashMap<>();
        iconMap.put(SITE_SUGGESTIONS[0].url,
                BitmapFactory.decodeFile(UrlUtils.getTestFilePath("/android/google.png")));
        iconMap.put(SITE_SUGGESTIONS[1].url,
                BitmapFactory.decodeResource(
                        InstrumentationRegistry.getTargetContext().getResources(),
                        R.drawable.star_green));

        final Map<String, Integer> colorMap = new HashMap<>();
        for (int i = 0; i < SITE_SUGGESTIONS.length; i++) {
            colorMap.put(SITE_SUGGESTIONS[i].url, FAKE_MOST_VISITED_COLORS[i]);
        }
        return new LargeIconBridge() {
            @Override
            public boolean getLargeIconForUrl(
                    final String pageUrl, int desiredSizePx, final LargeIconCallback callback) {
                ThreadUtils.postOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        callback.onLargeIconAvailable(
                                iconMap.get(pageUrl), colorMap.get(pageUrl), true);
                    }
                });
                return true;
            }
        };
    }

    public static SuggestionsDependenciesRule.TestFactory createFactory() {
        SuggestionsDependenciesRule.TestFactory result =
                new SuggestionsDependenciesRule.TestFactory();
        result.suggestionsSource = createSuggestionsSource();
        result.eventReporter = new DummySuggestionsEventReporter();
        result.largeIconBridge = createLargeIconBridge();
        result.mostVisitedSites = createMostVisitedSites();
        return result;
    }
}
