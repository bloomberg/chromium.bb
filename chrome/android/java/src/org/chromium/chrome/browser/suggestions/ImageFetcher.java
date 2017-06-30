// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.NativePageHost;
import org.chromium.chrome.browser.download.ui.ThumbnailProvider;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.cards.CardsVariationParameters;
import org.chromium.chrome.browser.ntp.snippets.FaviconFetchResult;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsConfig;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.TimeUnit;

/**
 * Class responsible for fetching images for the views in the NewTabPage and Chrome Home.
 * The images fetched by this class include:
 *   - favicons for content suggestions
 *   - thumbnails for content suggestions
 *   - large icons for URLs (used by the tiles in the NTP/Chrome Home)
 *
 * To fetch an image, the caller should create a request which is done in the following way:
 *   - for favicons: {@link #makeFaviconRequest(SnippetArticle, int, Callback)}
 *   - for article thumbnails: {@link #makeArticleThumbnailRequest(SnippetArticle, Callback)}
 *   - for article downloads: {@link #makeDownloadThumbnailRequest(SnippetArticle, int, Callback)}
 *   - for large icons: {@link #makeLargeIconRequest(String, int,
 * LargeIconBridge.LargeIconCallback)}
 *
 * If there are no errors is the image fetching process, the corresponding bitmap will be returned
 * in the callback. Otherwise, the callback will not be called.
 */
public class ImageFetcher {
    /** Supported favicon sizes in pixels. */
    private static final int[] FAVICON_SERVICE_SUPPORTED_SIZES = {16, 24, 32, 48, 64};
    private static final String FAVICON_SERVICE_FORMAT =
            "https://s2.googleusercontent.com/s2/favicons?domain=%s&src=chrome_newtab_mobile&sz=%d&alt=404";
    private static final int PUBLISHER_FAVICON_MINIMUM_SIZE_PX = 16;

    private final boolean mUseFaviconService;
    private final NativePageHost mHost;

    private boolean mIsDestroyed;

    private final SuggestionsSource mSuggestionsSource;
    private final Profile mProfile;
    private ThumbnailProvider mThumbnailProvider;
    private FaviconHelper mFaviconHelper;
    private LargeIconBridge mLargeIconBridge;

    public ImageFetcher(SuggestionsSource suggestionsSource, Profile profile, NativePageHost host) {
        mSuggestionsSource = suggestionsSource;
        mProfile = profile;
        mUseFaviconService = CardsVariationParameters.isFaviconServiceEnabled();
        mHost = host;
    }

    /**
     * Creates a request for a thumbnail from a downloaded image.
     *
     * If there is an error while fetching the thumbnail, the callback will not be called.
     *
     * @param suggestion The suggestion for which a thumbnail is needed.
     * @param thumbnailSizePx The required size for the thumbnail.
     * @param imageCallback The callback where the bitmap will be returned when fetched.
     * @return The request which will be used to fetch the image.
     */
    public DownloadThumbnailRequest makeDownloadThumbnailRequest(
            SnippetArticle suggestion, int thumbnailSizePx, Callback<Bitmap> imageCallback) {
        assert !mIsDestroyed;

        return new DownloadThumbnailRequest(suggestion, imageCallback, thumbnailSizePx);
    }

    /**
     * Creates a request for an article thumbnail.
     *
     * If there is an error while fetching the thumbnail, the callback will not be called.
     *
     * @param suggestion The article for which a thumbnail is needed.
     * @param callback The callback where the bitmap will be returned when fetched.
     */
    public void makeArticleThumbnailRequest(SnippetArticle suggestion, Callback<Bitmap> callback) {
        assert !mIsDestroyed;

        mSuggestionsSource.fetchSuggestionImage(suggestion, callback);
    }

    /**
     * Creates a request for favicon for the URL of a suggestion.
     *
     * If there is an error while fetching the favicon, the callback will not be called.
     *
     * @param suggestion The suggestion whose URL needs a favicon.
     * @param faviconSizePx The required size for the favicon.
     * @param faviconCallback The callback where the bitmap will be returned when fetched.
     */
    public void makeFaviconRequest(SnippetArticle suggestion, final int faviconSizePx,
            final Callback<Bitmap> faviconCallback) {
        assert !mIsDestroyed;

        long faviconFetchStartTimeMs = SystemClock.elapsedRealtime();
        URI pageUrl;

        try {
            pageUrl = new URI(suggestion.getUrl());
        } catch (URISyntaxException e) {
            assert false;
            return;
        }

        if (!suggestion.isArticle() || !SnippetsConfig.isFaviconsFromNewServerEnabled()) {
            // The old code path. Remove when the experiment is successful.
            // Currently, we have to use this for non-articles, due to privacy.
            fetchFaviconFromLocalCache(pageUrl, true, faviconFetchStartTimeMs, faviconSizePx,
                    suggestion, faviconCallback);
        } else {
            // The new code path.
            fetchFaviconFromLocalCacheOrGoogleServer(
                    suggestion, faviconFetchStartTimeMs, faviconCallback);
        }
    }

    /**
     * Gets the large icon (e.g. favicon or touch icon) for a given URL.
     *
     * If there is an error while fetching the icon, the callback will not be called.
     *
     * @param url The URL of the site whose icon is being requested.
     * @param size The desired size of the icon in pixels.
     * @param callback The callback to be notified when the icon is available.
     */
    public void makeLargeIconRequest(
            String url, int size, LargeIconBridge.LargeIconCallback callback) {
        assert !mIsDestroyed;

        getLargeIconBridge().getLargeIconForUrl(url, size, callback);
    }

    public void onDestroy() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }

        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }

        if (mThumbnailProvider != null) {
            mThumbnailProvider.destroy();
            mThumbnailProvider = null;
        }

        mIsDestroyed = true;
    }

    private void fetchFaviconFromLocalCache(final URI snippetUri, final boolean fallbackToService,
            final long faviconFetchStartTimeMs, final int faviconSizePx,
            final SnippetArticle suggestion, final Callback<Bitmap> faviconCallback) {
        getFaviconHelper().getLocalFaviconImageForURL(mProfile, getSnippetDomain(snippetUri),
                faviconSizePx, new FaviconHelper.FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap image, String iconUrl) {
                        if (image != null) {
                            assert faviconCallback != null;

                            faviconCallback.onResult(image);
                            recordFaviconFetchResult(suggestion,
                                    fallbackToService ? FaviconFetchResult.SUCCESS_CACHED
                                                      : FaviconFetchResult.SUCCESS_FETCHED,
                                    faviconFetchStartTimeMs);
                        } else if (fallbackToService) {
                            if (!fetchFaviconFromService(suggestion, snippetUri,
                                        faviconFetchStartTimeMs, faviconSizePx, faviconCallback)) {
                                recordFaviconFetchResult(suggestion, FaviconFetchResult.FAILURE,
                                        faviconFetchStartTimeMs);
                            }
                        }
                    }
                });
    }

    private void fetchFaviconFromLocalCacheOrGoogleServer(SnippetArticle suggestion,
            final long faviconFetchStartTimeMs, final Callback<Bitmap> faviconCallback) {
        // Set the desired size to 0 to specify we do not want to resize in c++, we'll resize here.
        mSuggestionsSource.fetchSuggestionFavicon(suggestion, PUBLISHER_FAVICON_MINIMUM_SIZE_PX,
                /* desiredSizePx */ 0, new Callback<Bitmap>() {
                    @Override
                    public void onResult(Bitmap image) {
                        recordFaviconFetchTime(faviconFetchStartTimeMs);
                        if (image == null) return;
                        faviconCallback.onResult(image);
                    }
                });
    }

    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("DefaultLocale")
    private boolean fetchFaviconFromService(final SnippetArticle suggestion, final URI snippetUri,
            final long faviconFetchStartTimeMs, final int faviconSizePx,
            final Callback<Bitmap> faviconCallback) {
        if (!mUseFaviconService) return false;
        int sizePx = getFaviconServiceSupportedSize(faviconSizePx);
        if (sizePx == 0) return false;

        // Replace the default icon by another one from the service when it is fetched.
        ensureIconIsAvailable(
                getSnippetDomain(snippetUri), // Store to the cache for the whole domain.
                String.format(FAVICON_SERVICE_FORMAT, snippetUri.getHost(), sizePx),
                /* useLargeIcon = */ false, /* isTemporary = */ true,
                new FaviconHelper.IconAvailabilityCallback() {
                    @Override
                    public void onIconAvailabilityChecked(boolean newlyAvailable) {
                        if (!newlyAvailable) {
                            recordFaviconFetchResult(suggestion, FaviconFetchResult.FAILURE,
                                    faviconFetchStartTimeMs);
                            return;
                        }
                        // The download succeeded, the favicon is in the cache; fetch it.
                        fetchFaviconFromLocalCache(snippetUri, /* fallbackToService = */ false,
                                faviconFetchStartTimeMs, faviconSizePx, suggestion,
                                faviconCallback);
                    }
                });
        return true;
    }

    private void recordFaviconFetchTime(long faviconFetchStartTimeMs) {
        RecordHistogram.recordMediumTimesHistogram(
                "NewTabPage.ContentSuggestions.ArticleFaviconFetchTime",
                SystemClock.elapsedRealtime() - faviconFetchStartTimeMs, TimeUnit.MILLISECONDS);
    }

    private void recordFaviconFetchResult(SnippetArticle suggestion, @FaviconFetchResult int result,
            long faviconFetchStartTimeMs) {
        // Record the histogram for articles only to have a fair comparision.
        if (!suggestion.isArticle()) return;
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.ContentSuggestions.ArticleFaviconFetchResult", result,
                FaviconFetchResult.COUNT);
        recordFaviconFetchTime(faviconFetchStartTimeMs);
    }

    /**
     * Checks if an icon with the given URL is available. If not,
     * downloads it and stores it as a favicon/large icon for the given {@code pageUrl}.
     * @param pageUrl The URL of the site whose icon is being requested.
     * @param iconUrl The URL of the favicon/large icon.
     * @param isLargeIcon Whether the {@code iconUrl} represents a large icon or favicon.
     * @param callback The callback to be notified when the favicon has been checked.
     */
    private void ensureIconIsAvailable(String pageUrl, String iconUrl, boolean isLargeIcon,
            boolean isTemporary, FaviconHelper.IconAvailabilityCallback callback) {
        if (mHost.getActiveTab().getWebContents() != null) {
            getFaviconHelper().ensureIconIsAvailable(mProfile,
                    mHost.getActiveTab().getWebContents(), pageUrl, iconUrl, isLargeIcon,
                    isTemporary, callback);
        }
    }

    private int getFaviconServiceSupportedSize(int faviconSizePX) {
        // Take the smallest size larger than mFaviconSizePx.
        for (int size : FAVICON_SERVICE_SUPPORTED_SIZES) {
            if (size > faviconSizePX) return size;
        }
        // Or at least the largest available size (unless too small).
        int largestSize =
                FAVICON_SERVICE_SUPPORTED_SIZES[FAVICON_SERVICE_SUPPORTED_SIZES.length - 1];
        if (faviconSizePX <= largestSize * 1.5) return largestSize;
        return 0;
    }

    public static String getSnippetDomain(URI snippetUri) {
        return String.format("%s://%s", snippetUri.getScheme(), snippetUri.getHost());
    }

    /**
     * Utility method to lazily create the {@link ThumbnailProvider}, and avoid unnecessary native
     * calls in tests.
     */
    private ThumbnailProvider getThumbnailProvider() {
        if (mThumbnailProvider == null) {
            mThumbnailProvider =
                    SuggestionsDependencyFactory.getInstance().createThumbnailProvider();
        }
        return mThumbnailProvider;
    }

    /**
     * Utility method to lazily create the {@link FaviconHelper}, and avoid unnecessary native
     * calls in tests.
     */
    @VisibleForTesting
    protected FaviconHelper getFaviconHelper() {
        if (mFaviconHelper == null) {
            mFaviconHelper = SuggestionsDependencyFactory.getInstance().createFaviconHelper();
        }
        return mFaviconHelper;
    }

    /**
     * Utility method to lazily create the {@link LargeIconBridge}, and avoid unnecessary native
     * calls in tests.
     */
    @VisibleForTesting
    protected LargeIconBridge getLargeIconBridge() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge =
                    SuggestionsDependencyFactory.getInstance().createLargeIconBridge(mProfile);
        }
        return mLargeIconBridge;
    }

    /**
     * Request for a download thumbnail.
     *
     * Cancellation of the request is available through {@link #cancel(), which will remove the
     * request from the ThumbnailProvider queue}.
     */
    public class DownloadThumbnailRequest implements ThumbnailProvider.ThumbnailRequest {
        private final SnippetArticle mSuggestion;
        private final Callback<Bitmap> mCallback;
        private final int mSize;

        DownloadThumbnailRequest(SnippetArticle suggestion, Callback<Bitmap> callback, int size) {
            mSuggestion = suggestion;
            mCallback = callback;
            mSize = size;

            // Fetch the download thumbnail.
            getThumbnailProvider().getThumbnail(this);
        }

        @Override
        public String getFilePath() {
            return mSuggestion.getAssetDownloadFile().getAbsolutePath();
        }

        @Override
        public void onThumbnailRetrieved(String filePath, Bitmap thumbnail) {
            mCallback.onResult(thumbnail);
        }

        @Override
        public int getIconSize() {
            return mSize;
        }

        public void cancel() {
            if (mIsDestroyed) return;
            getThumbnailProvider().cancelRetrieval(this);
        }
    }
}
