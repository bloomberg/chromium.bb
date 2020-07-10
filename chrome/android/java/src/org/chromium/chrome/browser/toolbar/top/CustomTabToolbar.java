// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.support.v4.text.BidiFormatter;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.support.v7.content.res.AppCompatResources;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.GURLUtils;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.Toast;

import java.util.List;
import java.util.regex.Pattern;

/**
 * The Toolbar layout to be used for a custom tab. This is used for both phone and tablet UIs.
 */
public class CustomTabToolbar extends ToolbarLayout implements View.OnLongClickListener {
    private static final Object ORIGIN_SPAN = new Object();

    /**
     * A simple {@link FrameLayout} that prevents its children from getting touch events. This is
     * especially useful to prevent {@link UrlBar} from running custom touch logic since it is
     * read-only in custom tabs.
     */
    public static class InterceptTouchLayout extends FrameLayout {
        private GestureDetector mGestureDetector;

        public InterceptTouchLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
            mGestureDetector = new GestureDetector(
                    getContext(), new GestureDetector.SimpleOnGestureListener() {
                        @Override
                        public boolean onSingleTapConfirmed(MotionEvent e) {
                            if (LibraryLoader.getInstance().isInitialized()) {
                                RecordUserAction.record("CustomTabs.TapUrlBar");
                            }
                            return super.onSingleTapConfirmed(e);
                        }
                    }, ThreadUtils.getUiThreadHandler());
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

    /** Regular expression for prefixes to strip from publisher hostnames. */
    private static final Pattern HOSTNAME_PREFIX_PATTERN =
            Pattern.compile("^(www[0-9]*|web|ftp|wap|home|mobile|amp)\\.");

    private View mLocationBarFrameLayout;
    private View mTitleUrlContainer;
    private TextView mUrlBar;
    private View mLiteStatusView;
    private View mLiteStatusSeparatorView;
    private UrlBarCoordinator mUrlCoordinator;
    private TextView mTitleBar;
    private ImageButton mSecurityButton;
    private LinearLayout mCustomActionButtons;
    private ImageButton mCloseButton;
    private ImageButton mMenuButton;

    // Whether dark tint should be applied to icons and text.
    private boolean mUseDarkColors;

    private final ColorStateList mDarkModeTint;
    private final ColorStateList mLightModeTint;

    private ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private CustomTabToolbarAnimationDelegate mAnimDelegate;
    private int mState = STATE_DOMAIN_ONLY;
    private String mFirstUrl;

    protected ToolbarDataProvider mToolbarDataProvider;
    private LocationBar mLocationBar;

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

        mDarkModeTint = ToolbarColors.getThemedToolbarIconTint(context, false);
        mLightModeTint = ToolbarColors.getThemedToolbarIconTint(context, true);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        final int backgroundColor = ChromeColors.getDefaultThemeColor(getResources(), false);
        setBackground(new ColorDrawable(backgroundColor));
        mUseDarkColors = !ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);
        mUrlBar = (TextView) findViewById(R.id.url_bar);
        mUrlBar.setHint("");
        mUrlBar.setEnabled(false);
        mLiteStatusView = findViewById(R.id.url_bar_lite_status);
        mLiteStatusSeparatorView = findViewById(R.id.url_bar_lite_status_separator);
        mUrlCoordinator = new UrlBarCoordinator((UrlBar) mUrlBar);
        mLocationBar = new LocationBarImpl();
        mUrlCoordinator.setDelegate(mLocationBar);
        mUrlCoordinator.setAllowFocus(false);
        mTitleBar = findViewById(R.id.title_bar);
        mLocationBarFrameLayout = findViewById(R.id.location_bar_frame_layout);
        mTitleUrlContainer = findViewById(R.id.title_url_container);
        mTitleUrlContainer.setOnLongClickListener(this);
        mSecurityButton = findViewById(R.id.security_button);
        mCustomActionButtons = findViewById(R.id.action_buttons);
        mCloseButton = findViewById(R.id.close_button);
        mCloseButton.setOnLongClickListener(this);
        mMenuButton = findViewById(R.id.menu_button);
        mAnimDelegate = new CustomTabToolbarAnimationDelegate(mSecurityButton, mTitleUrlContainer);
    }

    @Override
    void initialize(ToolbarDataProvider toolbarDataProvider, ToolbarTabController tabController) {
        super.initialize(toolbarDataProvider, tabController);
        mLocationBar.updateVisualsForState();
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        mLocationBar.onNativeLibraryReady();
    }

    @Override
    void setCloseButtonImageResource(Drawable drawable) {
        mCloseButton.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
        mCloseButton.setImageDrawable(drawable);
        if (drawable != null) {
            updateButtonTint(mCloseButton);
        }
    }

    @Override
    void setCustomTabCloseClickHandler(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    @Override
    void addCustomActionButton(Drawable drawable, String description, OnClickListener listener) {
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
    void updateCustomActionButton(int index, Drawable drawable, String description) {
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
    int getTabStripHeight() {
        return 0;
    }

    /** @return The current active {@link Tab}. */
    @Nullable
    private Tab getCurrentTab() {
        return getToolbarDataProvider().getTab();
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
            lp.bottomMargin = getResources().getDimensionPixelSize(
                    R.dimen.custom_tabs_toolbar_vertical_padding);
            mTitleBar.setLayoutParams(lp);
            mTitleBar.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    getResources().getDimension(R.dimen.custom_tabs_title_text_size));
            mLocationBar.updateStatusIcon();
        } else {
            assert false : "Unreached state";
        }
    }

    @Override
    String getContentPublisher() {
        Tab tab = getToolbarDataProvider().getTab();
        if (tab == null) return null;

        String publisherUrl = TrustedCdn.getPublisherUrl(tab);
        if (publisherUrl != null) return extractPublisherFromPublisherUrl(publisherUrl);

        // TODO(bauerb): Remove this once trusted CDN publisher URLs have rolled out completely.
        if (mState == STATE_TITLE_ONLY) return parsePublisherNameFromUrl(tab.getUrl());

        return null;
    }

    @Override
    void onNavigatedToDifferentPage() {
        super.onNavigatedToDifferentPage();
        mLocationBar.setTitleToPageTitle();
        if (mState == STATE_TITLE_ONLY) {
            if (TextUtils.isEmpty(mFirstUrl)) {
                mFirstUrl = getToolbarDataProvider().getTab().getUrl();
            } else {
                if (mFirstUrl.equals(getToolbarDataProvider().getTab().getUrl())) return;
                setUrlBarHidden(false);
            }
        }
        mLocationBar.updateStatusIcon();
    }

    @VisibleForTesting
    public static String extractPublisherFromPublisherUrl(String publisherUrl) {
        String publisher =
                UrlFormatter.formatUrlForDisplayOmitScheme(GURLUtils.getOrigin(publisherUrl));

        String trimmedPublisher = HOSTNAME_PREFIX_PATTERN.matcher(publisher).replaceFirst("");
        return BidiFormatter.getInstance().unicodeWrap(trimmedPublisher);
    }

    private void updateButtonsTint() {
        if (getMenuButton() != null) {
            ApiCompatibilityUtils.setImageTintList(
                    getMenuButton(), mUseDarkColors ? mDarkModeTint : mLightModeTint);
        }
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
        mLocationBar.setTitleToPageTitle();
        mLocationBar.setUrlToPageUrl();
    }

    @Override
    public ColorDrawable getBackground() {
        return (ColorDrawable) super.getBackground();
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    @Override
    void onPrimaryColorChanged(boolean shouldAnimate) {
        if (mBrandColorTransitionActive) mBrandColorTransitionAnimation.cancel();

        final ColorDrawable background = getBackground();
        final int initialColor = background.getColor();
        final int finalColor = getToolbarDataProvider().getPrimaryColor();

        if (background.getColor() == finalColor) return;

        mBrandColorTransitionAnimation = ValueAnimator.ofFloat(0, 1).setDuration(
                ToolbarPhone.THEME_COLOR_TRANSITION_DURATION);
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
                mLocationBar.updateVisualsForState();
            }
        });
        mBrandColorTransitionAnimation.start();
        mBrandColorTransitionActive = true;
        if (!shouldAnimate) mBrandColorTransitionAnimation.end();
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
                if (MarginLayoutParamsCompat.getMarginStart(childLayoutParams) != startMargin) {
                    MarginLayoutParamsCompat.setMarginStart(childLayoutParams, startMargin);
                    childView.setLayoutParams(childLayoutParams);
                }
                if (childView == mLocationBarFrameLayout) {
                    locationBarLayoutChildIndex = i;
                    break;
                }
                int widthMeasureSpec;
                int heightMeasureSpec;
                if (childLayoutParams.width == LayoutParams.WRAP_CONTENT) {
                    widthMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.width == LayoutParams.MATCH_PARENT) {
                    widthMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY);
                } else {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.width, MeasureSpec.EXACTLY);
                }
                if (childLayoutParams.height == LayoutParams.WRAP_CONTENT) {
                    heightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.height == LayoutParams.MATCH_PARENT) {
                    heightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
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

        if (MarginLayoutParamsCompat.getMarginEnd(urlLayoutParams) != locationBarLayoutEndMargin) {
            MarginLayoutParamsCompat.setMarginEnd(urlLayoutParams, locationBarLayoutEndMargin);
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
        return mLocationBar;
    }

    @Override
    public boolean onLongClick(View v) {
        if (v == mCloseButton || v.getParent() == mCustomActionButtons) {
            return Toast.showAnchoredToast(getContext(), v, v.getContentDescription());
        }
        if (v == mTitleUrlContainer) {
            Tab tab = getCurrentTab();
            if (tab == null) return false;
            Clipboard.getInstance().copyUrlToClipboard(((TabImpl) tab).getOriginalUrl());
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

    @Override
    void showAppMenuUpdateBadge(boolean animate) {}

    @Override
    boolean isShowingAppMenuUpdateBadge() {
        return false;
    }

    @Override
    void removeAppMenuUpdateBadge(boolean animate) {}

    @Override
    View getMenuButtonWrapper() {
        // This class has no menu button wrapper, so return the menu button instead.
        return getMenuButton();
    }

    @Override
    ImageButton getMenuButton() {
        return mMenuButton;
    }

    @Override
    void disableMenuButton() {
        super.disableMenuButton();
        mMenuButton = null;
        // In addition to removing the menu button, we also need to remove the margin on the custom
        // action button.
        ViewGroup.MarginLayoutParams p =
                (ViewGroup.MarginLayoutParams) mCustomActionButtons.getLayoutParams();
        p.setMarginEnd(0);
        mCustomActionButtons.setLayoutParams(p);
    }

    private class LocationBarImpl implements LocationBar {
        @Override
        public void onNativeLibraryReady() {
            mSecurityButton.setOnClickListener(v -> {
                Tab currentTab = getToolbarDataProvider().getTab();
                if (currentTab == null || currentTab.getWebContents() == null) return;
                Activity activity = currentTab.getWindowAndroid().getActivity().get();
                if (activity == null) return;
                PageInfoController.show(activity, currentTab, getContentPublisher(),
                        PageInfoController.OpenedFromSource.TOOLBAR);
            });
        }

        @Override
        public View getViewForUrlBackFocus() {
            Tab tab = getCurrentTab();
            if (tab == null) return null;
            return tab.getView();
        }

        @Override
        public boolean allowKeyboardLearning() {
            return !CustomTabToolbar.this.isIncognito();
        }

        @Override
        public boolean shouldCutCopyVerbatim() {
            return false;
        }

        @Override
        public void gestureDetected(boolean isLongPress) {}

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
                PostTask.postDelayedTask(
                        UiThreadTaskTraits.DEFAULT, mTitleAnimationStarter, TITLE_ANIM_DELAY_MS);
            }

            mTitleBar.setText(title);
        }

        @Override
        public void setUrlToPageUrl() {
            Tab tab = getCurrentTab();
            if (tab == null) {
                mUrlCoordinator.setUrlBarData(
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
                return;
            }

            String publisherUrl = TrustedCdn.getPublisherUrl(tab);
            String url = publisherUrl != null ? publisherUrl : tab.getUrl().trim();
            if (mState == STATE_TITLE_ONLY) {
                if (!TextUtils.isEmpty(getToolbarDataProvider().getTitle())) setTitleToPageTitle();
            }

            // Don't show anything for Chrome URLs and "about:blank".
            // If we have taken a pre-initialized WebContents, then the starting URL
            // is "about:blank". We should not display it.
            if (NativePageFactory.isNativePageUrl(url, getCurrentTab().isIncognito())
                    || ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(url)) {
                mUrlCoordinator.setUrlBarData(
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
                return;
            }
            final CharSequence displayText;
            final int originStart;
            final int originEnd;
            if (publisherUrl != null) {
                // TODO(bauerb): Move this into the ToolbarDataProvider as well?
                String plainDisplayText =
                        getContext().getString(R.string.custom_tab_amp_publisher_url,
                                extractPublisherFromPublisherUrl(publisherUrl));
                ColorStateList tint = mUseDarkColors ? mDarkModeTint : mLightModeTint;
                SpannableString formattedDisplayText = SpanApplier.applySpans(plainDisplayText,
                        new SpanInfo("<pub>", "</pub>", ORIGIN_SPAN),
                        new SpanInfo(
                                "<bg>", "</bg>", new ForegroundColorSpan(tint.getDefaultColor())));
                originStart = formattedDisplayText.getSpanStart(ORIGIN_SPAN);
                originEnd = formattedDisplayText.getSpanEnd(ORIGIN_SPAN);
                formattedDisplayText.removeSpan(ORIGIN_SPAN);
                displayText = formattedDisplayText;
            } else {
                UrlBarData urlBarData = getToolbarDataProvider().getUrlBarData();
                displayText = urlBarData.displayText.subSequence(
                        urlBarData.originStartIndex, urlBarData.originEndIndex);
                originStart = 0;
                originEnd = displayText.length();
            }

            // The Lite Status view visibility should be updated on every new URL and only be
            // displayed along with the URL bar.
            final boolean liteStatusIsVisible =
                    getToolbarDataProvider().isPreview() && mUrlBar.getVisibility() == View.VISIBLE;
            mLiteStatusView.setVisibility(liteStatusIsVisible ? View.VISIBLE : View.GONE);
            mLiteStatusSeparatorView.setVisibility(liteStatusIsVisible ? View.VISIBLE : View.GONE);

            mUrlCoordinator.setUrlBarData(
                    UrlBarData.create(url, displayText, originStart, originEnd, url),
                    UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
        }

        @Override
        public void updateLoadingState(boolean updateUrl) {
            if (updateUrl) setUrlToPageUrl();
            updateStatusIcon();
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
            updateStatusIcon();
            updateButtonsTint();
            if (mUrlCoordinator.setUseDarkTextColors(mUseDarkColors)) {
                setUrlToPageUrl();
            }

            mTitleBar.setTextColor(ApiCompatibilityUtils.getColor(resources,
                    mUseDarkColors ? R.color.default_text_color_dark
                                   : R.color.default_text_color_light));

            if (getProgressBar() != null) {
                if (!ToolbarColors.isUsingDefaultToolbarColor(
                            getResources(), false, getBackground().getColor())) {
                    getProgressBar().setThemeColor(getBackground().getColor(), false);
                } else {
                    getProgressBar().setBackgroundColor(ApiCompatibilityUtils.getColor(
                            resources, R.color.progress_bar_background));
                    getProgressBar().setForegroundColor(ApiCompatibilityUtils.getColor(
                            resources, R.color.progress_bar_foreground));
                }
            }
        }

        @Override
        public void initializeControls(WindowDelegate windowDelegate, WindowAndroid windowAndroid,
                ActivityTabProvider provider) {}

        @Override
        public void updateStatusIcon() {
            if (mState == STATE_TITLE_ONLY) return;

            int securityIconResource = getToolbarDataProvider().getSecurityIconResource(
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()));
            if (securityIconResource == 0) {
                // Hide the button if we don't have an actual icon to display.
                mSecurityButton.setImageDrawable(null);
                mAnimDelegate.hideSecurityButton();
            } else {
                // ImageView#setImageResource is no-op if given resource is the current one.
                mSecurityButton.setImageResource(securityIconResource);
                ColorStateList colorStateList = AppCompatResources.getColorStateList(
                        getContext(), getToolbarDataProvider().getSecurityIconColorStateList());
                ApiCompatibilityUtils.setImageTintList(mSecurityButton, colorStateList);
                mAnimDelegate.showSecurityButton();
            }

            int contentDescriptionId = getToolbarDataProvider().getSecurityIconContentDescription();
            String contentDescription = getContext().getString(contentDescriptionId);
            mSecurityButton.setContentDescription(contentDescription);

            setUrlToPageUrl();
            mUrlBar.invalidate();
        }

        @Override
        public View getContainerView() {
            return CustomTabToolbar.this;
        }

        @Override
        public View getSecurityIconView() {
            return mSecurityButton;
        }

        @Override
        public void setDefaultTextEditActionModeCallback(ToolbarActionModeCallback callback) {
            mUrlCoordinator.setActionModeCallback(callback);
        }

        @Override
        public void backKeyPressed() {
            assert false : "The URL bar should never take focus in CCTs.";
        }

        @Override
        public void destroy() {}

        @Override
        public boolean shouldForceLTR() {
            return true;
        }

        @Override
        public void showUrlBarCursorWithoutFocusAnimations() {}

        @Override
        public void selectAll() {}

        @Override
        public void revertChanges() {}

        @Override
        public void updateMicButtonState() {}

        @Override
        public void onTabLoadingNTP(NewTabPage ntp) {}

        @Override
        public void setAutocompleteProfile(Profile profile) {}

        @Override
        public void setShowIconsWhenUrlFocused(boolean showIcon) {}

        @Override
        public int getUrlContainerMarginEnd() {
            return 0;
        }

        @Override
        public void setUnfocusedWidth(int unfocusedWidth) {}

        @Override
        public void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
                boolean isSearchEngineGoogle, String searchEngineUrl) {}

        // Implements FakeBoxDelegate.
        @Override
        public boolean isUrlBarFocused() {
            return false;
        }

        @Override
        public void setUrlBarFocus(boolean shouldBeFocused, @Nullable String pastedText,
                @LocationBar.OmniboxFocusReason int reason) {}

        @Override
        public boolean isCurrentPage(NativePage nativePage) {
            return false;
        }

        @Override
        public LocationBarVoiceRecognitionHandler getLocationBarVoiceRecognitionHandler() {
            return null;
        }
    }
}
