// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Intent;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Handler;
import android.support.customtabs.CustomTabsIntent;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.omnibox.AutocompleteController;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Prototype that queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable, SearchLocationBarLayout.Delegate,
                   View.OnLayoutChangeListener {
    private static final String TAG = "searchwidget";

    /** How long the animation should run for. */
    private static final int ANIMATION_DURATION_MS = 200;

    /** Padding gleaned from the background Drawable of the search box. */
    private final Rect mSearchBoxPadding = new Rect();

    /** Location of the search box on the home screen. */
    private Rect mSearchBoxWidgetBounds;

    /** Small margin/padding used for the search box. */
    private int mSpacingSmall;

    /** Medium margin/padding used for the search box. */
    private int mSpacingMedium;

    /** Large margin/padding used for the search box. */
    private int mSpacingLarge;

    /**
     * View that the omnibox suggestion list anchors to. This is different from the main search box
     * because the upstream LocationBarLayout code expects a full white box instead of a floating
     * box with shadows.
     */
    private View mAnchorView;

    /** See {@link BoxAnimatorScrim}. */
    private BoxAnimatorScrim mScrimView;

    /** Main content view. */
    private ViewGroup mContentView;

    /** Whether or not the library has begun loading. */
    private boolean mIsNativeLoading;

    /** Whether the native library has been loaded. */
    private boolean mIsNativeReady;

    /** Input submitted before before the native library was loaded. */
    private String mQueuedUrl;

    /** Animates the SearchActivity's controls moving into place. */
    private ValueAnimator mAnimator;

    /** The View that represents the search box. */
    private SearchLocationBarLayout mSearchBox;

    private UrlBar mUrlBar;

    private SearchBoxDataProvider mSearchBoxDataProvider;

    private SnackbarManager mSnackbarManager;
    private ActivityWindowAndroid mWindowAndroid;
    private Tab mTab;

    @Override
    public void backKeyPressed() {
        finish();
    }

    @Override
    public void onStop() {
        finish();
        super.onStop();
    }

    @Override
    public void finish() {
        if (SearchWidgetProvider.ANIMATE_TRANSITION) {
            // Update the search widgets so that they all show up as opaque again.
            SearchWidgetProvider.setLaunchingWidgetId(SearchWidgetProvider.INVALID_WIDGET_ID);
            SearchWidgetProvider.updateAllWidgets();
        }

        super.finish();
        overridePendingTransition(0, android.R.anim.fade_out);
    }

    @Override
    protected boolean shouldDelayBrowserStartup() {
        return true;
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (super.onActivityResultWithNative(requestCode, resultCode, intent)) return true;
        boolean result = mWindowAndroid.onActivityResult(requestCode, resultCode, intent);

        // The voice query should have completed.  If the voice recognition isn't confident about
        // what it heard, it puts the query into the omnibox.  We need to focus it at that point.
        if (mUrlBar != null) focusTextBox(false);

        return result;
    }

    @Override
    protected void setContentView() {
        initializeDimensions();

        mWindowAndroid = new ActivityWindowAndroid(this);
        mSnackbarManager = new SnackbarManager(this);

        // Build the Views that {@link SearchLocationBarLayout} expects to exist.
        ViewStub resultsStub = new ViewStub(this);
        resultsStub.setId(R.id.omnibox_results_container_stub);
        resultsStub.setLayoutResource(R.layout.omnibox_results_container);

        // The FadingBackgroundView isn't used here, but is still animated by the LocationBarLayout.
        // This interferes with the animation we need to show the widget moving to the right place.
        FadingBackgroundView fadingView = new FadingBackgroundView(this, null) {
            @Override
            public void showFadingOverlay() {}
            @Override
            public void hideFadingOverlay(boolean fadeOut) {}
        };

        fadingView.setId(R.id.fading_focus_target);
        FrameLayout.LayoutParams fadingBackgroundLayoutParams = new FrameLayout.LayoutParams(0, 0);

        // Add an empty view to prevent crashing.
        ViewGroup bottomContainer = new FrameLayout(this);
        bottomContainer.setId(R.id.bottom_container);

        // Build the search box, set it to invisible until the animation is done.
        mSearchBox = new SearchLocationBarLayout(this, null);
        mSearchBox.setDelegate(this);
        mSearchBox.setVisibility(mSearchBoxWidgetBounds == null ? View.VISIBLE : View.INVISIBLE);
        mSearchBox.setBackgroundResource(R.drawable.card_single);
        mSearchBox.setPadding(mSpacingLarge, mSpacingMedium, mSpacingLarge, mSpacingMedium);
        mSearchBox.initializeControls(new WindowDelegate(getWindow()), mWindowAndroid);
        mSearchBox.setUrlBarFocusable(true);
        mSearchBoxDataProvider = new SearchBoxDataProvider();
        mSearchBox.setToolbarDataProvider(mSearchBoxDataProvider);
        FrameLayout.LayoutParams searchParams =
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        ApiCompatibilityUtils.setMarginStart(searchParams, mSpacingSmall);
        ApiCompatibilityUtils.setMarginEnd(searchParams, mSpacingSmall);
        searchParams.topMargin = mSpacingSmall;

        // The scrim animates the search box moving into place.
        mScrimView = new BoxAnimatorScrim(this, null);
        mScrimView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Don't allow clicking on the scrim until the animation completes.
                if (mScrimView.getInterpolatedValue() < 1.0f) return;

                // Finish the Activity if the user clicks on the scrim.
                finish();
            }
        });
        if (!SearchWidgetProvider.ANIMATE_TRANSITION) mScrimView.setInterpolatedValue(1.0f);

        // The anchor view puts the omnibox suggestions in the correct place, visually.
        mAnchorView = new View(this);
        mAnchorView.setId(R.id.toolbar);
        mAnchorView.setClickable(true);
        FrameLayout.LayoutParams anchorParams =
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        anchorParams.gravity = Gravity.CENTER_HORIZONTAL;

        // Initialize and build the View hierarchy.
        mContentView = createContentView(mSearchBox);
        mContentView.addView(fadingView, fadingBackgroundLayoutParams);
        mContentView.addView(mScrimView);
        mContentView.addView(resultsStub);
        mContentView.addView(mAnchorView, anchorParams);
        mContentView.addView(mSearchBox, searchParams);
        mContentView.addView(bottomContainer);
        setContentView(mContentView);
        mUrlBar = (UrlBar) mSearchBox.findViewById(R.id.url_bar);

        mSearchBox.setShowCachedZeroSuggestResults(true);
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mIsNativeReady = true;

        mTab = new Tab(TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID),
                Tab.INVALID_TAB_ID, false, this, mWindowAndroid, TabLaunchType.FROM_EXTERNAL_APP,
                null, null);
        mTab.initialize(WebContentsFactory.createWebContents(false, false), null,
                new TabDelegateFactory(), false, false);
        mTab.loadUrl(new LoadUrlParams("about:blank"));
        mSearchBoxDataProvider.onNativeLibraryReady(mTab);
        mSearchBox.onNativeLibraryReady();
        mSearchBox.setAutocompleteProfile(Profile.getLastUsedProfile().getOriginalProfile());

        if (mQueuedUrl != null) loadUrl(mQueuedUrl);

        new Handler().post(new Runnable() {
            @Override
            public void run() {
                onDeferredStartup();
            }
        });
    }

    @Override
    public void onDeferredStartup() {
        super.onDeferredStartup();

        AutocompleteController.nativePrefetchZeroSuggestResults();
        CustomTabsConnection.getInstance(getApplication()).warmup(0);

        if (!isVoiceSearchIntent() && mUrlBar.isFocused()) mSearchBox.onUrlFocusChange(true);
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        return mSearchBox;
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        beginQuery();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    private boolean isVoiceSearchIntent() {
        return IntentUtils.safeGetBooleanExtra(
                getIntent(), SearchWidgetProvider.EXTRA_START_VOICE_SEARCH, false);
    }

    private void beginQuery() {
        if (isVoiceSearchIntent()) {
            mSearchBox.startVoiceRecognition();
        } else {
            focusTextBox(true);
        }
    }

    @Override
    protected void onDestroy() {
        if (mTab != null && mTab.isInitialized()) mTab.destroy();
        super.onDestroy();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    private void focusTextBox(boolean clearQuery) {
        if (mIsNativeReady) mSearchBox.onUrlFocusChange(true);
        mUrlBar.setCursorVisible(true);
        mUrlBar.setIgnoreTextChangesForAutocomplete(true);
        if (clearQuery) mUrlBar.setUrl("", null);
        mUrlBar.setIgnoreTextChangesForAutocomplete(false);
        mUrlBar.setSelection(0, mUrlBar.getText().length());
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                UiUtils.showKeyboard(mUrlBar);
            }
        });
    }

    @Override
    public void loadUrl(String url) {
        // Wait until native has loaded.
        if (!mIsNativeReady) {
            mQueuedUrl = url;
            return;
        }

        // Don't do anything if the input was empty. This is done after the native check to prevent
        // resending a queued query after the user deleted it.
        if (TextUtils.isEmpty(url)) return;

        // Fix up the URL and send it to a tab.
        String fixedUrl = UrlFormatter.fixupUrl(url);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(fixedUrl));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        boolean useHerbTab = ContextUtils.getAppSharedPreferences().getBoolean(
                SearchWidgetProvider.PREF_USE_HERB_TAB, false);
        if (useHerbTab) {
            intent = ChromeLauncherActivity.createCustomTabActivityIntent(this, intent, true);
            intent.putExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE,
                    CustomTabsIntent.SHOW_PAGE_TITLE);
        } else {
            intent.setPackage(getPackageName());
            IntentHandler.addTrustedIntentExtras(intent);
        }

        // TODO(dfalcantara): Make IntentUtils take in an ActivityOptions bundle once public.
        startActivity(intent,
                ActivityOptionsCompat
                        .makeCustomAnimation(this, android.R.anim.fade_in, android.R.anim.fade_out)
                        .toBundle());
        finish();
    }

    private ViewGroup createContentView(final View searchBox) {
        assert mContentView == null;

        ViewGroup contentView = new FrameLayout(this) {
            @Override
            public void onMeasure(int widthSpec, int heightSpec) {
                super.onMeasure(widthSpec, heightSpec);
                if (mAnchorView == null) return;

                // Calculate how big the box is without shadow-induced padding.
                FrameLayout.LayoutParams anchorParams =
                        (FrameLayout.LayoutParams) mAnchorView.getLayoutParams();
                int anchorViewWidth = searchBox.getMeasuredWidth() - mSearchBoxPadding.left
                        - mSearchBoxPadding.right;
                int anchorViewHeight =
                        mSpacingSmall + mSearchBox.getMeasuredHeight() - mSearchBoxPadding.bottom;
                if (anchorParams.width == anchorViewWidth
                        && anchorParams.height == anchorViewHeight) {
                    return;
                }

                // Move the anchor view up a little bit as a dirty hack until we can add a
                // dimension. This allows the suggestion list to move up past the rounded corners of
                // the search box.
                anchorParams.topMargin = -(mSpacingSmall / 4);
                anchorParams.width = anchorViewWidth;
                anchorParams.height = anchorViewHeight;

                // Measure the anchor view to match the search box's height without its side or
                // bottom shadow padding. This isn't exactly what we need, but it's hard to get the
                // correct behavior because LocationBarLayout overrides how the suggestions are
                // laid out.
                int anchorWidthSpec =
                        MeasureSpec.makeMeasureSpec(anchorViewWidth, MeasureSpec.EXACTLY);
                int anchorHeightSpec =
                        MeasureSpec.makeMeasureSpec(anchorViewHeight, MeasureSpec.EXACTLY);
                measureChild(mAnchorView, anchorWidthSpec, anchorHeightSpec);
            }
        };

        contentView.addOnLayoutChangeListener(this);
        contentView.setId(R.id.control_container);
        return contentView;
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mSearchBoxWidgetBounds != null) {
            if (mAnimator == null) initializeAnimation();
        } else {
            // If there's no animation, then we can load the library immediately without worrying
            // about jank.
            beginLoadingLibrary();
        }
        mContentView.removeOnLayoutChangeListener(this);
    }

    private void initializeDimensions() {
        mSearchBoxWidgetBounds = getIntent().getSourceBounds();

        // Cache the padding of the Drawable that is used as the background for the search box.
        Drawable searchBackground =
                ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.card_single);
        searchBackground.getPadding(mSearchBoxPadding);

        // TODO(dfalcantara): Add values to the XML files instead of reusing random ones.
        mSpacingSmall = getResources().getDimensionPixelSize(
                R.dimen.tablet_toolbar_start_padding_no_buttons);
        mSpacingMedium =
                getResources().getDimensionPixelSize(R.dimen.location_bar_incognito_badge_padding);
        mSpacingLarge =
                getResources().getDimensionPixelSize(R.dimen.contextual_search_peek_promo_padding);
    }

    private void initializeAnimation() {
        assert SearchWidgetProvider.ANIMATE_TRANSITION;

        // The bounds of the home screen widget are given in window space coordinates, so they need
        // to be converted into conrdinates that are relative from the root View.  The converted
        // bounds are used to animate the box moving from its widget location to the location at the
        // top of the screen.
        int[] rootWindowLocation = new int[2];
        mContentView.getLocationInWindow(rootWindowLocation);

        final Rect sourceRect = new Rect();
        sourceRect.left = mSearchBoxWidgetBounds.left - rootWindowLocation[0];
        sourceRect.right = mSearchBoxWidgetBounds.right - rootWindowLocation[0];
        sourceRect.top = mSearchBoxWidgetBounds.top - rootWindowLocation[1];
        sourceRect.bottom = mSearchBoxWidgetBounds.bottom - rootWindowLocation[1];

        int[] targetBoxLocation = new int[2];
        mSearchBox.getLocationInWindow(targetBoxLocation);
        final Rect targetRect = new Rect();
        targetRect.left = targetBoxLocation[0] - rootWindowLocation[0];
        targetRect.right = targetRect.left + mSearchBox.getMeasuredWidth();
        targetRect.top = targetBoxLocation[1] - rootWindowLocation[1];
        targetRect.bottom = targetRect.top + mSearchBox.getMeasuredHeight();

        mScrimView.setAnimationRects(sourceRect, targetRect);

        mAnimator = ValueAnimator.ofFloat(0, 1);
        mAnimator.setDuration(ANIMATION_DURATION_MS);
        mAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                // Make the widget on the homescreen hide itself.
                SearchWidgetProvider.updateAllWidgets();
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                // Defer loading the library until the box is in the right place to prevent jank.
                beginLoadingLibrary();
            }
        });
        mAnimator.addUpdateListener(mScrimView);

        new Handler().post(new Runnable() {
            @Override
            public void run() {
                mAnimator.start();
            }
        });
    }

    private void beginLoadingLibrary() {
        if (mIsNativeLoading) return;
        mIsNativeLoading = true;

        // Show the real search box and let the user type in it.
        mScrimView.setInterpolatedValue(1.0f);
        mSearchBox.setVisibility(View.VISIBLE);
        beginQuery();
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mSearchBox.showCachedZeroSuggestResultsIfAvailable();
            }
        });
        ChromeBrowserInitializer.getInstance(getApplicationContext())
                .handlePreNativeStartup(SearchActivity.this);
    }
}
