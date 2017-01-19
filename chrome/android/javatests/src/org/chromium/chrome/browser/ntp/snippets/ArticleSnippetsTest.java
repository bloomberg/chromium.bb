// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.BitmapFactory;
import android.support.test.filters.MediumTest;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.FaviconHelper.IconAvailabilityCallback;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsMetricsReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.RenderUtils.ViewRenderer;

import java.io.IOException;
import java.util.Arrays;
import java.util.Set;

/**
 * Tests for the appearance of Article Snippets.
 */
public class ArticleSnippetsTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private ViewRenderer mViewRenderer;

    private SuggestionsUiDelegate mUiDelegate;
    private FakeSuggestionsSource mSnippetsSource;
    private NewTabPageRecyclerView mRecyclerView;
    private NewTabPageAdapter mAdapter;

    private FrameLayout mContentView;
    private UiConfig mUiConfig;

    public ArticleSnippetsTest() {
        super(ChromeActivity.class);
    }

    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    @RetryOnFailure
    public void testSnippetAppearance() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                setupTestData();

                mContentView = new FrameLayout(getActivity());
                mUiConfig = new UiConfig(mContentView);

                getActivity().setContentView(mContentView);

                mRecyclerView = (NewTabPageRecyclerView) getActivity().getLayoutInflater()
                        .inflate(R.layout.new_tab_page_recycler_view, mContentView, false);
                mContentView.addView(mRecyclerView);

                View aboveTheFold = new View(getActivity());

                mRecyclerView.setAboveTheFoldView(aboveTheFold);
                mAdapter = new NewTabPageAdapter(mUiDelegate, aboveTheFold, mUiConfig,
                        OfflinePageBridge.getForProfile(Profile.getLastUsedProfile()),
                        /* contextMenuManager = */null);
                mRecyclerView.setAdapter(mAdapter);
            }
        });

        getInstrumentation().waitForIdleSync();

        int first = mAdapter.getFirstCardPosition();
        mViewRenderer.renderAndCompare(mRecyclerView.getChildAt(first), "short_snippet");
        mViewRenderer.renderAndCompare(mRecyclerView.getChildAt(first + 1), "long_snippet");

        int firstOfSecondCategory = first + 1 /* card 2 */ + 1 /* header */ + 1 /* card 3*/;

        mViewRenderer.renderAndCompare(
                mRecyclerView.getChildAt(firstOfSecondCategory), "minimal_snippet");
        mViewRenderer.renderAndCompare(mRecyclerView, "snippets");

        // See how everything looks in narrow layout.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Since we inform the UiConfig manually about the desired display style, the only
                // reason we actually change the LayoutParams is for the rendered Views to look
                // right.
                ViewGroup.LayoutParams params = mContentView.getLayoutParams();
                params.width = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 350,
                        mRecyclerView.getResources().getDisplayMetrics());
                mContentView.setLayoutParams(params);

                mUiConfig.setDisplayStyleForTesting(UiConfig.DISPLAY_STYLE_NARROW);
            }
        });

        getInstrumentation().waitForIdleSync();

        mViewRenderer.renderAndCompare(mRecyclerView.getChildAt(first), "short_snippet_narrow");
        mViewRenderer.renderAndCompare(mRecyclerView.getChildAt(first + 1), "long_snippet_narrow");
        mViewRenderer.renderAndCompare(mRecyclerView.getChildAt(firstOfSecondCategory),
                "long_minimal_snippet_narrow");
        mViewRenderer.renderAndCompare(mRecyclerView.getChildAt(firstOfSecondCategory + 1),
                "short_minimal_snippet_narrow");
        mViewRenderer.renderAndCompare(mRecyclerView, "snippets_narrow");
    }

    private void setupTestData() {
        @CategoryInt
        int fullCategory = 0;
        @CategoryInt
        int minimalCategory = 1;
        SnippetArticle shortSnippet = new SnippetArticle(fullCategory, "id1", "Snippet",
                "Publisher", "Preview Text", "www.google.com",
                1466614774, // Timestamp
                10f); // Score
        shortSnippet.setThumbnailBitmap(BitmapFactory.decodeResource(getActivity().getResources(),
                R.drawable.signin_promo_illustration));

        SnippetArticle longSnippet = new SnippetArticle(fullCategory, "id2",
                new String(new char[20]).replace("\0", "Snippet "),
                new String(new char[20]).replace("\0", "Publisher "),
                new String(new char[80]).replace("\0", "Preview Text "), "www.google.com",
                1466614074, // Timestamp
                20f); // Score

        SnippetArticle minimalSnippet = new SnippetArticle(minimalCategory, "id3",
                new String(new char[20]).replace("\0", "Bookmark "), "Publisher",
                "This should not be displayed", "www.google.com",
                1466614774, // Timestamp
                10f); // Score

        SnippetArticle minimalSnippet2 = new SnippetArticle(minimalCategory, "id4", "Bookmark",
                "Publisher", "This should not be displayed", "www.google.com",
                1466614774, // Timestamp
                10f); // Score

        mSnippetsSource.setInfoForCategory(
                fullCategory, new SuggestionsCategoryInfo(fullCategory, "Section Title",
                                      ContentSuggestionsCardLayout.FULL_CARD, false, false, false,
                                      true, "No suggestions"));
        mSnippetsSource.setStatusForCategory(fullCategory, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(
                fullCategory, Arrays.asList(shortSnippet, longSnippet));

        mSnippetsSource.setInfoForCategory(
                minimalCategory, new SuggestionsCategoryInfo(minimalCategory, "Section Title",
                                         ContentSuggestionsCardLayout.MINIMAL_CARD, false, false,
                                         false, true, "No suggestions"));
        mSnippetsSource.setStatusForCategory(minimalCategory, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(
                minimalCategory, Arrays.asList(minimalSnippet, minimalSnippet2));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mViewRenderer = new ViewRenderer(getActivity(),
                "chrome/test/data/android/render_tests", this.getClass().getSimpleName());
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mUiDelegate = new MockUiDelegate();
        mSnippetsSource = new FakeSuggestionsSource();
    }

    /**
     * A SuggestionsUiDelegate to initialize our Adapter.
     */
    private class MockUiDelegate implements SuggestionsUiDelegate {
        private SuggestionsMetricsReporter mSuggestionsMetricsReporter =
                new DummySuggestionsMetricsReporter();

        @Override
        public void getLocalFaviconImageForURL(
                final String url, int size, final FaviconImageCallback faviconCallback) {
            // Run the callback asynchronously incase the caller made that assumption.
            ThreadUtils.postOnUiThread(new Runnable(){
                @Override
                public void run() {
                    // Return an arbitrary drawable.
                    faviconCallback.onFaviconAvailable(
                            BitmapFactory.decodeResource(getActivity().getResources(),
                            R.drawable.star_green),
                            url);
                }
            });
        }

        @Override
        public void getLargeIconForUrl(String url, int size, LargeIconCallback callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void ensureIconIsAvailable(String pageUrl, String iconUrl, boolean isLargeIcon,
                boolean isTemporary, IconAvailabilityCallback callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getUrlsAvailableOffline(Set<String> pageUrls, Callback<Set<String>> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public SuggestionsSource getSuggestionsSource() {
            return mSnippetsSource;
        }

        @Override
        public void addDestructionObserver(DestructionObserver destructionObserver) {}

        @Override
        public SuggestionsMetricsReporter getMetricsReporter() {
            return mSuggestionsMetricsReporter;
        }

        @Override
        public SuggestionsNavigationDelegate getNavigationDelegate() {
            return null;
        }
    }

    private static class DummySuggestionsMetricsReporter implements SuggestionsMetricsReporter {
        @Override
        public void onPageShown(int[] categories, int[] suggestionsPerCategory) {}

        @Override
        public void onSuggestionShown(SnippetArticle suggestion) {}

        @Override
        public void onSuggestionOpened(SnippetArticle suggestion, int windowOpenDisposition) {}

        @Override
        public void onSuggestionMenuOpened(SnippetArticle suggestion) {}

        @Override
        public void onMoreButtonShown(@CategoryInt ActionItem category) {}

        @Override
        public void onMoreButtonClicked(@CategoryInt ActionItem category) {}

        @Override
        public void setRanker(SuggestionsRanker suggestionsRanker) {}
    }
}
