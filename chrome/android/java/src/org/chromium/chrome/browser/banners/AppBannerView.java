// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.content.browser.ContentView;
import org.chromium.ui.base.LocalizationUtils;

/**
 * Lays out a banner for showing info about an app on the Play Store.
 * Rather than utilizing the Android RatingBar, which would require some nasty styling, a custom
 * View is used to paint a Drawable showing all the stars.  Showing different ratings is done by
 * adjusting the clipping rectangle width.
 */
public class AppBannerView extends SwipableOverlayView implements View.OnClickListener {
    /**
     * Class that is alerted about things happening to the BannerView.
     */
    public static interface Observer {
        /**
         * Called when the banner is dismissed.
         * @param banner Banner being dismissed.
         */
        public void onBannerDismissed(AppBannerView banner);

        /**
         * Called when the install button has been clicked.
         * @param banner Banner firing the event.
         */
        public void onButtonClicked(AppBannerView banner);

        /**
         * Called when something other than the button is clicked.
         * @param banner Banner firing the event.
         */
        public void onBannerClicked(AppBannerView banner);
    }

    // XML layout for the BannerView.
    private static final int BANNER_LAYOUT = R.layout.app_banner_view;

    // True if the layout is in left-to-right layout mode (regular mode).
    private final boolean mIsLayoutLTR;

    // Class to alert about BannerView events.
    private AppBannerView.Observer mObserver;

    // Views comprising the app banner.
    private ImageView mIconView;
    private TextView mTitleView;
    private Button mButtonView;
    private RatingView mRatingView;
    private ImageView mLogoView;

    // Information about the package.
    private AppData mAppData;

    // Variables used during layout calculations and saved to avoid reallocations.
    private final Point mSpaceMain;
    private final Point mSpaceForLogo;
    private final Point mSpaceForRating;
    private final Point mSpaceForTitle;

    // Dimension values.
    private int mDefinedMaxWidth;
    private int mPaddingContent;
    private int mMarginSide;
    private int mMarginBottom;

    // Initial padding values.
    private final Rect mBackgroundDrawablePadding;

    /**
     * Creates a BannerView and adds it to the given ContentView.
     * @param contentView ContentView to display the AppBannerView for.
     * @param observer    Class that is alerted for AppBannerView events.
     * @param data        Data about the app.
     * @return            The created banner.
     */
    public static AppBannerView create(ContentView contentView, Observer observer, AppData data) {
        Context context = contentView.getContext().getApplicationContext();
        AppBannerView banner =
                (AppBannerView) LayoutInflater.from(context).inflate(BANNER_LAYOUT, null);
        banner.initialize(observer, data);
        banner.addToView(contentView);
        return banner;
    }

    /**
     * Creates a BannerView from an XML layout.
     */
    public AppBannerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsLayoutLTR = !LocalizationUtils.isSystemLayoutDirectionRtl();
        mSpaceMain = new Point();
        mSpaceForLogo = new Point();
        mSpaceForRating = new Point();
        mSpaceForTitle = new Point();

        // Store the background Drawable's padding.  The background used for banners is a 9-patch,
        // which means that it already defines padding.  We need to take it into account when adding
        // even more padding to the inside of it.
        mBackgroundDrawablePadding = new Rect();
        mBackgroundDrawablePadding.left = ApiCompatibilityUtils.getPaddingStart(this);
        mBackgroundDrawablePadding.right = ApiCompatibilityUtils.getPaddingEnd(this);
        mBackgroundDrawablePadding.top = getPaddingTop();
        mBackgroundDrawablePadding.bottom = getPaddingBottom();
    }

    /**
     * Initialize the banner with information about the package.
     * @param observer Class to alert about changes to the banner.
     * @param data     Information about the app being advertised.
     */
    private void initialize(Observer observer, AppData data) {
        mObserver = observer;
        mAppData = data;
        initializeControls();
    }

    private void initializeControls() {
        // Cache the banner dimensions, adjusting margins for drop shadows defined in the background
        // Drawable.  The Drawable is defined to have the same margin on both the left and right.
        Resources res = getResources();
        mDefinedMaxWidth = res.getDimensionPixelSize(R.dimen.app_banner_max_width);
        mPaddingContent = res.getDimensionPixelSize(R.dimen.app_banner_padding_content);
        mMarginSide = res.getDimensionPixelSize(R.dimen.app_banner_margin_sides)
                - mBackgroundDrawablePadding.left;
        mMarginBottom = res.getDimensionPixelSize(R.dimen.app_banner_margin_bottom)
                - mBackgroundDrawablePadding.bottom;
        if (getLayoutParams() != null) {
            MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
            params.leftMargin = mMarginSide;
            params.rightMargin = mMarginSide;
            params.bottomMargin = mMarginBottom;
        }

        // Add onto the padding defined by the Drawable's drop shadow.
        int padding = res.getDimensionPixelSize(R.dimen.app_banner_padding);
        int paddingStart = mBackgroundDrawablePadding.left + padding;
        int paddingTop = mBackgroundDrawablePadding.top + padding;
        int paddingEnd = mBackgroundDrawablePadding.right + padding;
        int paddingBottom = mBackgroundDrawablePadding.bottom + padding;
        ApiCompatibilityUtils.setPaddingRelative(
                this, paddingStart, paddingTop, paddingEnd, paddingBottom);

        // Pull out all of the controls we are expecting.
        mIconView = (ImageView) findViewById(R.id.app_icon);
        mTitleView = (TextView) findViewById(R.id.app_title);
        mButtonView = (Button) findViewById(R.id.app_install_button);
        mRatingView = (RatingView) findViewById(R.id.app_rating);
        mLogoView = (ImageView) findViewById(R.id.store_logo);
        assert mIconView != null;
        assert mTitleView != null;
        assert mButtonView != null;
        assert mLogoView != null;
        assert mRatingView != null;

        // Set up the button to fire an event.
        mButtonView.setOnClickListener(this);

        // Configure the controls with the package information.
        mTitleView.setText(mAppData.title());
        mIconView.setImageDrawable(mAppData.icon());
        mRatingView.initialize(mAppData.rating());

        // Update the button state.
        updateButtonState();
    }

    @Override
    public void onClick(View view) {
        if (mObserver != null && view == mButtonView) mObserver.onButtonClicked(this);
    }

    @Override
    protected void onViewClicked() {
        if (mObserver != null) mObserver.onBannerClicked(this);
    }

    @Override
    protected ViewGroup.MarginLayoutParams createLayoutParams() {
        // Define the margin around the entire banner that accounts for the drop shadow.
        ViewGroup.MarginLayoutParams params = super.createLayoutParams();
        params.setMargins(mMarginSide, 0, mMarginSide, mMarginBottom);
        return params;
    }

    /**
     * Removes this View from its parent and alerts any observers of the dismissal.
     * @return Whether or not the View was successfully dismissed.
     */
    @Override
    boolean removeFromParent() {
        boolean removed = super.removeFromParent();
        if (removed) mObserver.onBannerDismissed(this);
        return removed;
    }

    /**
     * Returns data for the app the banner is being shown for.
     * @return The AppData being used by the banner.
     */
    AppData getAppData() {
        return mAppData;
    }

    /**
     * Updates the text and color of the button displayed on the button.
     */
    void updateButtonState() {
        if (mButtonView == null) return;

        int bgColor;
        int fgColor;
        String text;
        if (mAppData.installState() == AppData.INSTALL_STATE_INSTALLED) {
            bgColor = getResources().getColor(R.color.app_banner_open_button_bg);
            fgColor = getResources().getColor(R.color.app_banner_open_button_fg);
            text = getResources().getString(R.string.app_banner_open);
        } else {
            bgColor = getResources().getColor(R.color.app_banner_install_button_bg);
            fgColor = getResources().getColor(R.color.app_banner_install_button_fg);
            if (mAppData.installState() == AppData.INSTALL_STATE_NOT_INSTALLED) {
                text = mAppData.installButtonText();
            } else {
                text = getResources().getString(R.string.app_banner_installing);
            }
        }

        mButtonView.setBackgroundColor(bgColor);
        mButtonView.setTextColor(fgColor);
        mButtonView.setText(text);
    }

    /**
     * Determine how big an icon needs to be for the Layout.
     * @param context Context to grab resources from.
     * @return        How big the icon is expected to be, in pixels.
     */
    static int getIconSize(Context context) {
        return context.getResources().getDimensionPixelSize(R.dimen.app_banner_icon_size);
    }

    /**
     * Fade the banner back into view.
     */
    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        ObjectAnimator.ofFloat(this, "alpha", getAlpha(), 1.f).setDuration(
                MS_ANIMATION_DURATION).start();
        setVisibility(VISIBLE);
    }

    /**
     * Immediately hide the banner to avoid having them show up in snapshots.
     */
    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        setVisibility(INVISIBLE);
    }

    /**
     * Watch for changes in the available screen height, which triggers a complete recreation of the
     * banner widgets.  This is mainly due to the fact that the Nexus 7 has a smaller banner defined
     * for its landscape versus its portrait layouts.
     */
    @Override
    protected void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);

        // If the card's maximum width hasn't changed, the individual views can't have, either.
        int newDefinedWidth = getResources().getDimensionPixelSize(R.dimen.app_banner_max_width);
        if (mDefinedMaxWidth == newDefinedWidth) return;

        // Cannibalize another version of this layout to get Views using the new resources and
        // sizes.
        while (getChildCount() > 0) removeViewAt(0);
        mIconView = null;
        mTitleView = null;
        mButtonView = null;
        mRatingView = null;
        mLogoView = null;

        AppBannerView cannibalized =
                (AppBannerView) LayoutInflater.from(getContext()).inflate(BANNER_LAYOUT, null);
        while (cannibalized.getChildCount() > 0) {
            View child = cannibalized.getChildAt(0);
            cannibalized.removeViewAt(0);
            addView(child);
        }
        initializeControls();
        requestLayout();
    }

    /**
     * Measurement for components of the banner are performed using the following procedure:
     *
     * 00000000000000000000000000000000000000000000000000000
     * 01111155555555555555555555555555555555555555555555550
     * 01111155555555555555555555555555555555555555555555550
     * 01111144444444444440000000000000000000000222222222220
     * 01111133333333333330000000000000000000000222222222220
     * 00000000000000000000000000000000000000000000000000000
     *
     * 0) A maximum width is enforced on the banner, based on the smallest width of the screen,
     *    then padding defined by the 9-patch background Drawable is subtracted from all sides.
     * 1) The icon takes up the left side of the banner.
     * 2) The install button occupies the bottom-right of the banner.
     * 3) The Google Play logo occupies the space to the left of the button.
     * 4) The rating is assigned space above the logo and below the title.
     * 5) The title is assigned whatever space is left.  The maximum height of the banner is defined
     *    by deducting the height of either the install button or the logo + rating, (which is
     *    bigger).  If the title cannot fit two lines comfortably, it is shrunk down to one.
     *
     * See {@link #android.view.View.onMeasure(int, int)} for the parameters.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Enforce a maximum width on the banner.
        Resources res = getResources();
        float density = res.getDisplayMetrics().density;
        int screenSmallestWidth = (int) (res.getConfiguration().smallestScreenWidthDp * density);
        int specWidth = MeasureSpec.getSize(widthMeasureSpec);
        int maxWidth = Math.min(Math.min(specWidth, mDefinedMaxWidth), screenSmallestWidth);
        int maxHeight = MeasureSpec.getSize(heightMeasureSpec);

        // Track how much space is available for the banner content.
        mSpaceMain.x = maxWidth - ApiCompatibilityUtils.getPaddingStart(this)
                - ApiCompatibilityUtils.getPaddingEnd(this);
        mSpaceMain.y = maxHeight - getPaddingTop() - getPaddingBottom();

        // Measure the icon, which hugs the banner's starting edge and defines the banner's height.
        measureChildForSpace(mIconView, mSpaceMain);
        mSpaceMain.x -= getChildWidthWithMargins(mIconView);
        mSpaceMain.y = getChildHeightWithMargins(mIconView) + getPaddingTop() + getPaddingBottom();

        // Additional padding is defined by the mock for non-icon content on the end and bottom.
        mSpaceMain.x -= mPaddingContent;
        mSpaceMain.y -= mPaddingContent;

        // Measure the install button, which sits in the bottom-right corner.
        measureChildForSpace(mButtonView, mSpaceMain);

        // Measure the logo, which sits in the bottom-left corner next to the icon.
        mSpaceForLogo.x = mSpaceMain.x - getChildWidthWithMargins(mButtonView);
        mSpaceForLogo.y = mSpaceMain.y;
        measureChildForSpace(mLogoView, mSpaceForLogo);

        // Measure the star rating, which sits below the title and above the logo.
        mSpaceForRating.x = mSpaceForLogo.x;
        mSpaceForRating.y = mSpaceForLogo.y - getChildHeightWithMargins(mLogoView);
        measureChildForSpace(mRatingView, mSpaceForRating);

        // The app title spans the top of the banner.
        mSpaceForTitle.x = mSpaceMain.x;
        mSpaceForTitle.y = mSpaceMain.y - getChildHeightWithMargins(mLogoView)
                - getChildHeightWithMargins(mRatingView);
        measureChildForSpace(mTitleView, mSpaceForTitle);

        // Set the measured dimensions for the banner.
        int measuredHeight = mIconView.getMeasuredHeight() + getPaddingTop() + getPaddingBottom();
        setMeasuredDimension(maxWidth, measuredHeight);
    }

    /**
     * Lays out the controls according to the algorithm in {@link #onMeasure}.
     * See {@link #android.view.View.onLayout(boolean, int, int, int, int)} for the parameters.
     */
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);

        int top = getPaddingTop();
        int bottom = getMeasuredHeight() - getPaddingBottom();
        int start = ApiCompatibilityUtils.getPaddingStart(this);
        int end = getMeasuredWidth() - ApiCompatibilityUtils.getPaddingEnd(this);

        // Lay out the icon.
        int iconWidth = mIconView.getMeasuredWidth();
        int iconLeft = mIsLayoutLTR ? start : (getMeasuredWidth() - start - iconWidth);
        mIconView.layout(iconLeft, top, iconLeft + iconWidth, top + mIconView.getMeasuredHeight());
        start += getChildWidthWithMargins(mIconView);

        // Factor in the additional padding.
        end -= mPaddingContent;
        bottom -= mPaddingContent;

        // Lay out the app title text.
        int titleWidth = mTitleView.getMeasuredWidth();
        int titleTop = top + ((MarginLayoutParams) mTitleView.getLayoutParams()).topMargin;
        int titleBottom = titleTop + mTitleView.getMeasuredHeight();
        int titleLeft = mIsLayoutLTR ? start : (getMeasuredWidth() - start - titleWidth);
        mTitleView.layout(titleLeft, titleTop, titleLeft + titleWidth, titleBottom);

        // The mock shows the margin eating into the descender area of the TextView.
        int textBaseline = mTitleView.getLineBounds(mTitleView.getLineCount() - 1, null);
        top = titleTop + textBaseline
                + ((MarginLayoutParams) mTitleView.getLayoutParams()).bottomMargin;

        // Lay out the app rating below the title.
        int starWidth = mRatingView.getMeasuredWidth();
        int starTop = top + ((MarginLayoutParams) mRatingView.getLayoutParams()).topMargin;
        int starBottom = starTop + mRatingView.getMeasuredHeight();
        int starLeft = mIsLayoutLTR ? start : (getMeasuredWidth() - start - starWidth);
        mRatingView.layout(starLeft, starTop, starLeft + starWidth, starBottom);

        // Lay out the logo in the bottom-left.
        int logoWidth = mLogoView.getMeasuredWidth();
        int logoBottom = bottom - ((MarginLayoutParams) mLogoView.getLayoutParams()).bottomMargin;
        int logoTop = logoBottom - mLogoView.getMeasuredHeight();
        int logoLeft = mIsLayoutLTR ? start : (getMeasuredWidth() - start - logoWidth);
        mLogoView.layout(logoLeft, logoTop, logoLeft + logoWidth, logoBottom);

        // Lay out the install button in the bottom-right corner.
        int buttonHeight = mButtonView.getMeasuredHeight();
        int buttonWidth = mButtonView.getMeasuredWidth();
        int buttonRight = mIsLayoutLTR ? end : (getMeasuredWidth() - end + buttonWidth);
        int buttonLeft = buttonRight - buttonWidth;
        mButtonView.layout(buttonLeft, bottom - buttonHeight, buttonRight, bottom);
    }

    /**
     * Calculates how wide the given View has been measured to be, including its margins.
     * @param child Child to measure.
     * @return      Measured width of the child plus its margins.
     */
    private int getChildWidthWithMargins(View child) {
        MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();
        return child.getMeasuredWidth() + ApiCompatibilityUtils.getMarginStart(params)
                + ApiCompatibilityUtils.getMarginEnd(params);
    }

    /**
     * Calculates how tall the given View has been measured to be, including its margins.
     * @param child Child to measure.
     * @return Measured height of the child plus its margins.
     */
    private static int getChildHeightWithMargins(View child) {
        MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();
        return child.getMeasuredHeight() + params.topMargin + params.bottomMargin;
    }

    /**
     * Measures a child so that it fits within the given space, taking into account heights defined
     * in the layout.
     * @param child     View to measure.
     * @param available Available space, with width stored in the x coordinate and height in the y.
     */
    private void measureChildForSpace(View child, Point available) {
        int childHeight = child.getLayoutParams().height;
        int maxHeight = childHeight > 0 ? Math.min(available.y, childHeight) : available.y;
        int widthSpec = MeasureSpec.makeMeasureSpec(available.x, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(maxHeight, MeasureSpec.AT_MOST);
        measureChildWithMargins(child, widthSpec, 0, heightSpec, 0);
    }
}
