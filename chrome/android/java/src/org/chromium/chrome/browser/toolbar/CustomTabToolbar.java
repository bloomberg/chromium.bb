// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Pair;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.dom_distiller.DomDistillerServiceFactory;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.page_info.PageInfoPopup;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.widget.Toast;

import java.util.List;

/**
 * The Toolbar layout to be used for a custom tab. This is used for both phone and tablet UIs.
 */
public class CustomTabToolbar extends ToolbarLayout implements LocationBar,
        View.OnLongClickListener {

    /**
     * A simple {@link FrameLayout} that prevents its children from getting touch events. This is
     * especially useful to prevent {@link UrlBar} from running custom touch logic since it is
     * read-only in custom tabs.
     */
    public static class InterceptTouchLayout extends FrameLayout {
        private GestureDetector mGestureDetector;

        public InterceptTouchLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
            mGestureDetector = new GestureDetector(getContext(),
                    new GestureDetector.SimpleOnGestureListener() {
                        @Override
                        public boolean onSingleTapConfirmed(MotionEvent e) {
                            if (LibraryLoader.isInitialized()) {
                                RecordUserAction.record("CustomTabs.TapUrlBar");
                            }
                            return super.onSingleTapConfirmed(e);
                        }
                    });
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            return true;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            mGestureDetector.onTouchEvent(event);
            return super.onTouchEvent(event);
        }
    }

    private static final int TITLE_ANIM_DELAY_MS = 800;
    private static final int STATE_DOMAIN_ONLY = 0;
    private static final int STATE_TITLE_ONLY = 1;
    private static final int STATE_DOMAIN_AND_TITLE = 2;

    private View mLocationBarFrameLayout;
    private View mTitleUrlContainer;
    private UrlBar mUrlBar;
    private TextView mTitleBar;
    private TintedImageButton mSecurityButton;
    private LinearLayout mCustomActionButtons;
    private ImageButton mCloseButton;

    // Whether dark tint should be applied to icons and text.
    private boolean mUseDarkColors = true;

    private ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private CustomTabToolbarAnimationDelegate mAnimDelegate;
    private int mState = STATE_DOMAIN_ONLY;
    private String mFirstUrl;

    protected ToolbarDataProvider mToolbarDataProvider;

    private Runnable mTitleAnimationStarter = new Runnable() {
        @Override
        public void run() {
            mAnimDelegate.startTitleAnimation(getContext());
        }
    };

    /**
     * Constructor for getting this class inflated from an xml layout file.
     */
    public CustomTabToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setBackground(new ColorDrawable(
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_primary_color)));
        mUrlBar = (UrlBar) findViewById(R.id.url_bar);
        mUrlBar.setHint("");
        mUrlBar.setDelegate(this);
        mUrlBar.setEnabled(false);
        mUrlBar.setAllowFocus(false);
        mTitleBar = (TextView) findViewById(R.id.title_bar);
        mLocationBarFrameLayout = findViewById(R.id.location_bar_frame_layout);
        mTitleUrlContainer = findViewById(R.id.title_url_container);
        mTitleUrlContainer.setOnLongClickListener(this);
        mSecurityButton = (TintedImageButton) findViewById(R.id.security_button);
        mCustomActionButtons = findViewById(R.id.action_buttons);
        mCloseButton = (ImageButton) findViewById(R.id.close_button);
        mCloseButton.setOnLongClickListener(this);
        mAnimDelegate = new CustomTabToolbarAnimationDelegate(mSecurityButton, mTitleUrlContainer);
    }

    @Override
    public void initialize(ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController, AppMenuButtonHelper appMenuButtonHelper) {
        super.initialize(toolbarDataProvider, tabController, appMenuButtonHelper);
        updateVisualsForState();
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        mSecurityButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                Tab currentTab = getToolbarDataProvider().getTab();
                if (currentTab == null || currentTab.getWebContents() == null) return;
                Activity activity = currentTab.getWindowAndroid().getActivity().get();
                if (activity == null) return;
                String publisherName = mState == STATE_TITLE_ONLY
                        ? parsePublisherNameFromUrl(currentTab.getUrl()) : null;
                PageInfoPopup.show(
                        activity, currentTab, publisherName, PageInfoPopup.OPENED_FROM_TOOLBAR);
            }
        });
    }

    @Override
    public void setCloseButtonImageResource(Drawable drawable) {
        mCloseButton.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
        mCloseButton.setImageDrawable(drawable);
    }

    @Override
    public void setCustomTabCloseClickHandler(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    @Override
    public void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        ImageButton button = (ImageButton) LayoutInflater.from(getContext())
                                     .inflate(R.layout.custom_tabs_toolbar_button, null);
        button.setOnLongClickListener(this);
        button.setOnClickListener(listener);
        button.setVisibility(VISIBLE);

        updateCustomActionButtonVisuals(button, drawable, description);

        // Add the view at the beginning of the child list.
        mCustomActionButtons.addView(button, 0);
    }

    @Override
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        ImageButton button = (ImageButton) mCustomActionButtons.getChildAt(
                mCustomActionButtons.getChildCount() - 1 - index);
        assert button != null;
        updateCustomActionButtonVisuals(button, drawable, description);
    }

    private void updateCustomActionButtonVisuals(
            ImageButton button, Drawable drawable, String description) {
        Resources resources = getResources();

        // The height will be scaled to match spec while keeping the aspect ratio, so get the scaled
        // width through that.
        int sourceHeight = drawable.getIntrinsicHeight();
        int sourceScaledHeight = resources.getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int sourceWidth = drawable.getIntrinsicWidth();
        int sourceScaledWidth = sourceWidth * sourceScaledHeight / sourceHeight;
        int minPadding = resources.getDimensionPixelSize(R.dimen.min_toolbar_icon_side_padding);

        int sidePadding = Math.max((2 * sourceScaledHeight - sourceScaledWidth) / 2, minPadding);
        int topPadding = button.getPaddingTop();
        int bottomPadding = button.getPaddingBottom();
        button.setPadding(sidePadding, topPadding, sidePadding, bottomPadding);
        button.setImageDrawable(drawable);
        updateButtonTint(button);

        button.setContentDescription(description);
    }

    /**
     * @return The custom action button with the given {@code index}. For test purpose only.
     * @param index The index of the custom action button to return.
     */
    @VisibleForTesting
    public ImageButton getCustomActionButtonForTest(int index) {
        return (ImageButton) mCustomActionButtons.getChildAt(index);
    }

    @Override
    public int getTabStripHeight() {
        return 0;
    }

    @Override
    public Tab getCurrentTab() {
        return getToolbarDataProvider().getTab();
    }

    @Override
    public boolean allowKeyboardLearning() {
        return !super.isIncognito();
    }

    @Override
    public boolean shouldEmphasizeHttpsScheme() {
        return !mToolbarDataProvider.isUsingBrandColor();
    }

    @Override
    public void setShowTitle(boolean showTitle) {
        if (showTitle) {
            mState = STATE_DOMAIN_AND_TITLE;
            mAnimDelegate.prepareTitleAnim(mUrlBar, mTitleBar);
        } else {
            mState = STATE_DOMAIN_ONLY;
        }
    }

    @Override
    public void setUrlBarHidden(boolean hideUrlBar) {
        // Urlbar visibility cannot be toggled if it is the only visible element.
        if (mState == STATE_DOMAIN_ONLY) return;

        if (hideUrlBar && mState == STATE_DOMAIN_AND_TITLE) {
            mState = STATE_TITLE_ONLY;
            mAnimDelegate.setTitleAnimationEnabled(false);
            mUrlBar.setVisibility(View.GONE);
            mTitleBar.setVisibility(View.VISIBLE);
            LayoutParams lp = (LayoutParams) mTitleBar.getLayoutParams();
            lp.bottomMargin = 0;
            mTitleBar.setLayoutParams(lp);
            mTitleBar.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    getResources().getDimension(R.dimen.location_bar_url_text_size));
        } else if (!hideUrlBar && mState == STATE_TITLE_ONLY) {
            mState = STATE_DOMAIN_AND_TITLE;
            mTitleBar.setVisibility(View.VISIBLE);
            mUrlBar.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    getResources().getDimension(R.dimen.custom_tabs_url_text_size));
            mUrlBar.setVisibility(View.VISIBLE);
            LayoutParams lp = (LayoutParams) mTitleBar.getLayoutParams();
            lp.bottomMargin = getResources()
                    .getDimensionPixelSize(R.dimen.custom_tabs_toolbar_vertical_padding);
            mTitleBar.setLayoutParams(lp);
            mTitleBar.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    getResources().getDimension(R.dimen.custom_tabs_title_text_size));
            updateSecurityIcon();
        } else {
            assert false : "Unreached state";
        }
    }

    @Override
    public String getContentPublisher() {
        if (mState == STATE_TITLE_ONLY) {
            if (getToolbarDataProvider().getTab() == null) return null;
            return parsePublisherNameFromUrl(getToolbarDataProvider().getTab().getUrl());
        }
        return null;
    }

    @Override
    public void setTitleToPageTitle() {
        String title = getToolbarDataProvider().getTitle();
        if (!getToolbarDataProvider().hasTab() || TextUtils.isEmpty(title)) {
            mTitleBar.setText("");
            return;
        }

        // It takes some time to parse the title of the webcontent, and before that
        // ToolbarDataProvider#getTitle always returns the url. We postpone the title animation
        // until the title is authentic.
        if ((mState == STATE_DOMAIN_AND_TITLE || mState == STATE_TITLE_ONLY)
                && !title.equals(getToolbarDataProvider().getCurrentUrl())
                && !title.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)) {
            // Delay the title animation until security icon animation finishes.
            ThreadUtils.postOnUiThreadDelayed(mTitleAnimationStarter, TITLE_ANIM_DELAY_MS);
        }

        mTitleBar.setText(title);
    }

    @Override
    protected void onNavigatedToDifferentPage() {
        super.onNavigatedToDifferentPage();
        setTitleToPageTitle();
        if (mState == STATE_TITLE_ONLY) {
            if (TextUtils.isEmpty(mFirstUrl)) {
                mFirstUrl = getToolbarDataProvider().getTab().getUrl();
            } else {
                if (mFirstUrl.equals(getToolbarDataProvider().getTab().getUrl())) return;
                setUrlBarHidden(false);
            }
        }
        updateSecurityIcon();
    }

    @Override
    public void setUrlToPageUrl() {
        if (getCurrentTab() == null) {
            mUrlBar.setUrl("", null);
            return;
        }

        String url = getCurrentTab().getUrl().trim();
        if (mState == STATE_TITLE_ONLY) {
            if (!TextUtils.isEmpty(getToolbarDataProvider().getTitle())) setTitleToPageTitle();
        }

        // Don't show anything for Chrome URLs and "about:blank".
        // If we have taken a pre-initialized WebContents, then the starting URL
        // is "about:blank". We should not display it.
        if (NativePageFactory.isNativePageUrl(url, getCurrentTab().isIncognito())
                || ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(url)) {
            mUrlBar.setUrl("", null);
            return;
        }
        String displayText = getToolbarDataProvider().getText();
        Pair<String, String> urlText = LocationBarLayout.splitPathFromUrlDisplayText(displayText);
        displayText = urlText.first;

        if (DomDistillerUrlUtils.isDistilledPage(url)) {
            if (isStoredArticle(url)) {
                Profile profile = getCurrentTab().getProfile();
                DomDistillerService domDistillerService =
                        DomDistillerServiceFactory.getForProfile(profile);
                String originalUrl = domDistillerService.getUrlForEntry(
                        DomDistillerUrlUtils.getValueForKeyInUrl(url, "entry_id"));
                displayText =
                        DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl);
            } else if (DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url) != null) {
                String originalUrl = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
                displayText =
                        DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl);
            }
        }

        if (mUrlBar.setUrl(url, displayText)) {
            mUrlBar.emphasizeUrl();
        }
    }

    private boolean isStoredArticle(String url) {
        DomDistillerService domDistillerService =
                DomDistillerServiceFactory.getForProfile(Profile.getLastUsedProfile());
        String entryIdFromUrl = DomDistillerUrlUtils.getValueForKeyInUrl(url, "entry_id");
        if (TextUtils.isEmpty(entryIdFromUrl)) return false;
        return domDistillerService.hasEntry(entryIdFromUrl);
    }

    @Override
    public void updateLoadingState(boolean updateUrl) {
        if (updateUrl) setUrlToPageUrl();
        updateSecurityIcon();
    }

    @Override
    public void setToolbarDataProvider(ToolbarDataProvider model) {
        mToolbarDataProvider = model;
    }

    @Override
    public ToolbarDataProvider getToolbarDataProvider() {
        return mToolbarDataProvider;
    }

    @Override
    public void updateVisualsForState() {
        Resources resources = getResources();
        updateSecurityIcon();
        updateButtonsTint();
        mUrlBar.setUseDarkTextColors(mUseDarkColors);

        int titleTextColor = mUseDarkColors
                ? ApiCompatibilityUtils.getColor(resources, R.color.url_emphasis_default_text)
                : ApiCompatibilityUtils.getColor(resources,
                        R.color.url_emphasis_light_default_text);
        mTitleBar.setTextColor(titleTextColor);

        if (getProgressBar() != null) {
            if (!ColorUtils.isUsingDefaultToolbarColor(
                        getResources(), false, false, getBackground().getColor())) {
                getProgressBar().setThemeColor(getBackground().getColor(), false);
            } else {
                getProgressBar().setBackgroundColor(ApiCompatibilityUtils.getColor(resources,
                        R.color.progress_bar_background));
                getProgressBar().setForegroundColor(ApiCompatibilityUtils.getColor(resources,
                        R.color.progress_bar_foreground));
            }
        }
    }

    private void updateButtonsTint() {
        mMenuButton.setTint(mUseDarkColors ? mDarkModeTint : mLightModeTint);
        updateButtonTint(mCloseButton);
        int numCustomActionButtons = mCustomActionButtons.getChildCount();
        for (int i = 0; i < numCustomActionButtons; i++) {
            updateButtonTint((ImageButton) mCustomActionButtons.getChildAt(i));
        }
        updateButtonTint(mSecurityButton);
    }

    private void updateButtonTint(ImageButton button) {
        Drawable drawable = button.getDrawable();
        if (drawable instanceof TintedDrawable) {
            ((TintedDrawable) drawable).setTint(mUseDarkColors ? mDarkModeTint : mLightModeTint);
        }
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        setTitleToPageTitle();
        setUrlToPageUrl();
    }

    @Override
    public ColorDrawable getBackground() {
        return (ColorDrawable) super.getBackground();
    }

    @Override
    public void initializeControls(WindowDelegate windowDelegate, WindowAndroid windowAndroid) {
    }

    @Override
    public void updateSecurityIcon() {
        if (mState == STATE_TITLE_ONLY) return;

        boolean showSecurityButton = true;
        if (!getToolbarDataProvider().shouldShowSecurityIcon()) {
            // Hide the button if we don't have an actual icon to display.
            showSecurityButton = false;
            mSecurityButton.setImageDrawable(null);
        } else {
            // ImageView#setImageResource is no-op if given resource is the current one.
            mSecurityButton.setImageResource(getToolbarDataProvider().getSecurityIconResource());
            mSecurityButton.setTint(
                    LocationBarLayout.getColorStateList(getToolbarDataProvider(), getResources()));
        }

        if (showSecurityButton) {
            mAnimDelegate.showSecurityButton();
        } else {
            mAnimDelegate.hideSecurityButton();
        }

        mUrlBar.emphasizeUrl();
        mUrlBar.invalidate();
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    @Override
    protected void onPrimaryColorChanged(boolean shouldAnimate) {
        if (mBrandColorTransitionActive) mBrandColorTransitionAnimation.cancel();

        final ColorDrawable background = getBackground();
        final int initialColor = background.getColor();
        final int finalColor = getToolbarDataProvider().getPrimaryColor();

        if (background.getColor() == finalColor) return;

        mBrandColorTransitionAnimation = ValueAnimator.ofFloat(0, 1)
                .setDuration(ToolbarPhone.THEME_COLOR_TRANSITION_DURATION);
        mBrandColorTransitionAnimation.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        mBrandColorTransitionAnimation.addUpdateListener(new AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float fraction = animation.getAnimatedFraction();
                int red = (int) (Color.red(initialColor)
                        + fraction * (Color.red(finalColor) - Color.red(initialColor)));
                int green = (int) (Color.green(initialColor)
                        + fraction * (Color.green(finalColor) - Color.green(initialColor)));
                int blue = (int) (Color.blue(initialColor)
                        + fraction * (Color.blue(finalColor) - Color.blue(initialColor)));
                background.setColor(Color.rgb(red, green, blue));
            }
        });
        mBrandColorTransitionAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mBrandColorTransitionActive = false;

                // Using the current background color instead of the final color in case this
                // animation was cancelled.  This ensures the assets are updated to the visible
                // color.
                mUseDarkColors =
                        !ColorUtils.shouldUseLightForegroundOnBackground(background.getColor());
                updateVisualsForState();
            }
        });
        mBrandColorTransitionAnimation.start();
        mBrandColorTransitionActive = true;
        if (!shouldAnimate) mBrandColorTransitionAnimation.end();
    }

    @Override
    public View getContainerView() {
        return this;
    }

    @Override
    public void setDefaultTextEditActionModeCallback(ToolbarActionModeCallback callback) {
        mUrlBar.setCustomSelectionActionModeCallback(callback);
    }

    private void updateLayoutParams() {
        int startMargin = 0;
        int locationBarLayoutChildIndex = -1;
        for (int i = 0; i < getChildCount(); i++) {
            View childView = getChildAt(i);
            if (childView == mCloseButton && childView.getVisibility() == GONE) {
                startMargin += getResources().getDimensionPixelSize(
                        R.dimen.custom_tabs_toolbar_horizontal_margin_no_close);
            } else if (childView.getVisibility() != GONE) {
                LayoutParams childLayoutParams = (LayoutParams) childView.getLayoutParams();
                if (ApiCompatibilityUtils.getMarginStart(childLayoutParams) != startMargin) {
                    ApiCompatibilityUtils.setMarginStart(childLayoutParams, startMargin);
                    childView.setLayoutParams(childLayoutParams);
                }
                if (childView == mLocationBarFrameLayout) {
                    locationBarLayoutChildIndex = i;
                    break;
                }
                int widthMeasureSpec;
                int heightMeasureSpec;
                if (childLayoutParams.width == LayoutParams.WRAP_CONTENT) {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredWidth(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.width == LayoutParams.MATCH_PARENT) {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredWidth(), MeasureSpec.EXACTLY);
                } else {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.width, MeasureSpec.EXACTLY);
                }
                if (childLayoutParams.height == LayoutParams.WRAP_CONTENT) {
                    heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredHeight(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.height == LayoutParams.MATCH_PARENT) {
                    heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                            getMeasuredHeight(), MeasureSpec.EXACTLY);
                } else {
                    heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.height, MeasureSpec.EXACTLY);
                }
                childView.measure(widthMeasureSpec, heightMeasureSpec);
                startMargin += childView.getMeasuredWidth();
            }
        }

        assert locationBarLayoutChildIndex != -1;
        int locationBarLayoutEndMargin = 0;
        for (int i = locationBarLayoutChildIndex + 1; i < getChildCount(); i++) {
            View childView = getChildAt(i);
            if (childView.getVisibility() != GONE) {
                locationBarLayoutEndMargin += childView.getMeasuredWidth();
            }
        }
        LayoutParams urlLayoutParams = (LayoutParams) mLocationBarFrameLayout.getLayoutParams();

        if (ApiCompatibilityUtils.getMarginEnd(urlLayoutParams) != locationBarLayoutEndMargin) {
            ApiCompatibilityUtils.setMarginEnd(urlLayoutParams, locationBarLayoutEndMargin);
            mLocationBarFrameLayout.setLayoutParams(urlLayoutParams);
        }

        // Update left margin of mTitleUrlContainer here to make sure the security icon is always
        // placed left of the urlbar.
        LayoutParams lp = (LayoutParams) mTitleUrlContainer.getLayoutParams();
        if (mSecurityButton.getVisibility() == View.GONE) {
            lp.leftMargin = 0;
        } else {
            lp.leftMargin = mSecurityButton.getMeasuredWidth();
        }
        mTitleUrlContainer.setLayoutParams(lp);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        updateLayoutParams();
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public LocationBar getLocationBar() {
        return this;
    }

    @Override
    public boolean useLightDrawables() {
        return !mUseDarkColors;
    }

    @Override
    public boolean onLongClick(View v) {
        if (v == mCloseButton || v.getParent() == mCustomActionButtons) {
            return AccessibilityUtil.showAccessibilityToast(
                    getContext(), v, v.getContentDescription());
        }
        if (v == mTitleUrlContainer) {
            ClipboardManager clipboard = (ClipboardManager) getContext()
                    .getSystemService(Context.CLIPBOARD_SERVICE);
            Tab tab = getCurrentTab();
            if (tab == null) return false;
            String url = tab.getOriginalUrl();
            ClipData clip = ClipData.newPlainText("url", url);
            clipboard.setPrimaryClip(clip);
            Toast.makeText(getContext(), R.string.url_copied, Toast.LENGTH_SHORT).show();
            return true;
        }
        return false;
    }

    private static String parsePublisherNameFromUrl(String url) {
        // TODO(ianwen): Make it generic to parse url from URI path. http://crbug.com/599298
        // The url should look like: https://www.google.com/amp/s/www.nyt.com/ampthml/blogs.html
        // or https://www.google.com/amp/www.nyt.com/ampthml/blogs.html.
        Uri uri = Uri.parse(url);
        List<String> segments = uri.getPathSegments();
        if (segments.size() >= 3) {
            if (segments.get(1).length() > 1) return segments.get(1);
            return segments.get(2);
        }
        return url;
    }

    // Toolbar and LocationBar calls that are not relevant here.

    @Override
    public void onTextChangedForAutocomplete() {}

    @Override
    public void backKeyPressed() {
        assert false : "The URL bar should never take focus in CCTs.";
    }

    @Override
    public boolean shouldForceLTR() {
        return true;
    }

    @Override
    @UrlBar.ScrollType
    public int getScrollType() {
        return UrlBar.SCROLL_TO_TLD;
    }

    @Override
    public void setUrlBarFocus(boolean shouldBeFocused) {}

    @Override
    public void showUrlBarCursorWithoutFocusAnimations() {}

    @Override
    public boolean isUrlBarFocused() {
        return false;
    }

    @Override
    public void selectAll() {}

    @Override
    public void revertChanges() {}

    @Override
    public long getFirstUrlBarFocusTime() {
        return 0;
    }

    @Override
    public void hideSuggestions() {}

    @Override
    public void updateMicButtonState() {}

    @Override
    public void onTabLoadingNTP(NewTabPage ntp) {}

    @Override
    public void setAutocompleteProfile(Profile profile) {}

    @Override
    public void showAppMenuUpdateBadge() {}

    @Override
    public boolean isShowingAppMenuUpdateBadge() {
        return false;
    }

    @Override
    public void removeAppMenuUpdateBadge(boolean animate) {}

    @Override
    protected void setAppMenuUpdateBadgeToVisible(boolean animate) {}

    @Override
    public View getMenuButtonWrapper() {
        // This class has no menu button wrapper, so return the menu button instead.
        return mMenuButton;
    }

    @Override
    public boolean mustQueryUrlBarLocationForSuggestions() {
        return false;
    }

    // Temporary fix to override ToolbarLayout's highlight-related methods
    @Override
    public void setMenuButtonHighlight(boolean highlight) {}

    @Override
    protected void setMenuButtonHighlightDrawable(boolean highlighting) {}

    @Override
    public boolean isSuggestionsListShown() {
        // Custom tabs do not support suggestions.
        return false;
    }

    @Override
    public boolean useModernDesign() {
        return false;
    }
}
