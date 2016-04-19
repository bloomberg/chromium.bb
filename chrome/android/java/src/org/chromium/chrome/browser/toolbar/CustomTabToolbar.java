// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Pair;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.dom_distiller.DomDistillerServiceFactory;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.pageinfo.WebsiteSettingsPopup;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * The Toolbar layout to be used for a custom tab. This is used for both phone and tablet UIs.
 */
public class CustomTabToolbar extends ToolbarLayout implements LocationBar,
        View.OnLongClickListener {
    private static final int TITLE_ANIM_DELAY_MS = 800;
    private static final int STATE_DOMAIN_ONLY = 0;
    private static final int STATE_TITLE_ONLY = 1;
    private static final int STATE_DOMAIN_AND_TITLE = 2;

    private View mLocationBarFrameLayout;
    private View mTitleUrlContainer;
    private UrlBar mUrlBar;
    private TextView mTitleBar;
    private ImageView mSecurityButton;
    private ImageButton mCustomActionButton;
    private int mSecurityIconType;
    private ImageButton mCloseButton;

    // Whether dark tint should be applied to icons and text.
    private boolean mUseDarkColors = true;

    private CustomTabToolbarAnimationDelegate mAnimDelegate;
    private long mInitializeTimeStamp;
    private int mState = STATE_DOMAIN_ONLY;
    private String mFirstUrl;
    private boolean mShowsOfflinePage = false;

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
        mSecurityButton = (ImageButton) findViewById(R.id.security_button);
        mSecurityIconType = ConnectionSecurityLevel.NONE;
        mCustomActionButton = (ImageButton) findViewById(R.id.action_button);
        mCustomActionButton.setOnLongClickListener(this);
        mCloseButton = (ImageButton) findViewById(R.id.close_button);
        mCloseButton.setOnLongClickListener(this);
        mAnimDelegate = new CustomTabToolbarAnimationDelegate(mSecurityButton, mTitleUrlContainer);
    }

    @Override
    protected int getToolbarHeightWithoutShadowResId() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public void initialize(ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController, AppMenuButtonHelper appMenuButtonHelper) {
        super.initialize(toolbarDataProvider, tabController, appMenuButtonHelper);
        updateVisualsForState();
        mInitializeTimeStamp = System.currentTimeMillis();
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
                WebsiteSettingsPopup.show(activity, currentTab, mState == STATE_TITLE_ONLY
                        ? parsePublisherNameFromUrl(currentTab.getUrl()) : null);
            }
        });
    }

    @Override
    public void setCloseButtonImageResource(Drawable drawable) {
        mCloseButton.setImageDrawable(drawable);
    }

    @Override
    public void setCustomTabCloseClickHandler(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    @Override
    public void setCustomActionButton(Drawable drawable, String description,
            OnClickListener listener) {
        Resources resources = getResources();

        // The height will be scaled to match spec while keeping the aspect ratio, so get the scaled
        // width through that.
        int sourceHeight = drawable.getIntrinsicHeight();
        int sourceScaledHeight = resources.getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int sourceWidth = drawable.getIntrinsicWidth();
        int sourceScaledWidth = sourceWidth * sourceScaledHeight / sourceHeight;
        int minPadding = resources.getDimensionPixelSize(R.dimen.min_toolbar_icon_side_padding);

        int sidePadding = Math.max((2 * sourceScaledHeight - sourceScaledWidth) / 2, minPadding);
        int topPadding = mCustomActionButton.getPaddingTop();
        int bottomPadding = mCustomActionButton.getPaddingBottom();
        mCustomActionButton.setPadding(sidePadding, topPadding, sidePadding, bottomPadding);
        mCustomActionButton.setImageDrawable(drawable);

        mCustomActionButton.setContentDescription(description);
        mCustomActionButton.setOnClickListener(listener);
        mCustomActionButton.setVisibility(VISIBLE);
        updateButtonsTint();
    }

    /**
     * @return The custom action button. For test purpose only.
     */
    @VisibleForTesting
    public ImageButton getCustomActionButtonForTest() {
        return mCustomActionButton;
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
    public boolean shouldEmphasizeHttpsScheme() {
        int securityLevel = getSecurityLevel();
        if (securityLevel == ConnectionSecurityLevel.SECURITY_ERROR
                || securityLevel == ConnectionSecurityLevel.SECURITY_POLICY_WARNING) {
            return true;
        }
        return false;
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
            updateSecurityIcon(getSecurityLevel());
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
        Tab currentTab = getToolbarDataProvider().getTab();
        if (currentTab == null || TextUtils.isEmpty(currentTab.getTitle())) {
            mTitleBar.setText("");
            return;
        }
        String title = currentTab.getTitle();

        // It takes some time to parse the title of the webcontent, and before that Tab#getTitle
        // always return the url. We postpone the title animation until the title is authentic.
        if ((mState == STATE_DOMAIN_AND_TITLE || mState == STATE_TITLE_ONLY)
                && !title.equals(currentTab.getUrl())
                && !title.equals(UrlConstants.ABOUT_BLANK)) {
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
        showOfflineBoltIfNecessary();
    }

    @Override
    public void setUrlToPageUrl() {
        if (getCurrentTab() == null) {
            mUrlBar.setUrl("", null);
            return;
        }

        String url = getCurrentTab().getUrl().trim();

        // Don't show anything for Chrome URLs and "about:blank".
        // If we have taken a pre-initialized WebContents, then the starting URL
        // is "about:blank". We should not display it.
        if (NativePageFactory.isNativePageUrl(url, getCurrentTab().isIncognito())
                || UrlConstants.ABOUT_BLANK.equals(url)) {
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
            mUrlBar.deEmphasizeUrl();
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
        updateSecurityIcon(getSecurityLevel());
    }

    @Override
    public void updateVisualsForState() {
        Resources resources = getResources();
        updateSecurityIcon(getSecurityLevel());
        updateButtonsTint();
        mUrlBar.setUseDarkTextColors(mUseDarkColors);

        int titleTextColor = mUseDarkColors
                ? ApiCompatibilityUtils.getColor(resources, R.color.url_emphasis_default_text)
                : ApiCompatibilityUtils.getColor(resources,
                        R.color.url_emphasis_light_default_text);
        mTitleBar.setTextColor(titleTextColor);

        if (getProgressBar() != null) {
            if (!mUseDarkColors) {
                getProgressBar().setBackgroundColor(ColorUtils
                        .getLightProgressbarBackground(getToolbarDataProvider().getPrimaryColor()));
                getProgressBar().setForegroundColor(ApiCompatibilityUtils.getColor(resources,
                        R.color.progress_bar_foreground_white));
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
        if (mCloseButton.getDrawable() instanceof TintedDrawable) {
            ((TintedDrawable) mCloseButton.getDrawable()).setTint(
                    mUseDarkColors ? mDarkModeTint : mLightModeTint);
        }
        if (mCustomActionButton.getDrawable() instanceof TintedDrawable) {
            ((TintedDrawable) mCustomActionButton.getDrawable()).setTint(
                    mUseDarkColors ? mDarkModeTint : mLightModeTint);
        }
        if (mSecurityButton.getDrawable() instanceof TintedDrawable) {
            ((TintedDrawable) mSecurityButton.getDrawable()).setTint(
                    mUseDarkColors ? mDarkModeTint : mLightModeTint);
        }
    }

    @Override
    public void setMenuButtonHelper(final AppMenuButtonHelper helper) {
        mMenuButton.setOnTouchListener(new OnTouchListener() {
            @SuppressLint("ClickableViewAccessibility")
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                return helper.onTouch(v, event);
            }
        });
        mMenuButton.setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(View view, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_UP) {
                    return helper.onEnterKeyPress(view);
                }
                return false;
            }
        });
    }

    @Override
    public View getMenuAnchor() {
        return mMenuButton;
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
    public void initializeControls(WindowDelegate windowDelegate, ActionBarDelegate delegate,
            WindowAndroid windowAndroid) {
    }

    private int getSecurityLevel() {
        if (getCurrentTab() == null) return ConnectionSecurityLevel.NONE;
        return getCurrentTab().getSecurityLevel();
    }

    @Override
    public void updateSecurityIcon(int securityLevel) {
        if (mSecurityIconType == securityLevel || mState == STATE_TITLE_ONLY) return;

        mSecurityIconType = securityLevel;

        if (securityLevel == ConnectionSecurityLevel.NONE) {
            mAnimDelegate.hideSecurityButton();
        } else {
            int id = LocationBarLayout.getSecurityIconResource(
                    securityLevel, !shouldEmphasizeHttpsScheme());
            // ImageView#setImageResource is no-op if given resource is the current one.
            if (id == 0) {
                mSecurityButton.setImageDrawable(null);
            } else {
                mSecurityButton.setImageResource(id);
            }
            mAnimDelegate.showSecurityButton();
        }
        mUrlBar.emphasizeUrl();
        mUrlBar.invalidate();
    }

    private void showOfflineBoltIfNecessary() {
        boolean isOfflinePage = getCurrentTab() != null && getCurrentTab().isOfflinePage();
        if (isOfflinePage == mShowsOfflinePage) return;

        mShowsOfflinePage = isOfflinePage;
        if (mShowsOfflinePage) {
            // If we are showing an offline page, immediately update icon to offline bolt.
            TintedDrawable bolt = TintedDrawable.constructTintedDrawable(
                    getResources(), R.drawable.offline_bolt);
            bolt.setTint(mUseDarkColors ? mDarkModeTint : mLightModeTint);
            mSecurityButton.setImageDrawable(bolt);
            mAnimDelegate.showSecurityButton();
        } else {
            // We are hiding the offline page so connection security information will change.
            mSecurityIconType = ConnectionSecurityLevel.NONE;
            mAnimDelegate.hideSecurityButton();
        }
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    @Override
    protected void onPrimaryColorChanged(boolean shouldAnimate) {
        int primaryColor = getToolbarDataProvider().getPrimaryColor();
        getBackground().setColor(primaryColor);
        mUseDarkColors = !ColorUtils.shoudUseLightForegroundOnBackground(primaryColor);
        updateVisualsForState();
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
            if (childView.getVisibility() != GONE) {
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
    public boolean onLongClick(View v) {
        CharSequence description = null;
        if (v == mCloseButton) {
            description = getResources().getString(R.string.close_tab);
        } else if (v == mCustomActionButton) {
            description = mCustomActionButton.getContentDescription();
        } else {
            return false;
        }
        return showAccessibilityToast(v, description);
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
    public void setToolbarDataProvider(ToolbarDataProvider model) {}

    @Override
    public void onUrlPreFocusChanged(boolean gainFocus) {}

    @Override
    public void onTextChangedForAutocomplete(boolean canInlineAutocomplete) {}

    @Override
    public void setUrlFocusChangeListener(UrlFocusChangeListener listener) {}

    @Override
    public void setUrlBarFocus(boolean shouldBeFocused) {}

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
    public void backKeyPressed() {}

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
}
