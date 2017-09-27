// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static android.graphics.BitmapFactory.decodeFile;

import static org.chromium.base.test.util.UrlUtils.getTestFilePath;
import static org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites.createSiteSuggestion;

import android.graphics.Bitmap;

import org.chromium.base.ThreadUtils;
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
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Interface for providing test data
 */
public class NtpUiCaptureTestData {
    private static final String QUERIES_PUBLISHER = "Queries";
    private static final String SPORTS_PUBLISHER = "Football, cricket, hockey, and more | Sports";
    private static final String MEME_PUBLISHER = "Meme Feed";
    private static final String FACTS_PUBLISHER = "Facts";
    private static final String NEWS_PUBLISHER = "The Morning News";
    private static final String TECH_PUBLISHER = "Tech";
    private static final String SHOP_PUBLISHER = "Shop.rr";
    private static final String ENTERTAINMENT_PUBLISHER = "Now Entertainment";

    private static final SiteSuggestion[] SITE_SUGGESTIONS = {
            createSiteSuggestion(QUERIES_PUBLISHER, "queries"),
            createSiteSuggestion(SPORTS_PUBLISHER, "sports"),
            createSiteSuggestion(MEME_PUBLISHER, "meme"),
            createSiteSuggestion(FACTS_PUBLISHER, "facts"),
            createSiteSuggestion(NEWS_PUBLISHER, "news"),
            createSiteSuggestion(TECH_PUBLISHER, "tech"),
            createSiteSuggestion(SHOP_PUBLISHER, "shop"),
            createSiteSuggestion(ENTERTAINMENT_PUBLISHER, "movies")};

    /** Grey, the default fallback color as defined in fallback_icon_style.cc. */
    private static final int DEFAULT_ICON_COLOR = 0xff787878;

    private static final int[] FALLBACK_COLORS = {
            0xff306090, // Muted blue.
            0xff903060, // Muted purplish red.
            0xff309060, // Muted green.
            0xff603090, // Muted purple.
            0xff906030, // Muted brown.
            DEFAULT_ICON_COLOR,
            0xff609060, // Muted brownish green.
            0xff903030 // Muted red.
    };

    private static final Bitmap QUERIES_ICON =
            decodeFile(getTestFilePath("/android/UiCapture/dots.png"));
    private static final Bitmap SPORTS_ICON =
            decodeFile(getTestFilePath("/android/UiCapture/landscape.png"));
    private static final Bitmap MEME_ICON =
            decodeFile(getTestFilePath("/android/UiCapture/hot.png"));
    private static final Bitmap NEWS_ICON =
            decodeFile(getTestFilePath("/android/UiCapture/train.png"));
    private static final Bitmap SHOP_ICON =
            decodeFile(getTestFilePath("/android/UiCapture/heart.png"));
    private static final Bitmap ENTERTAINMENT_ICON =
            decodeFile(getTestFilePath("/android/UiCapture/cloud.png"));

    private static final SnippetArticle[] FAKE_ARTICLE_SUGGESTIONS = new SnippetArticle[] {
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion0",
                    "James Roderick to step down as conductor for Laville orchestra",
                    NEWS_PUBLISHER, "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JUNE, 1), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion1", "Boy raises orphaned goat",
                    MEME_PUBLISHER, "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion2", "Top gigs this week",
                    ENTERTAINMENT_PUBLISHER, "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion3", "No, you can’t sit here",
                    MEME_PUBLISHER, "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion4",
                    "Army training more difficult than expected", FACTS_PUBLISHER,
                    "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion5",
                    "Classical music attracts smaller audiences", NEWS_PUBLISHER,
                    "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion6",
                    "Report finds that freelancers are happier", ENTERTAINMENT_PUBLISHER,
                    "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion7",
                    "Dog denies eating the cookies", MEME_PUBLISHER, "summary is not used",
                    "http://example.com", getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.ARTICLES, "suggestion8",
                    "National train strike leads to massive delays for commuters", NEWS_PUBLISHER,
                    "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.JANUARY, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
    };

    private static final SnippetArticle[] FAKE_BOOKMARK_SUGGESTIONS = new SnippetArticle[] {
            new SnippetArticle(KnownCategories.BOOKMARKS, "bookmark0",
                    "Light pollution worse than ever", FACTS_PUBLISHER, "summary is not used",
                    "http://example.com", getTimestamp(2017, Calendar.MARCH, 10), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.BOOKMARKS, "bookmark1",
                    "Emergency services suffering further budget cuts", NEWS_PUBLISHER,
                    "summary is not used", "http://example.com",
                    getTimestamp(2017, Calendar.FEBRUARY, 20), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
            new SnippetArticle(KnownCategories.BOOKMARKS, "bookmark2",
                    "Local election yields surprise winner", FACTS_PUBLISHER, "summary is not used",
                    "http://example.com", getTimestamp(2017, Calendar.MARCH, 30), 0.0f, 0L, false,
                    /* thumbnailDominantColor = */ null),
    };

    public static void registerArticleSamples(FakeSuggestionsSource suggestionsSource) {
        ContentSuggestionsTestUtils.registerCategory(suggestionsSource, KnownCategories.ARTICLES);
        suggestionsSource.setSuggestionsForCategory(
                KnownCategories.ARTICLES, Arrays.asList(FAKE_ARTICLE_SUGGESTIONS));
        suggestionsSource.setFaviconForId("suggestion0", NEWS_ICON);
        suggestionsSource.setThumbnailForId("suggestion0", "/android/UiCapture/conductor.jpg");
        suggestionsSource.setFaviconForId("suggestion2", ENTERTAINMENT_ICON);
        suggestionsSource.setThumbnailForId("suggestion2", "/android/UiCapture/gig.jpg");
        suggestionsSource.setThumbnailForId("suggestion3", "/android/UiCapture/bench.jpg");
        suggestionsSource.setFaviconForId("suggestion5", NEWS_ICON);
        suggestionsSource.setThumbnailForId("suggestion5", "/android/UiCapture/violin.jpg");
        suggestionsSource.setFaviconForId("suggestion6", ENTERTAINMENT_ICON);
        suggestionsSource.setThumbnailForId("suggestion6", "/android/UiCapture/freelancer.jpg");
        suggestionsSource.setThumbnailForId("suggestion7", "/android/UiCapture/dog.jpg");
        suggestionsSource.setFaviconForId("suggestion8", NEWS_ICON);
    }

    public static void registerBookmarkSamples(FakeSuggestionsSource suggestionsSource) {
        ContentSuggestionsTestUtils.registerCategory(suggestionsSource, KnownCategories.BOOKMARKS);
        suggestionsSource.setSuggestionsForCategory(
                KnownCategories.BOOKMARKS, Arrays.asList(FAKE_BOOKMARK_SUGGESTIONS));
        suggestionsSource.setThumbnailForId("bookmark1", "/android/UiCapture/fire.jpg");
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
        iconMap.put(SITE_SUGGESTIONS[0].url, QUERIES_ICON);
        iconMap.put(SITE_SUGGESTIONS[1].url, SPORTS_ICON);
        iconMap.put(SITE_SUGGESTIONS[2].url, MEME_ICON);
        iconMap.put(SITE_SUGGESTIONS[4].url, NEWS_ICON);
        iconMap.put(SITE_SUGGESTIONS[6].url, SHOP_ICON);
        iconMap.put(SITE_SUGGESTIONS[7].url, ENTERTAINMENT_ICON);

        final Map<String, Integer> colorMap = new HashMap<>();
        for (int i = 0; i < SITE_SUGGESTIONS.length; i++) {
            colorMap.put(SITE_SUGGESTIONS[i].url, FALLBACK_COLORS[i]);
        }
        return new LargeIconBridge() {
            @Override
            public boolean getLargeIconForUrl(
                    String url, int desiredSizePx, LargeIconCallback callback) {
                ThreadUtils.postOnUiThread(() -> {
                    int fallbackColor =
                            colorMap.containsKey(url) ? colorMap.get(url) : DEFAULT_ICON_COLOR;
                    callback.onLargeIconAvailable(iconMap.get(url), fallbackColor, true);
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

    public static List<SiteSuggestion> getSiteSuggestions() {
        return Collections.unmodifiableList(Arrays.asList(SITE_SUGGESTIONS));
    }
}
