// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
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
         * Called when the BannerView is dismissed.
         * @param banner BannerView being dismissed.
         */
        public void onBannerDismissed(AppBannerView banner);

        /**
         * Called when the button has been clicked.
         * @param banner BannerView firing the event.
         */
        public void onButtonClicked(AppBannerView banner);
    }

    // Maximum number of stars in the rating.
    private static final int NUM_STARS = 5;

    // XML layout for the BannerView.
    private static final int BANNER_LAYOUT = R.layout.app_banner_view;

    // True if the layout is in left-to-right layout mode (regular mode).
    private final boolean mIsLayoutLTR;

    // Class to alert when the BannerView is dismissed.
    private Observer mObserver;

    // Views comprising the app banner.
    private ImageView mIconView;
    private TextView mTitleView;
    private Button mButtonView;
    private ImageView mRatingView;
    private ImageView mLogoView;
    private ClipDrawable mRatingClipperDrawable;

    // Information about the package.
    private String mUrl;
    private String mPackageName;
    private float mAppRating;

    // Variables used during layout calculations and saved to avoid reallocations.
    private final Point mSpaceMain;
    private final Point mSpaceForLogo;
    private final Point mSpaceForRating;
    private final Point mSpaceForTitle;

    /**
     * Creates a BannerView and adds it to the given ContentView.
     * @param contentView ContentView to display the BannerView for.
     * @param observer    Class that is alerted for BannerView events.
     * @param icon        Icon to display for the app.
     * @param title       Title of the app.
     * @param rating      Rating of the app.
     * @param buttonText  Text to show on the button.
     */
    public static AppBannerView create(ContentView contentView, Observer observer, String url,
            String packageName, String title, Drawable icon, float rating, String buttonText) {
        Context context = contentView.getContext().getApplicationContext();
        AppBannerView banner =
                (AppBannerView) LayoutInflater.from(context).inflate(BANNER_LAYOUT, null);
        banner.initialize(observer, url, packageName, title, icon, rating, buttonText);
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
    }

    /**
     * Initialize the banner with information about the package.
     * @param observer   Class to alert about changes to the banner.
     * @param title      Title of the package.
     * @param icon       Icon for the package.
     * @param rating     Play store rating for the package.
     * @param buttonText Text to show on the button.
     */
    private void initialize(Observer observer, String url, String packageName,
            String title, Drawable icon, float rating, String buttonText) {
        mObserver = observer;
        mUrl = url;
        mPackageName = packageName;

        // Pull out all of the controls we are expecting.
        mIconView = (ImageView) findViewById(R.id.app_icon);
        mTitleView = (TextView) findViewById(R.id.app_title);
        mButtonView = (Button) findViewById(R.id.app_install_button);
        mRatingView = (ImageView) findViewById(R.id.app_rating);
        mLogoView = (ImageView) findViewById(R.id.store_logo);
        assert mIconView != null;
        assert mTitleView != null;
        assert mButtonView != null;
        assert mLogoView != null;
        assert mRatingView != null;

        // Set up the button to fire an event.
        mButtonView.setOnClickListener(this);

        // Configure the controls with the package information.
        mTitleView.setText(title);
        mIconView.setImageDrawable(icon);
        mAppRating = rating;
        mButtonView.setText(buttonText);
        initializeRatingView();
    }

    private void initializeRatingView() {
        // Set up the stars Drawable.
        Drawable ratingDrawable = getResources().getDrawable(R.drawable.app_banner_rating);
        mRatingClipperDrawable =
                new ClipDrawable(ratingDrawable, Gravity.START, ClipDrawable.HORIZONTAL);
        mRatingView.setImageDrawable(mRatingClipperDrawable);

        // Clips the ImageView for the ratings so that it shows an appropriate number of stars.
        // Ratings are rounded to the nearest 0.5 increment, like in the Play Store.
        float roundedRating = Math.round(mAppRating * 2) / 2.0f;
        float percentageRating = roundedRating / NUM_STARS;
        int clipLevel = (int) (percentageRating * 10000);
        mRatingClipperDrawable.setLevel(clipLevel);
    }

    @Override
    public void onClick(View view) {
        if (mObserver != null && view == mButtonView) {
            mObserver.onButtonClicked(this);
        }
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
     * @return The URL that the banner was created for.
     */
    String getUrl() {
        return mUrl;
    }

    /**
     * @return The package that the banner is displaying information for.
     */
    String getPackageName() {
        return mPackageName;
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
        int definedMaxWidth = (int) res.getDimension(R.dimen.app_banner_max_width);
        int specWidth = MeasureSpec.getSize(widthMeasureSpec);
        int maxWidth = Math.min(Math.min(specWidth, definedMaxWidth), screenSmallestWidth);
        int maxHeight = MeasureSpec.getSize(heightMeasureSpec);

        // Track how much space is available for the banner content.
        mSpaceMain.x = maxWidth - ApiCompatibilityUtils.getPaddingStart(this)
                - ApiCompatibilityUtils.getPaddingEnd(this);
        mSpaceMain.y = maxHeight - getPaddingTop() - getPaddingBottom();

        // Measure the icon, which hugs the banner's starting edge and defines the banner's height.
        measureChildForSpace(mIconView, mSpaceMain);
        mSpaceMain.x -= getChildWidthWithMargins(mIconView);
        mSpaceMain.y = getChildHeightWithMargins(mIconView) + getPaddingTop() + getPaddingBottom();

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
        mTitleView.setMaxLines(2);
        measureChildForSpace(mTitleView, mSpaceForTitle);

        // Ensure the text doesn't get cut in half through one of the lines.
        int requiredHeight = mTitleView.getLineHeight() * mTitleView.getLineCount();
        if (getChildHeightWithMargins(mTitleView) < requiredHeight) {
            mTitleView.setMaxLines(1);
            measureChildForSpace(mTitleView, mSpaceForTitle);
        }

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
        int top = getPaddingTop();
        int bottom = getMeasuredHeight() - getPaddingBottom();
        int start = ApiCompatibilityUtils.getPaddingStart(this);
        int end = getMeasuredWidth() - ApiCompatibilityUtils.getPaddingEnd(this);

        // Lay out the icon.
        int iconWidth = mIconView.getMeasuredWidth();
        int iconLeft = mIsLayoutLTR ? start : (getMeasuredWidth() - start - iconWidth);
        mIconView.layout(iconLeft, top, iconLeft + iconWidth, top + mIconView.getMeasuredHeight());
        start += getChildWidthWithMargins(mIconView);

        // Lay out the app title text.  The TextView seems to internally account for its margins.
        int titleWidth = mTitleView.getMeasuredWidth();
        int titleTop = top;
        int titleBottom = titleTop + getChildHeightWithMargins(mTitleView);
        int titleLeft = mIsLayoutLTR ? start : (getMeasuredWidth() - start - titleWidth);
        mTitleView.layout(titleLeft, titleTop, titleLeft + titleWidth, titleBottom);
        top += getChildHeightWithMargins(mTitleView);

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
