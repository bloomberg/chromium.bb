// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.support.annotation.DrawableRes;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.text.format.DateUtils;
import android.util.TypedValue;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExternalResource;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.base.test.util.parameter.CommandLineParameter;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.download.ui.ThumbnailProvider;
import org.chromium.chrome.browser.download.ui.ThumbnailProvider.ThumbnailRequest;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.ContextMenuManager.TouchEnabledDelegate;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.suggestions.DestructionObserver;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.ThumbnailGradient;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * Tests for the appearance of Article Snippets.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class ArticleSnippetsTest {
    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    // Rules must be public for JUnit to access them, but FindBugs complains about that.
    @SuppressFBWarnings("URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD")
    @Rule
    public ExternalResource mDisableChromeAnimationsRule = new ExternalResource() {

        private float mOldAnimationMultiplier;

        @Override
        protected void before() {
            mOldAnimationMultiplier = ChromeAnimation.Animation.getAnimationMultiplier();
            ChromeAnimation.Animation.setAnimationMultiplierForTesting(0f);
        }

        @Override
        protected void after() {
            ChromeAnimation.Animation.setAnimationMultiplierForTesting(mOldAnimationMultiplier);
        }
    };

    private SuggestionsUiDelegate mUiDelegate;
    private FakeSuggestionsSource mSnippetsSource;
    private SuggestionsRecyclerView mRecyclerView;
    private NewTabPageAdapter mAdapter;

    private FrameLayout mContentView;
    private SnippetArticleViewHolder mSuggestion;
    private SignInPromo.GenericPromoViewHolder mSigninPromo;

    private UiConfig mUiConfig;

    private MockThumbnailProvider mThumbnailProvider;

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/757735")
    @Feature({"ArticleSnippets", "RenderTest"})
    @CommandLineParameter({"", "enable-features=" + ChromeFeatureList.CHROME_HOME + ","
            + ChromeFeatureList.CHROME_HOME_MODERN_LAYOUT})
    @RetryOnFailure
    public void testSnippetAppearance() throws IOException {
        // Don't load the Bitmap on the UI thread - this is a StrictModeViolation.
        final Bitmap watch = BitmapFactory.decodeFile(
                UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/watch.jpg"));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                setupTestData(watch);

                mContentView = new FrameLayout(mActivityTestRule.getActivity());
                mUiConfig = new UiConfig(mContentView);

                mActivityTestRule.getActivity().setContentView(mContentView);

                mRecyclerView = new SuggestionsRecyclerView(mActivityTestRule.getActivity());
                mContentView.addView(mRecyclerView);

                mAdapter = new NewTabPageAdapter(mUiDelegate, /* aboveTheFold = */ null, mUiConfig,
                        OfflinePageBridge.getForProfile(Profile.getLastUsedProfile()),
                        /* contextMenuManager = */ null, /* tileGroupDelegate = */ null,
                        /* suggestionsCarousel = */ null);
                mAdapter.refreshSuggestions();
                mRecyclerView.setAdapter(mAdapter);
            }
        });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        int first = mAdapter.getFirstCardPosition();
        mRenderTestRule.render(mRecyclerView.getChildAt(first), "short_snippet");
        mRenderTestRule.render(mRecyclerView.getChildAt(first + 1), "long_snippet");

        int firstOfSecondCategory = first + 1 /* card 2 */ + 1 /* header */ + 1 /* card 3 */;

        mRenderTestRule.render(mRecyclerView.getChildAt(firstOfSecondCategory), "minimal_snippet");
        mRenderTestRule.render(mRecyclerView, "snippets");

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

                mUiConfig.setDisplayStyleForTesting(new UiConfig.DisplayStyle(
                        HorizontalDisplayStyle.NARROW, VerticalDisplayStyle.REGULAR));
            }
        });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mRenderTestRule.render(mRecyclerView.getChildAt(first), "short_snippet_narrow");
        mRenderTestRule.render(mRecyclerView.getChildAt(first + 1), "long_snippet_narrow");
        mRenderTestRule.render(
                mRecyclerView.getChildAt(firstOfSecondCategory), "long_minimal_snippet_narrow");
        mRenderTestRule.render(mRecyclerView.getChildAt(firstOfSecondCategory + 1),
                "short_minimal_snippet_narrow");
        mRenderTestRule.render(mRecyclerView, "snippets_narrow");
    }

    @Test
    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    public void testDownloadSuggestion() throws IOException {
        final String filePath =
                UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/capybara.jpg");
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentView = new FrameLayout(mActivityTestRule.getActivity());
                mUiConfig = new UiConfig(mContentView);

                mActivityTestRule.getActivity().setContentView(mContentView);

                mRecyclerView = new SuggestionsRecyclerView(mActivityTestRule.getActivity());
                TouchEnabledDelegate touchEnabledDelegate = new TouchEnabledDelegate() {
                    @Override
                    public void setTouchEnabled(boolean enabled) {
                        mRecyclerView.setTouchEnabled(enabled);
                    }
                };
                ContextMenuManager contextMenuManager =
                        new ContextMenuManager(mActivityTestRule.getActivity(),
                                mUiDelegate.getNavigationDelegate(), touchEnabledDelegate);
                mRecyclerView.init(mUiConfig, contextMenuManager);
                mRecyclerView.setAdapter(mAdapter);

                mSuggestion = new SnippetArticleViewHolder(
                        mRecyclerView, contextMenuManager, mUiDelegate, mUiConfig);

                long timestamp = System.currentTimeMillis() - 5 * DateUtils.MINUTE_IN_MILLIS;

                SnippetArticle download = new SnippetArticle(KnownCategories.DOWNLOADS, "id1",
                        "test_image.jpg", "example.com", null, "http://example.com", timestamp, 10f,
                        timestamp, false);
                download.setAssetDownloadData("asdf", filePath, "image/jpeg");
                SuggestionsCategoryInfo categoryInfo =
                        new SuggestionsCategoryInfo(KnownCategories.DOWNLOADS, "Downloads",
                                ContentSuggestionsCardLayout.FULL_CARD,
                                ContentSuggestionsAdditionalAction.NONE,
                                /* show_if_empty = */ true, "No suggestions");
                mSuggestion.onBindViewHolder(download, categoryInfo);
                mContentView.addView(mSuggestion.itemView);
            }
        });

        mRenderTestRule.render(mSuggestion.itemView, "download_snippet_placeholder");

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        List<ThumbnailRequest> requests = mThumbnailProvider.getRequests();
        Assert.assertEquals(1, requests.size());
        final ThumbnailRequest request = requests.get(0);
        Assert.assertEquals(filePath, request.getFilePath());

        final Bitmap thumbnail = BitmapFactory.decodeFile(filePath);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mThumbnailProvider.fulfillRequest(request, thumbnail);
            }
        });
        mRenderTestRule.render(mSuggestion.itemView, "download_snippet_thumbnail");
    }

    @Test
    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    @CommandLineParameter({"", "enable-features=" + ChromeFeatureList.CHROME_HOME + ","
                    + ChromeFeatureList.CHROME_HOME_MODERN_LAYOUT})
    public void testSigninPromo() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new FrameLayout(mActivityTestRule.getActivity());
            mUiConfig = new UiConfig(mContentView);

            mActivityTestRule.getActivity().setContentView(mContentView);

            mRecyclerView = new SuggestionsRecyclerView(mActivityTestRule.getActivity());
            TouchEnabledDelegate touchEnabledDelegate =
                    enabled -> mRecyclerView.setTouchEnabled(enabled);
            ContextMenuManager contextMenuManager =
                    new ContextMenuManager(mActivityTestRule.getActivity(),
                            mUiDelegate.getNavigationDelegate(), touchEnabledDelegate);
            mRecyclerView.init(mUiConfig, contextMenuManager);
            mRecyclerView.setAdapter(mAdapter);

            mSigninPromo = new SignInPromo.GenericPromoViewHolder(
                    mRecyclerView, contextMenuManager, mUiConfig);
            mSigninPromo.onBindViewHolder(new SignInPromo.GenericSigninPromoData());
            mContentView.addView(mSigninPromo.itemView);
        });

        mRenderTestRule.render(mSigninPromo.itemView, "signin_promo");
    }

    private void setupTestData(Bitmap thumbnail) {
        @CategoryInt
        int fullCategory = 0;
        @CategoryInt
        int minimalCategory = 1;
        SnippetArticle shortSnippet = new SnippetArticle(fullCategory, "id1", "Snippet",
                "Publisher", "Preview Text", "www.google.com",
                1466614774, // Publish timestamp
                10f, // Score
                1466634774, // Fetch timestamp
                false); // IsVideoSuggestion

        Drawable drawable = ThumbnailGradient.createDrawableWithGradientIfNeeded(
                thumbnail, mActivityTestRule.getActivity().getResources());
        shortSnippet.setThumbnail(mUiDelegate.getReferencePool().put(drawable));

        SnippetArticle longSnippet = new SnippetArticle(fullCategory, "id2",
                new String(new char[20]).replace("\0", "Snippet "),
                new String(new char[20]).replace("\0", "Publisher "),
                new String(new char[80]).replace("\0", "Preview Text "), "www.google.com",
                1466614074, // Publish timestamp
                20f, // Score
                1466634774, // Fetch timestamp
                false); // IsVideoSuggestion

        SnippetArticle minimalSnippet = new SnippetArticle(minimalCategory, "id3",
                new String(new char[20]).replace("\0", "Bookmark "), "Publisher",
                "This should not be displayed", "www.google.com",
                1466614774, // Publish timestamp
                10f, // Score
                1466634774, // Fetch timestamp
                false); // IsVideoSuggestion

        SnippetArticle minimalSnippet2 = new SnippetArticle(minimalCategory, "id4", "Bookmark",
                "Publisher", "This should not be displayed", "www.google.com",
                1466614774, // Publish timestamp
                10f, // Score
                1466634774, // Fetch timestamp
                false); // IsVideoSuggestion

        mSnippetsSource.setInfoForCategory(fullCategory,
                new SuggestionsCategoryInfo(fullCategory, "Section Title",
                        ContentSuggestionsCardLayout.FULL_CARD,
                        ContentSuggestionsAdditionalAction.NONE,
                        /*show_if_empty=*/true, "No suggestions"));
        mSnippetsSource.setStatusForCategory(fullCategory, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(
                fullCategory, Arrays.asList(shortSnippet, longSnippet));

        mSnippetsSource.setInfoForCategory(minimalCategory,
                new SuggestionsCategoryInfo(minimalCategory, "Section Title",
                        ContentSuggestionsCardLayout.MINIMAL_CARD,
                        ContentSuggestionsAdditionalAction.NONE,
                        /* show_if_empty = */ true, "No suggestions"));
        mSnippetsSource.setStatusForCategory(minimalCategory, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(
                minimalCategory, Arrays.asList(minimalSnippet, minimalSnippet2));
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromePreferenceManager.getInstance().setNewTabPageGenericSigninPromoDismissed(true);
        mThumbnailProvider = new MockThumbnailProvider();
        mSnippetsSource = new FakeSuggestionsSource();
        mSuggestionsDeps.getFactory().thumbnailProvider = mThumbnailProvider;
        mSuggestionsDeps.getFactory().suggestionsSource = mSnippetsSource;
        mUiDelegate = new MockUiDelegate();
        mSnippetsSource.setDefaultFavicon(getBitmap(R.drawable.star_green));

        FeatureUtilities.resetChromeHomeEnabledForTests();
        FeatureUtilities.cacheChromeHomeEnabled();

        if (FeatureUtilities.isChromeHomeModernEnabled()) {
            mRenderTestRule.setVariantPrefix("modern");
        }
    }

    private Bitmap getBitmap(@DrawableRes int resId) {
        return BitmapFactory.decodeResource(
                mActivityTestRule.getInstrumentation().getTargetContext().getResources(), resId);
    }

    /**
     * Simple mock ThumbnailProvider that allows delaying requests and fulfilling them at a later
     * point.
     */
    private static class MockThumbnailProvider implements ThumbnailProvider {
        private final List<ThumbnailRequest> mRequests = new ArrayList<>();

        public List<ThumbnailRequest> getRequests() {
            return mRequests;
        }

        public void fulfillRequest(ThumbnailRequest request, Bitmap bitmap) {
            cancelRetrieval(request);
            request.onThumbnailRetrieved(request.getFilePath(), bitmap);
        }

        @Override
        public void destroy() {}

        @Override
        public void getThumbnail(ThumbnailRequest request) {
            mRequests.add(request);
        }

        @Override
        public void cancelRetrieval(ThumbnailRequest request) {
            boolean removed = mRequests.remove(request);
            Assert.assertTrue(
                    String.format(Locale.US, "Request for '%s' not found", request.getFilePath()),
                    removed);
        }
    }

    /**
     * A SuggestionsUiDelegate to initialize our Adapter.
     */
    private class MockUiDelegate implements SuggestionsUiDelegate {
        private final SuggestionsEventReporter mSuggestionsEventReporter =
                new DummySuggestionsEventReporter();
        private final SuggestionsRanker mSuggestionsRanker = new SuggestionsRanker();
        private final DiscardableReferencePool mReferencePool = new DiscardableReferencePool();
        private final ImageFetcher mImageFetcher =
                new MockImageFetcher(mSnippetsSource, mReferencePool);

        @Override
        public SuggestionsSource getSuggestionsSource() {
            return mSnippetsSource;
        }

        @Override
        public SuggestionsRanker getSuggestionsRanker() {
            return mSuggestionsRanker;
        }

        @Override
        public DiscardableReferencePool getReferencePool() {
            return mReferencePool;
        }

        @Override
        public void addDestructionObserver(DestructionObserver destructionObserver) {}

        @Override
        public boolean isVisible() {
            return true;
        }

        @Override
        public SuggestionsEventReporter getEventReporter() {
            return mSuggestionsEventReporter;
        }

        @Override
        public SuggestionsNavigationDelegate getNavigationDelegate() {
            return null;
        }

        @Override
        public ImageFetcher getImageFetcher() {
            return mImageFetcher;
        }
    }

    private class MockImageFetcher extends ImageFetcher {
        public MockImageFetcher(
                SuggestionsSource suggestionsSource, DiscardableReferencePool referencePool) {
            super(suggestionsSource, null, referencePool, null);
        }

        @Override
        public void makeFaviconRequest(SnippetArticle suggestion, final int faviconSizePx,
                final Callback<Bitmap> faviconCallback) {
            // Run the callback asynchronously in case the caller made that assumption.
            ThreadUtils.postOnUiThread(() -> {
                // Return an arbitrary drawable.
                faviconCallback.onResult(getBitmap(R.drawable.star_green));
            });
        }

        @Override
        public void makeLargeIconRequest(final String url, final int largeIconSizePx,
                final LargeIconBridge.LargeIconCallback callback) {
            // Run the callback asynchronously in case the caller made that assumption.
            ThreadUtils.postOnUiThread(() -> {
                // Return an arbitrary drawable.
                callback.onLargeIconAvailable(
                        getBitmap(R.drawable.star_green), largeIconSizePx, true);
            });
        }
    }
}
