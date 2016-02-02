// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Build;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.CommandLine;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.FaviconHelper.IconAvailabilityCallback;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.MostVisitedItem.MostVisitedItemManager;
import org.chromium.chrome.browser.ntp.NewTabPage.OnSearchBoxScrollListener;
import org.chromium.chrome.browser.ntp.snippets.SnippetsManager;
import org.chromium.chrome.browser.preferences.DocumentModeManager;
import org.chromium.chrome.browser.profiles.MostVisitedSites.MostVisitedURLsObserver;
import org.chromium.chrome.browser.profiles.MostVisitedSites.ThumbnailCallback;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.Locale;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * The native new tab page, represented by some basic data such as title and url, and an Android
 * View that displays the page.
 */
public class NewTabPageView extends FrameLayout
        implements MostVisitedURLsObserver, OnLayoutChangeListener {

    private static final int SHADOW_COLOR = 0x11000000;
    private static final long SNAP_SCROLL_DELAY_MS = 30;

    private ViewGroup mContentView;
    private NewTabScrollView mScrollView;
    private LogoView mSearchProviderLogoView;
    private View mSearchBoxView;
    private TextView mSearchBoxTextView;
    private ImageView mVoiceSearchButton;
    private MostVisitedLayout mMostVisitedLayout;
    private View mMostVisitedPlaceholder;
    private View mOptOutView;
    private View mNoSearchLogoSpacer;
    private RecyclerView mSnippetsView;

    private OnSearchBoxScrollListener mSearchBoxScrollListener;

    private NewTabPageManager mManager;
    private MostVisitedDesign mMostVisitedDesign;
    private MostVisitedItem[] mMostVisitedItems;
    private SnippetsManager mSnippetsManager;
    private boolean mFirstShow = true;
    private boolean mSearchProviderHasLogo = true;
    private boolean mHasReceivedMostVisitedSites;
    private boolean mPendingSnapScroll;

    /**
     * The number of asynchronous tasks that need to complete before the page is done loading.
     * This starts at one to track when the view is finished attaching to the window.
     */
    private int mPendingLoadTasks = 1;
    private boolean mLoadHasCompleted;

    private float mUrlFocusChangePercent;
    private boolean mDisableUrlFocusChangeAnimations;

    private boolean mSnapshotMostVisitedChanged;
    private int mSnapshotWidth;
    private int mSnapshotHeight;
    private int mSnapshotScrollY;

    /**
     * Manages the view interaction with the rest of the system.
     */
    public interface NewTabPageManager extends MostVisitedItemManager {
        /** @return Whether the location bar is shown in the NTP. */
        boolean isLocationBarShownInNTP();

        /** @return Whether voice search is enabled and the microphone should be shown. */
        boolean isVoiceSearchEnabled();

        /** @return Whether the NTP Interests tab is enabled and its button should be shown. */
        boolean isInterestsEnabled();

        /** @return Whether the document mode opt out promo should be shown. */
        boolean shouldShowOptOutPromo();

        /** Called when the document mode opt out promo is shown. */
        void optOutPromoShown();

        /** Called when the user clicks "settings" or "ok, got it" on the opt out promo. */
        void optOutPromoClicked(boolean settingsClicked);

        /** Opens the bookmarks page in the current tab. */
        void navigateToBookmarks();

        /** Opens the recent tabs page in the current tab. */
        void navigateToRecentTabs();

        /** Opens a given URL in the current tab. */
        void open(String url);

        /** Opens the interests dialog. */
        void navigateToInterests();

        /**
         * Animates the search box up into the omnibox and bring up the keyboard.
         * @param beginVoiceSearch Whether to begin a voice search.
         * @param pastedText Text to paste in the omnibox after it's been focused. May be null.
         */
        void focusSearchBox(boolean beginVoiceSearch, String pastedText);

        /**
         * Gets the list of most visited sites.
         * @param observer The observer to be notified with the list of sites.
         * @param numResults The maximum number of sites to retrieve.
         */
        void setMostVisitedURLsObserver(MostVisitedURLsObserver observer, int numResults);

        /**
         * Gets a cached thumbnail of a URL.
         * @param url The URL whose thumbnail is being retrieved.
         * @param thumbnailCallback The callback to be notified when the thumbnail is available.
         */
        void getURLThumbnail(String url, ThumbnailCallback thumbnailCallback);

        /**
         * Gets the favicon image for a given URL.
         * @param url The URL of the site whose favicon is being requested.
         * @param size The desired size of the favicon in pixels.
         * @param faviconCallback The callback to be notified when the favicon is available.
         */
        void getLocalFaviconImageForURL(
                String url, int size, FaviconImageCallback faviconCallback);

        /**
         * Gets the large icon (e.g. favicon or touch icon) for a given URL.
         * @param url The URL of the site whose icon is being requested.
         * @param size The desired size of the icon in pixels.
         * @param callback The callback to be notified when the icon is available.
         */
        void getLargeIconForUrl(String url, int size, LargeIconCallback callback);

        /**
         * Checks if an icon with the given URL is available. If not,
         * downloads it and stores it as a favicon/large icon for the given {@code pageUrl}.
         * @param pageUrl The URL of the site whose icon is being requested.
         * @param iconUrl The URL of the favicon/large icon.
         * @param isLargeIcon Whether the {@code iconUrl} represents a large icon or favicon.
         * @param callback The callback to be notified when the favicon has been checked.
         */
        void ensureIconIsAvailable(String pageUrl, String iconUrl, boolean isLargeIcon,
                IconAvailabilityCallback callback);

        /**
         * Checks if the page with the given URL is available offline.
         * @param pageUrl The URL of the site whose offline availability is requested.
         */
        boolean isOfflineAvailable(String pageUrl);

        /**
         * Called when the user clicks on the logo.
         * @param isAnimatedLogoShowing Whether the animated GIF logo is playing.
         */
        void onLogoClicked(boolean isAnimatedLogoShowing);

        /**
         * Gets the default search provider's logo and calls logoObserver with the result.
         * @param logoObserver The callback to notify when the logo is available.
         */
        void getSearchProviderLogo(LogoObserver logoObserver);

        /**
         * Called when the NTP has completely finished loading (all views will be inflated
         * and any dependent resources will have been loaded).
         * @param mostVisitedItems The MostVisitedItem shown on the NTP. Used to record metrics.
         */
        void onLoadingComplete(MostVisitedItem[] mostVisitedItems);
    }

    /**
     * Returns a title suitable for display for a link (e.g. a most visited item). If |title| is
     * non-empty, this simply returns it. Otherwise, returns a shortened form of the URL.
     */
    static String getTitleForDisplay(String title, String url) {
        if (TextUtils.isEmpty(title) && url != null) {
            Uri uri = Uri.parse(url);
            String host = uri.getHost();
            String path = uri.getPath();
            if (host == null) host = "";
            if (TextUtils.isEmpty(path) || path.equals("/")) path = "";
            title = host + path;
        }
        return title;
    }

    /**
     * Default constructor required for XML inflation.
     */
    public NewTabPageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the NTP. This must be called immediately after inflation, before this object is
     * used in any other way.
     *
     * @param manager NewTabPageManager used to perform various actions when the user interacts
     *                with the page.
     * @param isSingleUrlBarMode Whether the NTP is in single URL bar mode.
     * @param searchProviderHasLogo Whether the search provider has a logo.
     */
    public void initialize(NewTabPageManager manager, boolean isSingleUrlBarMode,
            boolean searchProviderHasLogo) {
        mManager = manager;

        mScrollView = (NewTabScrollView) findViewById(R.id.ntp_scrollview);
        mScrollView.enableBottomShadow(SHADOW_COLOR);
        mContentView = (ViewGroup) findViewById(R.id.ntp_content);

        mMostVisitedDesign = new MostVisitedDesign(getContext());
        mMostVisitedLayout = (MostVisitedLayout) findViewById(R.id.most_visited_layout);
        mMostVisitedDesign.initMostVisitedLayout(mMostVisitedLayout, searchProviderHasLogo);

        mSearchProviderLogoView = (LogoView) findViewById(R.id.search_provider_logo);
        mSearchBoxView = findViewById(R.id.search_box);
        mNoSearchLogoSpacer = findViewById(R.id.no_search_logo_spacer);

        mSearchBoxTextView = (TextView) mSearchBoxView.findViewById(R.id.search_box_text);
        String hintText = getResources().getString(R.string.search_or_type_url);
        if (isSingleUrlBarMode) {
            mSearchBoxTextView.setHint(hintText);
        } else {
            mSearchBoxTextView.setContentDescription(hintText);
        }
        mSearchBoxTextView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.focusSearchBox(false, null);
            }
        });
        mSearchBoxTextView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (s.length() == 0) return;
                mManager.focusSearchBox(false, s.toString());
                mSearchBoxTextView.setText("");
            }
        });

        mVoiceSearchButton = (ImageView) findViewById(R.id.voice_search_button);
        mVoiceSearchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.focusSearchBox(true, null);
            }
        });

        NewTabPageToolbar toolbar = (NewTabPageToolbar) findViewById(R.id.ntp_toolbar);
        toolbar.getRecentTabsButton().setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.navigateToRecentTabs();
            }
        });
        toolbar.getBookmarksButton().setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.navigateToBookmarks();
            }
        });
        toolbar.getInterestsButton().setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.navigateToInterests();
            }
        });

        initializeSearchBoxScrollHandling();
        addOnLayoutChangeListener(this);
        setSearchProviderHasLogo(searchProviderHasLogo);

        mPendingLoadTasks++;
        mManager.setMostVisitedURLsObserver(this,
                mMostVisitedDesign.getNumberOfTiles(searchProviderHasLogo));

        if (mManager.shouldShowOptOutPromo()) showOptOutPromo();

        // Set up snippets
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_NTP_SNIPPETS)) {
            mSnippetsView = (RecyclerView) findViewById(R.id.snippets_card_list);
            mSnippetsView.setVisibility(View.VISIBLE);
            RecordHistogram.recordEnumeratedHistogram(SnippetsManager.SNIPPETS_STATE_HISTOGRAM,
                    SnippetsManager.SNIPPETS_SHOWN, SnippetsManager.NUM_SNIPPETS_ACTIONS);
            mSnippetsView.setLayoutManager(new LinearLayoutManager(getContext()));
            mSnippetsManager = new SnippetsManager(mManager, mSnippetsView);
            mSnippetsView.addOnScrollListener(new RecyclerView.OnScrollListener() {
                private boolean mScrolledOnce = false;
                @Override
                public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                    if (newState != RecyclerView.SCROLL_STATE_DRAGGING) return;
                    RecordUserAction.record("MobileNTP.Snippets.Scrolled");
                    if (mScrolledOnce) return;
                    mScrolledOnce = true;
                    RecordHistogram.recordEnumeratedHistogram(
                            SnippetsManager.SNIPPETS_STATE_HISTOGRAM,
                            SnippetsManager.SNIPPETS_SCROLLED,
                            SnippetsManager.NUM_SNIPPETS_ACTIONS);
                }
            });
        }

        // Set up interests
        if (manager.isInterestsEnabled()) {
            toolbar.getInterestsButton().setVisibility(View.VISIBLE);
        }
    }

    private int getTabsMovedIllustration() {
        switch (Build.MANUFACTURER.toLowerCase(Locale.US)) {
            case "samsung":
                if (DocumentModeManager.isDeviceTabbedModeByDefault()) return 0;
                return R.drawable.tabs_moved_samsung;
            case "htc":
                return R.drawable.tabs_moved_htc;
            default:
                return R.drawable.tabs_moved_nexus;
        }
    }

    private void showOptOutPromo() {
        ViewStub optOutPromoStub = (ViewStub) findViewById(R.id.opt_out_promo_stub);
        mOptOutView = optOutPromoStub.inflate();
        // Fill in opt-out text with Settings link
        TextView optOutText = (TextView) mOptOutView.findViewById(R.id.opt_out_text);

        ClickableSpan settingsLink = new ClickableSpan() {
            @Override
            public void onClick(View view) {
                mManager.optOutPromoClicked(true);
            }

            // Disable underline on the link text.
            @Override
            public void updateDrawState(android.text.TextPaint textPaint) {
                super.updateDrawState(textPaint);
                textPaint.setUnderlineText(false);
            }
        };

        optOutText.setText(SpanApplier.applySpans(
                getContext().getString(R.string.tabs_and_apps_opt_out_text),
                new SpanInfo("<link>", "</link>", settingsLink)));
        optOutText.setMovementMethod(LinkMovementMethod.getInstance());

        ImageView illustration = (ImageView) mOptOutView.findViewById(R.id.tabs_moved_illustration);
        int resourceId = getTabsMovedIllustration();
        if (resourceId != 0) {
            illustration.setImageResource(getTabsMovedIllustration());
        } else {
            illustration.setImageDrawable(null);
        }

        mOptOutView.setVisibility(View.VISIBLE);
        mMostVisitedLayout.setVisibility(View.GONE);

        Button gotItButton = (Button) mOptOutView.findViewById(R.id.got_it_button);
        gotItButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mOptOutView.setVisibility(View.GONE);
                mMostVisitedLayout.setVisibility(View.VISIBLE);
                mManager.optOutPromoClicked(false);
                updateMostVisitedPlaceholderVisibility();
            }
        });
        mManager.optOutPromoShown();
    }

    private void updateSearchBoxOnScroll() {
        if (mDisableUrlFocusChangeAnimations) return;

        float percentage = 0;
        // During startup the view may not be fully initialized, so we only calculate the current
        // percentage if some basic view properties are sane.
        if (mScrollView.getHeight() != 0 && mSearchBoxView.getTop() != 0) {
            int scrollY = mScrollView.getScrollY();
            percentage = Math.max(
                    0f, Math.min(1f, scrollY / (float) mSearchBoxView.getTop()));
        }

        updateVisualsForToolbarTransition(percentage);

        if (mSearchBoxScrollListener != null) {
            mSearchBoxScrollListener.onNtpScrollChanged(percentage);
        }
    }

    private void initializeSearchBoxScrollHandling() {
        final Runnable mSnapScrollRunnable = new Runnable() {
            @Override
            public void run() {
                if (!mPendingSnapScroll) return;
                int scrollY = mScrollView.getScrollY();
                int dividerTop = mMostVisitedLayout.getTop() - mContentView.getPaddingTop();
                if (scrollY > 0 && scrollY < dividerTop) {
                    mScrollView.smoothScrollTo(0, scrollY < (dividerTop / 2) ? 0 : dividerTop);
                }
                mPendingSnapScroll = false;
            }
        };
        mScrollView.setOnScrollListener(new NewTabScrollView.OnScrollListener() {
            @Override
            public void onScrollChanged(int l, int t, int oldl, int oldt) {
                if (mPendingSnapScroll) {
                    mScrollView.removeCallbacks(mSnapScrollRunnable);
                    mScrollView.postDelayed(mSnapScrollRunnable, SNAP_SCROLL_DELAY_MS);
                }
                updateSearchBoxOnScroll();
            }
        });
        mScrollView.setOnTouchListener(new OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View v, MotionEvent event) {
                if (mScrollView.getHandler() == null) return false;
                mScrollView.removeCallbacks(mSnapScrollRunnable);

                if (event.getActionMasked() == MotionEvent.ACTION_CANCEL
                        || event.getActionMasked() == MotionEvent.ACTION_UP) {
                    mPendingSnapScroll = true;
                    mScrollView.postDelayed(mSnapScrollRunnable, SNAP_SCROLL_DELAY_MS);
                } else {
                    mPendingSnapScroll = false;
                }
                return false;
            }
        });
    }

    /**
     * Decrements the count of pending load tasks and notifies the manager when the page load
     * is complete.
     */
    private void loadTaskCompleted() {
        assert mPendingLoadTasks > 0;
        mPendingLoadTasks--;
        if (mPendingLoadTasks == 0) {
            if (mLoadHasCompleted) {
                assert false;
            } else {
                mLoadHasCompleted = true;
                mManager.onLoadingComplete(mMostVisitedItems);
                // Load the logo after everything else is finished, since it's lower priority.
                loadSearchProviderLogo();
            }
        }
    }

    /**
     * Loads the search provider logo (e.g. Google doodle), if any.
     */
    private void loadSearchProviderLogo() {
        mManager.getSearchProviderLogo(new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (logo == null && fromCache) return;
                mSearchProviderLogoView.setMananger(mManager);
                mSearchProviderLogoView.updateLogo(logo);
                mSnapshotMostVisitedChanged = true;
            }
        });
    }

    /**
     * Changes the layout depending on whether the selected search provider (e.g. Google, Bing)
     * has a logo.
     * @param hasLogo Whether the search provider has a logo.
     */
    public void setSearchProviderHasLogo(boolean hasLogo) {
        if (hasLogo == mSearchProviderHasLogo) return;
        mSearchProviderHasLogo = hasLogo;

        mMostVisitedDesign.setSearchProviderHasLogo(mMostVisitedLayout, hasLogo);

        if (!hasLogo) setUrlFocusChangeAnimationPercentInternal(0);

        // Hide or show all the views above the Most Visited items.
        int visibility = hasLogo ? View.VISIBLE : View.GONE;
        int childCount = mContentView.getChildCount();
        for (int i = 0; i < childCount; i++) {
            View child = mContentView.getChildAt(i);
            if (child == mMostVisitedLayout) break;
            // Don't change the visibility of a ViewStub as that will automagically inflate it.
            if (child instanceof ViewStub) continue;
            child.setVisibility(visibility);
        }

        updateMostVisitedPlaceholderVisibility();

        if (hasLogo) setUrlFocusChangeAnimationPercent(mUrlFocusChangePercent);
        mSnapshotMostVisitedChanged = true;
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     * @param disable Whether to disable the animations.
     */
    void setUrlFocusAnimationsDisabled(boolean disable) {
        if (disable == mDisableUrlFocusChangeAnimations) return;
        mDisableUrlFocusChangeAnimations = disable;
        if (!disable) setUrlFocusChangeAnimationPercent(mUrlFocusChangePercent);
    }

    /**
     * Shows a progressbar indicating the animated logo is being downloaded.
     */
    void showLogoLoadingView() {
        mSearchProviderLogoView.showLoadingView();
    }

    /**
     * Starts playing the given animated GIF logo.
     */
    void playAnimatedLogo(BaseGifImage gifImage) {
        mSearchProviderLogoView.playAnimatedLogo(gifImage);
    }

    /**
     * @return Whether the GIF animation is playing in the logo.
     */
    boolean isAnimatedLogoShowing() {
        return mSearchProviderLogoView.isAnimatedLogoShowing();
    }

    /**
     * @return Whether URL focus animations are currently disabled.
     */
    boolean urlFocusAnimationsDisabled() {
        return mDisableUrlFocusChangeAnimations;
    }

    /**
     * Specifies the percentage the URL is focused during an animation.  1.0 specifies that the URL
     * bar has focus and has completed the focus animation.  0 is when the URL bar is does not have
     * any focus.
     *
     * @param percent The percentage of the URL bar focus animation.
     */
    void setUrlFocusChangeAnimationPercent(float percent) {
        mUrlFocusChangePercent = percent;
        if (!mDisableUrlFocusChangeAnimations && mSearchProviderHasLogo) {
            setUrlFocusChangeAnimationPercentInternal(percent);
        }
    }

    /**
     * @return The percentage that the URL bar is focused during an animation.
     */
    @VisibleForTesting
    float getUrlFocusChangeAnimationPercent() {
        return mUrlFocusChangePercent;
    }

    /**
     * Unconditionally sets the percentage the URL is focused during an animation, without updating
     * mUrlFocusChangePercent.
     * @see #setUrlFocusChangeAnimationPercent
     */
    private void setUrlFocusChangeAnimationPercentInternal(float percent) {
        mContentView.setTranslationY(percent * (-mMostVisitedLayout.getTop()
                + mScrollView.getScrollY() + mContentView.getPaddingTop()));
        updateVisualsForToolbarTransition(percent);
    }

    private void updateVisualsForToolbarTransition(float transitionPercentage) {
        // Complete the full alpha transition in the first 40% of the animation.
        float searchUiAlpha =
                transitionPercentage >= 0.4f ? 0f : (0.4f - transitionPercentage) * 2.5f;
        // Ensure there are no rounding issues when the animation percent is 0.
        if (transitionPercentage == 0f) searchUiAlpha = 1f;

        mSearchProviderLogoView.setAlpha(searchUiAlpha);
        mSearchBoxView.setAlpha(searchUiAlpha);
    }

    /**
     * Get the bounds of the search box in relation to the top level NewTabPage view.
     *
     * @param originalBounds The bounding region of the search box without external transforms
     *                       applied.  The delta between this and the transformed bounds determines
     *                       the amount of scroll applied to this view.
     * @param transformedBounds The bounding region of the search box including any transforms
     *                          applied by the parent view hierarchy up to the NewTabPage view.
     *                          This more accurately reflects the current drawing location of the
     *                          search box.
     */
    void getSearchBoxBounds(Rect originalBounds, Rect transformedBounds) {
        int searchBoxX = (int) mSearchBoxView.getX();
        int searchBoxY = (int) mSearchBoxView.getY();
        originalBounds.set(
                searchBoxX + mSearchBoxView.getPaddingLeft(),
                searchBoxY + mSearchBoxView.getPaddingTop(),
                searchBoxX + mSearchBoxView.getWidth() - mSearchBoxView.getPaddingRight(),
                searchBoxY + mSearchBoxView.getHeight() - mSearchBoxView.getPaddingBottom());

        transformedBounds.set(originalBounds);
        View view = (View) mSearchBoxView.getParent();
        while (view != null) {
            transformedBounds.offset(-view.getScrollX(), -view.getScrollY());
            if (view == this) break;
            transformedBounds.offset((int) view.getX(), (int) view.getY());
            view = (View) view.getParent();
        }
    }

    /**
     * Sets the listener for search box scroll changes.
     * @param listener The listener to be notified on changes.
     */
    void setSearchBoxScrollListener(OnSearchBoxScrollListener listener) {
        mSearchBoxScrollListener = listener;
        if (mSearchBoxScrollListener != null) updateSearchBoxOnScroll();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        assert mManager != null;

        if (mFirstShow) {
            loadTaskCompleted();
            mFirstShow = false;
        } else {
            // Trigger a scroll update when reattaching the window to signal the toolbar that
            // it needs to reset the NTP state.
            if (mManager.isLocationBarShownInNTP()) updateSearchBoxOnScroll();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        setUrlFocusChangeAnimationPercent(0f);
    }

    /**
     * Update the visibility of the voice search button based on whether the feature is currently
     * enabled.
     */
    void updateVoiceSearchButtonVisibility() {
        mVoiceSearchButton.setVisibility(mManager.isVoiceSearchEnabled() ? VISIBLE : GONE);
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (visibility == VISIBLE) {
            updateVoiceSearchButtonVisibility();
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Make the search box and logo the same width as the most visited tiles.
        if (mMostVisitedLayout.getVisibility() != GONE) {
            int mostVisitedWidth = MeasureSpec.makeMeasureSpec(mMostVisitedLayout.getMeasuredWidth()
                    - mMostVisitedDesign.getMostVisitedLayoutBleed(), MeasureSpec.EXACTLY);
            int searchBoxHeight = MeasureSpec.makeMeasureSpec(
                    mSearchBoxView.getMeasuredHeight(), MeasureSpec.EXACTLY);
            int logoHeight = MeasureSpec.makeMeasureSpec(
                    mSearchProviderLogoView.getMeasuredHeight(), MeasureSpec.EXACTLY);
            mSearchBoxView.measure(mostVisitedWidth, searchBoxHeight);
            mSearchProviderLogoView.measure(mostVisitedWidth, logoHeight);
        }
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *         InvalidationAwareThumbnailProvider#shouldCaptureThumbnail()
     */
    boolean shouldCaptureThumbnail() {
        if (getWidth() == 0 || getHeight() == 0) return false;

        return mSnapshotMostVisitedChanged
                || getWidth() != mSnapshotWidth
                || getHeight() != mSnapshotHeight
                || mScrollView.getScrollY() != mSnapshotScrollY;
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *         InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    void captureThumbnail(Canvas canvas) {
        mSearchProviderLogoView.endFadeAnimation();
        ViewUtils.captureBitmap(this, canvas);
        mSnapshotWidth = getWidth();
        mSnapshotHeight = getHeight();
        mSnapshotScrollY = mScrollView.getScrollY();
        mSnapshotMostVisitedChanged = false;
    }

    // OnLayoutChangeListener overrides

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom,
            int oldLeft, int oldTop, int oldRight, int oldBottom) {
        int oldWidth = oldRight - oldLeft;
        int newWidth = right - left;
        if (oldWidth == newWidth) return;

        // Re-apply the url focus change amount after a rotation to ensure the views are correctly
        // placed with their new layout configurations.
        setUrlFocusChangeAnimationPercent(mUrlFocusChangePercent);
        updateSearchBoxOnScroll();
    }

    // MostVisitedURLsObserver implementation

    @Override
    public void onMostVisitedURLsAvailable(String[] titles, String[] urls) {
        mMostVisitedLayout.removeAllViews();

        MostVisitedItem[] oldItems = mMostVisitedItems;
        int oldItemCount = oldItems == null ? 0 : oldItems.length;
        mMostVisitedItems = new MostVisitedItem[titles.length];

        final boolean isInitialLoad = !mHasReceivedMostVisitedSites;
        LayoutInflater inflater = LayoutInflater.from(getContext());

        // Add the most visited items to the page.
        for (int i = 0; i < titles.length; i++) {
            final String url = urls[i];
            final String title = titles[i];

            // Look for an existing item to reuse.
            MostVisitedItem item = null;
            for (int j = 0; j < oldItemCount; j++) {
                MostVisitedItem oldItem = oldItems[j];
                if (oldItem != null && TextUtils.equals(url, oldItem.getUrl())
                        && TextUtils.equals(title, oldItem.getTitle())) {
                    item = oldItem;
                    item.setIndex(i);
                    oldItems[j] = null;
                    break;
                }
            }

            // If nothing can be reused, create a new item.
            if (item == null) {
                String displayTitle = getTitleForDisplay(title, url);
                item = new MostVisitedItem(mManager, title, url, i);
                View view = mMostVisitedDesign.createMostVisitedItemView(inflater, url,
                        displayTitle, item, isInitialLoad);
                item.initView(view);
            }

            mMostVisitedItems[i] = item;
            mMostVisitedLayout.addView(item.getView());
        }

        mHasReceivedMostVisitedSites = true;
        updateMostVisitedPlaceholderVisibility();

        if (isInitialLoad) {
            loadTaskCompleted();
            // The page contents are initially hidden; otherwise they'll be drawn centered on the
            // page before the most visited sites are available and then jump upwards to make space
            // once the most visited sites are available.
            mContentView.setVisibility(View.VISIBLE);
        }
        mSnapshotMostVisitedChanged = true;
    }

    @Override
    public void onPopularURLsAvailable(
            String[] urls, String[] faviconUrls, String[] largeIconUrls) {
        for (int i = 0; i < urls.length; i++) {
            final String url = urls[i];
            boolean useLargeIcon = !largeIconUrls[i].isEmpty();
            // Only fetch one of favicon or large icon based on what is required on the NTP.
            // The other will be fetched on visiting the site.
            String iconUrl = useLargeIcon ? largeIconUrls[i] : faviconUrls[i];
            if (iconUrl.isEmpty()) continue;

            IconAvailabilityCallback callback = new IconAvailabilityCallback() {
                @Override
                public void onIconAvailabilityChecked(boolean newlyAvailable) {
                    if (newlyAvailable) {
                        mMostVisitedDesign.onIconUpdated(url);
                    }
                }
            };
            mManager.ensureIconIsAvailable(url, iconUrl, useLargeIcon, callback);
        }
    }

    /**
     * Shows the most visited placeholder ("Nothing to see here") if there are no most visited
     * items and there is no search provider logo.
     */
    private void updateMostVisitedPlaceholderVisibility() {
        boolean showPlaceholder = mHasReceivedMostVisitedSites
                && !mManager.shouldShowOptOutPromo()
                && mMostVisitedLayout.getChildCount() == 0
                && !mSearchProviderHasLogo;

        mNoSearchLogoSpacer.setVisibility(
                (mSearchProviderHasLogo || showPlaceholder) ? View.GONE : View.INVISIBLE);

        if (showPlaceholder) {
            if (mMostVisitedPlaceholder == null) {
                ViewStub mostVisitedPlaceholderStub = (ViewStub) findViewById(
                        R.id.most_visited_placeholder_stub);
                mMostVisitedPlaceholder = mostVisitedPlaceholderStub.inflate();
            }
            mMostVisitedLayout.setVisibility(GONE);
            mMostVisitedPlaceholder.setVisibility(VISIBLE);
            return;
        } else if (mMostVisitedPlaceholder != null) {
            mMostVisitedLayout.setVisibility(VISIBLE);
            mMostVisitedPlaceholder.setVisibility(GONE);
        }
    }

    /**
     * The design for most visited tiles: each tile shows a large icon and the site's title.
     */
    private class MostVisitedDesign {

        private static final int NUM_TILES = 8;
        private static final int NUM_TILES_NO_LOGO = 12;
        private static final int MAX_ROWS = 2;
        private static final int MAX_ROWS_NO_LOGO = 3;

        private static final int ICON_CORNER_RADIUS_DP = 4;
        private static final int ICON_TEXT_SIZE_DP = 20;
        private static final int ICON_BACKGROUND_COLOR = 0xff787878;
        private static final int ICON_MIN_SIZE_PX = 48;

        private int mMostVisitedLayoutBleed;
        private int mMinIconSize;
        private int mDesiredIconSize;
        private RoundedIconGenerator mIconGenerator;

        MostVisitedDesign(Context context) {
            Resources res = context.getResources();
            mMostVisitedLayoutBleed = res.getDimensionPixelSize(
                    R.dimen.most_visited_layout_bleed);
            mDesiredIconSize = res.getDimensionPixelSize(R.dimen.most_visited_icon_size);
            // On ldpi devices, mDesiredIconSize could be even smaller than ICON_MIN_SIZE_PX.
            mMinIconSize = Math.min(mDesiredIconSize, ICON_MIN_SIZE_PX);
            int desiredIconSizeDp = Math.round(
                    mDesiredIconSize / res.getDisplayMetrics().density);
            mIconGenerator = new RoundedIconGenerator(
                    context, desiredIconSizeDp, desiredIconSizeDp, ICON_CORNER_RADIUS_DP,
                    ICON_BACKGROUND_COLOR, ICON_TEXT_SIZE_DP);
        }

        public int getNumberOfTiles(boolean searchProviderHasLogo) {
            return searchProviderHasLogo ? NUM_TILES : NUM_TILES_NO_LOGO;
        }

        public int getMostVisitedLayoutBleed() {
            return mMostVisitedLayoutBleed;
        }

        public void initMostVisitedLayout(MostVisitedLayout mostVisitedLayout,
                boolean searchProviderHasLogo) {
            mostVisitedLayout.setMaxRows(searchProviderHasLogo ? MAX_ROWS : MAX_ROWS_NO_LOGO);
        }

        public void setSearchProviderHasLogo(View mostVisitedLayout, boolean hasLogo) {
            int paddingTop = getResources().getDimensionPixelSize(hasLogo
                    ? R.dimen.most_visited_layout_padding_top
                    : R.dimen.most_visited_layout_no_logo_padding_top);
            mostVisitedLayout.setPadding(0, paddingTop, 0, mMostVisitedLayout.getPaddingBottom());
        }

        class LargeIconCallbackImpl implements LargeIconCallback {
            private MostVisitedItem mItem;
            private boolean mIsInitialLoad;

            public LargeIconCallbackImpl(MostVisitedItem item, boolean isInitialLoad) {
                mItem = item;
                mIsInitialLoad = isInitialLoad;
            }

            @Override
            public void onLargeIconAvailable(Bitmap icon, int fallbackColor) {
                MostVisitedItemView view = (MostVisitedItemView) mItem.getView();
                if (icon == null) {
                    mIconGenerator.setBackgroundColor(fallbackColor);
                    icon = mIconGenerator.generateIconForUrl(mItem.getUrl());
                    view.setIcon(new BitmapDrawable(getResources(), icon));
                    mItem.setTileType(fallbackColor == ICON_BACKGROUND_COLOR
                            ? MostVisitedTileType.ICON_DEFAULT : MostVisitedTileType.ICON_COLOR);
                } else {
                    RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                            getResources(), icon);
                    int cornerRadius = Math.round(ICON_CORNER_RADIUS_DP
                            * getResources().getDisplayMetrics().density * icon.getWidth()
                            / mDesiredIconSize);
                    roundedIcon.setCornerRadius(cornerRadius);
                    roundedIcon.setAntiAlias(true);
                    roundedIcon.setFilterBitmap(true);
                    view.setIcon(roundedIcon);
                    mItem.setTileType(MostVisitedTileType.ICON_REAL);
                }
                mSnapshotMostVisitedChanged = true;
                if (mIsInitialLoad) loadTaskCompleted();
            }
        }

        public View createMostVisitedItemView(LayoutInflater inflater, final String url,
                String displayTitle, MostVisitedItem item,
                final boolean isInitialLoad) {
            final MostVisitedItemView view = (MostVisitedItemView) inflater.inflate(
                    R.layout.most_visited_item, mMostVisitedLayout, false);
            view.setTitle(displayTitle);
            view.setOfflineAvailable(mManager.isOfflineAvailable(url));

            LargeIconCallback iconCallback = new LargeIconCallbackImpl(item, isInitialLoad);
            if (isInitialLoad) mPendingLoadTasks++;
            mManager.getLargeIconForUrl(url, mMinIconSize, iconCallback);

            return view;
        }

        public void onIconUpdated(final String url) {
            if (mMostVisitedItems == null) return;
            // Find a matching most visited item.
            for (MostVisitedItem item : mMostVisitedItems) {
                if (item.getUrl().equals(url)) {
                    LargeIconCallback iconCallback = new LargeIconCallbackImpl(item, false);
                    mManager.getLargeIconForUrl(url, mMinIconSize, iconCallback);
                    break;
                }
            }
        }
    }
}
