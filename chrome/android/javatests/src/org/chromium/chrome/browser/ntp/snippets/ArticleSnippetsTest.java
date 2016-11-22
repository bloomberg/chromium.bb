// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.BitmapFactory;
import android.test.suitebuilder.annotation.MediumTest;
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
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.MostVisitedItem;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.MostVisitedSites.MostVisitedURLsObserver;
import org.chromium.chrome.browser.profiles.Profile;
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

    private NewTabPageManager mNtpManager;
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
                mAdapter = new NewTabPageAdapter(mNtpManager, aboveTheFold, mUiConfig,
                        OfflinePageBridge.getForProfile(Profile.getLastUsedProfile()));
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
        SnippetArticle shortSnippet = new SnippetArticle(
                fullCategory,
                "id1",
                "Snippet",
                "Publisher",
                "Preview Text",
                "www.google.com",
                "",  // AMP URL
                1466614774,  // Timestamp
                10f,  // Score
                0);  // Position
        shortSnippet.setThumbnailBitmap(BitmapFactory.decodeResource(getActivity().getResources(),
                R.drawable.signin_promo_illustration));

        SnippetArticle longSnippet = new SnippetArticle(
                fullCategory,
                "id2",
                new String(new char[20]).replace("\0", "Snippet "),
                new String(new char[20]).replace("\0", "Publisher "),
                new String(new char[80]).replace("\0", "Preview Text "),
                "www.google.com",
                "",  // AMP URL
                1466614074,  // Timestamp
                20f,  // Score
                1);  // Position

        SnippetArticle minimalSnippet = new SnippetArticle(
                minimalCategory,
                "id3",
                new String(new char[20]).replace("\0", "Bookmark "),
                "Publisher",
                "This should not be displayed",
                "www.google.com",
                "",  // AMP URL
                1466614774,  // Timestamp
                10f,  // Score
                0);  // Position

        SnippetArticle minimalSnippet2 = new SnippetArticle(
                minimalCategory,
                "id4",
                "Bookmark",
                "Publisher",
                "This should not be displayed",
                "www.google.com",
                "",  // AMP URL
                1466614774,  // Timestamp
                10f,  // Score
                0);  // Position

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
        mNtpManager = new MockNewTabPageManager();
        mSnippetsSource = new FakeSuggestionsSource();
    }

    /**
     * A NewTabPageManager to initialize our Adapter.
     */
    private class MockNewTabPageManager implements NewTabPageManager {
        // TODO(dgn): provide a RecyclerView if we need to test the context menu.
        private ContextMenuManager mContextMenuManager =
                new ContextMenuManager(this, getActivity(), null);

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
        public void removeMostVisitedItem(MostVisitedItem item) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void openMostVisitedItem(int windowDisposition, MostVisitedItem item) {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isLocationBarShownInNTP() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isVoiceSearchEnabled() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isFakeOmniboxTextEnabledTablet() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void navigateToBookmarks() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void navigateToRecentTabs() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void trackSnippetsPageImpression(int[] categories, int[] suggestionsPerCategory) {}

        @Override
        public void trackSnippetImpression(SnippetArticle article) {}

        @Override
        public void trackSnippetMenuOpened(SnippetArticle article) {}

        @Override
        public void trackSnippetCategoryActionImpression(int category, int position) {}

        @Override
        public void trackSnippetCategoryActionClick(int category, int position) {}

        @Override
        public void openSnippet(int windowOpenDisposition, SnippetArticle article) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void focusSearchBox(boolean beginVoiceSearch, String pastedText) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void setMostVisitedURLsObserver(MostVisitedURLsObserver observer, int numResults) {
            throw new UnsupportedOperationException();
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
        public void onLogoClicked(boolean isAnimatedLogoShowing) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getSearchProviderLogo(LogoObserver logoObserver) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onLoadingComplete(MostVisitedItem[] mostVisitedItems) {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isOpenInNewWindowEnabled() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isOpenInIncognitoEnabled() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void navigateToDownloadManager() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onLearnMoreClicked() {
            throw new UnsupportedOperationException();
        }

        @Override
        public SuggestionsSource getSuggestionsSource() {
            return mSnippetsSource;
        }

        @Override
        public void addDestructionObserver(DestructionObserver destructionObserver) {}

        @Override
        public boolean isCurrentPage() {
            return true;
        }

        @Override
        public ContextMenuManager getContextMenuManager() {
            return mContextMenuManager;
        }
    }
}
