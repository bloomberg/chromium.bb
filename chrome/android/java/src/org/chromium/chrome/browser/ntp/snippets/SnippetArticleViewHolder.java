// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.media.ThumbnailUtils;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.v4.text.BidiFormatter;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.ui.DownloadFilter;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.ntp.cards.CardViewHolder;
import org.chromium.chrome.browser.ntp.cards.ImpressionTracker;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.TintedImageView;
import org.chromium.chrome.browser.widget.displaystyle.DisplayStyleObserver;
import org.chromium.chrome.browser.widget.displaystyle.DisplayStyleObserverAdapter;
import org.chromium.chrome.browser.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.concurrent.TimeUnit;

/**
 * A class that represents the view for a single card snippet.
 */
public class SnippetArticleViewHolder extends CardViewHolder implements ImpressionTracker.Listener {
    /**
     * A single instance of {@link RefreshOfflineBadgeVisibilityCallback} that can be reused as it
     * has no state.
     */
    public static final RefreshOfflineBadgeVisibilityCallback
            REFRESH_OFFLINE_BADGE_VISIBILITY_CALLBACK = new RefreshOfflineBadgeVisibilityCallback();

    private static final String ARTICLE_AGE_FORMAT_STRING = " - %s";
    private static final int FADE_IN_ANIMATION_TIME_MS = 300;

    private final SuggestionsUiDelegate mUiDelegate;
    private final UiConfig mUiConfig;
    private final ImageFetcher mImageFetcher;

    private final LinearLayout mTextLayout;
    private final TextView mHeadlineTextView;
    private final TextView mPublisherTextView;
    private final TextView mArticleSnippetTextView;
    private final TextView mArticleAgeTextView;
    private final TintedImageView mThumbnailView;
    private final ImageView mOfflineBadge;
    private final View mPublisherBar;

    private final int mThumbnailSize;
    /** Total horizontal space occupied by the thumbnail, sum of its size and margin. */
    private final int mThumbnailFootprintPx;
    private final int mIconBackgroundColor;
    private final ColorStateList mIconForegroundColorList;

    @Nullable
    private ImageFetcher.DownloadThumbnailRequest mThumbnailRequest;
    private SnippetArticle mArticle;
    private SuggestionsCategoryInfo mCategoryInfo;
    private int mPublisherFaviconSizePx;

    /**
     * Constructs a {@link SnippetArticleViewHolder} item used to display snippets.
     * @param parent The SuggestionsRecyclerView that is going to contain the newly created view.
     * @param contextMenuManager The manager responsible for the context menu.
     * @param uiDelegate The delegate object used to open an article, fetch thumbnails, etc.
     * @param uiConfig The NTP UI configuration object used to adjust the article UI.
     */
    public SnippetArticleViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, SuggestionsUiDelegate uiDelegate,
            UiConfig uiConfig) {
        super(ChromeFeatureList.isEnabled(ChromeFeatureList.CONTENT_SUGGESTIONS_LARGE_THUMBNAIL)
                        ? R.layout.new_tab_page_snippets_card_large_thumbnail
                        : R.layout.new_tab_page_snippets_card,
                parent, uiConfig, contextMenuManager);

        mUiDelegate = uiDelegate;
        mUiConfig = uiConfig;
        mImageFetcher = mUiDelegate.getImageFetcher();

        mTextLayout = (LinearLayout) itemView.findViewById(R.id.text_layout);
        mHeadlineTextView = (TextView) itemView.findViewById(R.id.article_headline);
        mPublisherTextView = (TextView) itemView.findViewById(R.id.article_publisher);
        mArticleSnippetTextView = (TextView) itemView.findViewById(R.id.article_snippet);
        mArticleAgeTextView = (TextView) itemView.findViewById(R.id.article_age);
        mThumbnailView = (TintedImageView) itemView.findViewById(R.id.article_thumbnail);
        mPublisherBar = itemView.findViewById(R.id.publisher_bar);
        mOfflineBadge = (ImageView) itemView.findViewById(R.id.offline_icon);

        boolean useLargeThumbnailLayout =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CONTENT_SUGGESTIONS_LARGE_THUMBNAIL);
        mThumbnailSize = itemView.getResources().getDimensionPixelSize(useLargeThumbnailLayout
                        ? R.dimen.snippets_thumbnail_size_large
                        : R.dimen.snippets_thumbnail_size);
        mThumbnailFootprintPx = mThumbnailSize
                + itemView.getResources().getDimensionPixelSize(R.dimen.snippets_thumbnail_margin);

        mIconBackgroundColor = DownloadUtils.getIconBackgroundColor(parent.getContext());
        mIconForegroundColorList = DownloadUtils.getIconForegroundColorList(parent.getContext());

        new ImpressionTracker(itemView, this);
        new DisplayStyleObserverAdapter(itemView, uiConfig, new DisplayStyleObserver() {
            @Override
            public void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
                updateLayout();
            }
        });
    }

    @Override
    public void onImpression() {
        if (mArticle != null && mArticle.trackImpression()) {
            mUiDelegate.getEventReporter().onSuggestionShown(mArticle);
            mRecyclerView.onSnippetImpression();
        }
    }

    @Override
    public void onCardTapped() {
        SuggestionsMetrics.recordCardTapped();
        int windowDisposition = WindowOpenDisposition.CURRENT_TAB;
        mUiDelegate.getEventReporter().onSuggestionOpened(
                mArticle, windowDisposition, mUiDelegate.getSuggestionsRanker());
        mUiDelegate.getNavigationDelegate().openSnippet(windowDisposition, mArticle);
    }

    @Override
    public void openItem(int windowDisposition) {
        mUiDelegate.getEventReporter().onSuggestionOpened(
                mArticle, windowDisposition, mUiDelegate.getSuggestionsRanker());
        mUiDelegate.getNavigationDelegate().openSnippet(windowDisposition, mArticle);
    }

    @Override
    public String getUrl() {
        return mArticle.mUrl;
    }

    @Override
    public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
        Boolean isSupported = mCategoryInfo.isContextMenuItemSupported(menuItemId);
        if (isSupported != null) return isSupported;

        return super.isItemSupported(menuItemId);
    }

    @Override
    public void onContextMenuCreated() {
        mUiDelegate.getEventReporter().onSuggestionMenuOpened(mArticle);
    }

    /**
     * Updates ViewHolder with data.
     * @param article The snippet to take the data from.
     * @param categoryInfo The info of the category which the snippet belongs to.
     */
    public void onBindViewHolder(
            final SnippetArticle article, SuggestionsCategoryInfo categoryInfo) {
        super.onBindViewHolder();

        mArticle = article;
        mCategoryInfo = categoryInfo;
        updateLayout();

        mHeadlineTextView.setText(mArticle.mTitle);

        // The favicon of the publisher should match the TextView height.
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        mPublisherTextView.measure(widthSpec, heightSpec);
        mPublisherFaviconSizePx = mPublisherTextView.getMeasuredHeight();

        mArticleSnippetTextView.setText(mArticle.mPreviewText);
        mPublisherTextView.setText(getPublisherString(mArticle));
        mArticleAgeTextView.setText(getArticleAge(mArticle));
        setThumbnail();

        // Set the favicon of the publisher.
        // We start initialising with the default favicon to reserve the space and prevent the text
        // from moving later.
        setDefaultFaviconOnView();
        mImageFetcher.makeFaviconRequest(mArticle, mPublisherFaviconSizePx, new Callback<Bitmap>() {
            @Override
            public void onResult(Bitmap image) {
                setFaviconOnView(image);
            }
        });

        mOfflineBadge.setVisibility(View.GONE);
        refreshOfflineBadgeVisibility();
    }

    /**
     * Updates the layout taking into account screen dimensions and the type of snippet displayed.
     */
    private void updateLayout() {
        final int horizontalStyle = mUiConfig.getCurrentDisplayStyle().horizontal;
        final int verticalStyle = mUiConfig.getCurrentDisplayStyle().vertical;
        final int layout = mCategoryInfo.getCardLayout();

        boolean showHeadline = shouldShowHeadline();
        boolean showDescription = shouldShowDescription(horizontalStyle, verticalStyle, layout);
        boolean showThumbnail = shouldShowThumbnail(layout);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTENT_SUGGESTIONS_LARGE_THUMBNAIL)) {
            mTextLayout.setMinimumHeight(showThumbnail ? mThumbnailSize : 0);
        }
        mHeadlineTextView.setVisibility(showHeadline ? View.VISIBLE : View.GONE);
        mHeadlineTextView.setMaxLines(getHeaderMaxLines(horizontalStyle, verticalStyle, layout));
        mArticleSnippetTextView.setVisibility(showDescription ? View.VISIBLE : View.GONE);
        mThumbnailView.setVisibility(showThumbnail ? View.VISIBLE : View.GONE);

        ViewGroup.MarginLayoutParams publisherBarParams =
                (ViewGroup.MarginLayoutParams) mPublisherBar.getLayoutParams();

        if (showDescription) {
            publisherBarParams.topMargin = mPublisherBar.getResources().getDimensionPixelSize(
                    R.dimen.snippets_publisher_margin_top_with_article_snippet);
        } else if (showHeadline) {
            // When we show a headline and not a description, we reduce the top margin of the
            // publisher bar.
            publisherBarParams.topMargin = mPublisherBar.getResources().getDimensionPixelSize(
                    R.dimen.snippets_publisher_margin_top_without_article_snippet);
        } else {
            // When there is no headline and no description, we remove the top margin of the
            // publisher bar.
            publisherBarParams.topMargin = 0;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CONTENT_SUGGESTIONS_LARGE_THUMBNAIL)) {
            ApiCompatibilityUtils.setMarginEnd(
                    publisherBarParams, showThumbnail ? mThumbnailFootprintPx : 0);
        }
        mPublisherBar.setLayoutParams(publisherBarParams);
    }

    /** If the title is empty (or contains only whitespace characters), we do not show it. */
    private boolean shouldShowHeadline() {
        return !mArticle.mTitle.trim().isEmpty();
    }

    private boolean shouldShowDescription(int horizontalStyle, int verticalStyle, int layout) {
        // Minimal cards don't have a description.
        if (layout == ContentSuggestionsCardLayout.MINIMAL_CARD) return false;

        // When the screen is too small (narrow or flat) we don't show the description to have more
        // space for the header.
        if (horizontalStyle == HorizontalDisplayStyle.NARROW) return false;
        if (verticalStyle == VerticalDisplayStyle.FLAT) return false;

        // When article's description is empty, we do not want empty space.
        if (mArticle != null && TextUtils.isEmpty(mArticle.mPreviewText)) return false;

        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTENT_SUGGESTIONS_SHOW_SUMMARY);
    }

    private boolean shouldShowThumbnail(int layout) {
        // Minimal cards don't have a thumbnail
        if (layout == ContentSuggestionsCardLayout.MINIMAL_CARD) return false;

        return true;
    }

    /**
     * If no summary (no description) is shown, allow more lines for the header (title).
     * @return The maximum number of header text lines.
     */
    private int getHeaderMaxLines(int horizontalStyle, int verticalStyle, int layout) {
        return shouldShowDescription(horizontalStyle, verticalStyle, layout) ? 2 : 3;
    }

    private static String getPublisherString(SnippetArticle article) {
        // We format the publisher here so that having a publisher name in an RTL language
        // doesn't mess up the formatting on an LTR device and vice versa.
        return BidiFormatter.getInstance().unicodeWrap(article.mPublisher);
    }

    private static String getArticleAge(SnippetArticle article) {
        if (article.mPublishTimestampMilliseconds == 0) return "";

        // DateUtils.getRelativeTimeSpanString(...) calls through to TimeZone.getDefault(). If this
        // has never been called before it loads the current time zone from disk. In most likelihood
        // this will have been called previously and the current time zone will have been cached,
        // but in some cases (eg instrumentation tests) it will cause a strict mode violation.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        CharSequence relativeTimeSpan;
        try {
            long time = SystemClock.elapsedRealtime();
            relativeTimeSpan =
                    DateUtils.getRelativeTimeSpanString(article.mPublishTimestampMilliseconds,
                            System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS);
            RecordHistogram.recordTimesHistogram("Android.StrictMode.SnippetUIBuildTime",
                    SystemClock.elapsedRealtime() - time, TimeUnit.MILLISECONDS);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        // We add a dash before the elapsed time, e.g. " - 14 minutes ago".
        return String.format(ARTICLE_AGE_FORMAT_STRING,
                BidiFormatter.getInstance().unicodeWrap(relativeTimeSpan));
    }

    private void setThumbnailFromBitmap(Bitmap thumbnail) {
        assert thumbnail != null;
        assert !thumbnail.isRecycled();
        assert thumbnail.getWidth() == mThumbnailSize;
        assert thumbnail.getHeight() == mThumbnailSize;

        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        mThumbnailView.setBackground(null);
        mThumbnailView.setImageBitmap(thumbnail);
        mThumbnailView.setTint(null);
    }

    private void setThumbnailFromFileType(int fileType) {
        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        mThumbnailView.setBackgroundColor(mIconBackgroundColor);
        mThumbnailView.setImageResource(
                DownloadUtils.getIconResId(fileType, DownloadUtils.ICON_SIZE_36_DP));
        mThumbnailView.setTint(mIconForegroundColorList);
    }

    private void setDownloadThumbnail() {
        assert mArticle.isDownload();
        if (!mArticle.isAssetDownload()) {
            setThumbnailFromFileType(DownloadFilter.FILTER_PAGE);
            return;
        }

        int fileType = DownloadFilter.fromMimeType(mArticle.getAssetDownloadMimeType());
        if (fileType == DownloadFilter.FILTER_IMAGE) {
            // For image downloads, attempt to fetch a thumbnail.
            mThumbnailRequest = mImageFetcher.makeDownloadThumbnailRequest(
                    mArticle, mThumbnailSize, new FetchImageCallback(mArticle, mThumbnailSize));
        }
        setThumbnailFromFileType(fileType);
    }

    private void setThumbnail() {
        // If there's still a pending thumbnail fetch, cancel it.
        cancelImageFetch();

        // mThumbnailView's visibility is modified in updateLayout().
        if (mThumbnailView.getVisibility() != View.VISIBLE) return;
        Bitmap thumbnail = mArticle.getThumbnailBitmap();
        if (thumbnail != null) {
            setThumbnailFromBitmap(thumbnail);
            return;
        }

        if (mArticle.isDownload()) {
            setDownloadThumbnail();
            return;
        }

        // Temporarily set placeholder and then fetch the thumbnail from a provider.
        mThumbnailView.setBackground(null);
        mThumbnailView.setImageResource(R.drawable.ic_snippet_thumbnail_placeholder);
        mThumbnailView.setTint(null);
        mImageFetcher.makeArticleThumbnailRequest(
                mArticle, new FetchImageCallback(mArticle, mThumbnailSize));
    }

    /** Updates the visibility of the card's offline badge by checking the bound article's info. */
    private void refreshOfflineBadgeVisibility() {
        boolean visible = mArticle.getOfflinePageOfflineId() != null || mArticle.isAssetDownload();
        if (visible == (mOfflineBadge.getVisibility() == View.VISIBLE)) return;
        mOfflineBadge.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    private void cancelImageFetch() {
        if (mThumbnailRequest != null) {
            mThumbnailRequest.cancel();
            mThumbnailRequest = null;
        }
    }

    private void fadeThumbnailIn(Bitmap thumbnail) {
        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        mThumbnailView.setBackground(null);
        mThumbnailView.setTint(null);
        int duration = (int) (FADE_IN_ANIMATION_TIME_MS
                * ChromeAnimation.Animation.getAnimationMultiplier());
        if (duration == 0) {
            mThumbnailView.setImageBitmap(thumbnail);
            return;
        }

        // Cross-fade between the placeholder and the thumbnail. We cross-fade because the incoming
        // image may have transparency and we don't want the previous image showing up behind.
        Drawable[] layers = {mThumbnailView.getDrawable(),
                new BitmapDrawable(mThumbnailView.getResources(), thumbnail)};
        TransitionDrawable transitionDrawable = new TransitionDrawable(layers);
        mThumbnailView.setImageDrawable(transitionDrawable);
        transitionDrawable.setCrossFadeEnabled(true);
        transitionDrawable.startTransition(duration);
    }

    private void setDefaultFaviconOnView() {
        setFaviconOnView(ApiCompatibilityUtils.getDrawable(
                mPublisherTextView.getContext().getResources(), R.drawable.default_favicon));
    }

    private void setFaviconOnView(Bitmap image) {
        setFaviconOnView(new BitmapDrawable(mPublisherTextView.getContext().getResources(), image));
    }

    private void setFaviconOnView(Drawable drawable) {
        drawable.setBounds(0, 0, mPublisherFaviconSizePx, mPublisherFaviconSizePx);
        ApiCompatibilityUtils.setCompoundDrawablesRelative(
                mPublisherTextView, drawable, null, null, null);
        mPublisherTextView.setVisibility(View.VISIBLE);
    }

    private class FetchImageCallback extends Callback<Bitmap> {
        private final SnippetArticle mSuggestion;
        private final int mThumbnailSize;
        private final boolean mIsBitmapOwned;

        FetchImageCallback(SnippetArticle suggestion, int size) {
            mSuggestion = suggestion;
            mThumbnailSize = size;
            mIsBitmapOwned = suggestion.isDownload();
        }

        @Override
        public void onResult(Bitmap thumbnail) {
            if (thumbnail == null) return; // Nothing to do, we keep the placeholder.

            // We need to crop and scale the downloaded bitmap, as the ImageView we set it on won't
            // be able to do so when using a TransitionDrawable (as opposed to the straight bitmap).
            // That's a limitation of TransitionDrawable, which doesn't handle layers of varying
            // sizes.
            if (thumbnail.getHeight() != mThumbnailSize || thumbnail.getWidth() != mThumbnailSize) {
                // Resize the thumbnail. If we fully own the input bitmap (e.g. it isn't cached
                // anywhere else), recycle the input image in the process.
                thumbnail = ThumbnailUtils.extractThumbnail(thumbnail, mThumbnailSize,
                        mThumbnailSize, mIsBitmapOwned ? ThumbnailUtils.OPTIONS_RECYCLE_INPUT : 0);
            }

            // Store the bitmap to skip the download task next time we display this snippet.
            mSuggestion.setThumbnailBitmap(mUiDelegate.getReferencePool().put(thumbnail));

            if (mSuggestion != mArticle) return;

            fadeThumbnailIn(thumbnail);
        }
    }

    /**
     * Callback to refresh the offline badge visibility.
     */
    public static class RefreshOfflineBadgeVisibilityCallback extends PartialBindCallback {
        @Override
        public void onResult(NewTabPageViewHolder holder) {
            assert holder instanceof SnippetArticleViewHolder;
            ((SnippetArticleViewHolder) holder).refreshOfflineBadgeVisibility();
        }
    }
}
