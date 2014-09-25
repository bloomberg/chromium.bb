// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

/**
 * Lays out a banner for showing info about an app on the Play Store.
 * The banner mimics the appearance of a Google Now card using a background Drawable with a shadow.
 *
 * PADDING CALCULATIONS
 * The banner has three different types of padding that need to be accounted for:
 * 1) The background Drawable of the banner looks like card with a drop shadow.  The Drawable
 *    defines a padding around the card that solely encompasses the space occupied by the drop
 *    shadow.
 * 2) The card itself needs to have padding so that the widgets don't abut the borders of the card.
 *    This is defined as mPaddingCard, and is equally applied to all four sides.
 * 3) Controls other than the icon are further constrained by mPaddingControls, which applies only
 *    to the bottom and end margins.
 * See {@link #AppBannerView.onMeasure(int, int)} for details.
 *
 * MARGIN CALCULATIONS
 * Margin calculations for the banner are complicated by the background Drawable's drop shadows,
 * since the drop shadows are meant to be counted as being part of the margin.  To deal with this,
 * the margins are calculated by deducting the background Drawable's padding from the margins
 * defined by the XML files.
 *
 * EVEN MORE LAYOUT QUIRKS
 * The layout of the banner, which includes its widget sizes, may change when the screen is rotated
 * to account for less screen real estate.  This means that all of the View's widgets and cached
 * dimensions must be rebuilt from scratch.
 */
public class AppBannerView extends SwipableOverlayView
        implements View.OnClickListener, InstallerDelegate.Observer, IntentCallback {
    private static final String TAG = "AppBannerView";

    /**
     * Class that is alerted about things happening to the BannerView.
     */
    public static interface Observer {
        /**
         * Called when the banner is removed from the hierarchy.
         * @param banner Banner being dismissed.
         */
        public void onBannerRemoved(AppBannerView banner);

        /**
         * Called when the user manually closes a banner.
         * @param banner      Banner being blocked.
         * @param url         URL of the page that requested the banner.
         * @param packageName Name of the app's package.
         */
        public void onBannerBlocked(AppBannerView banner, String url, String packageName);

        /**
         * Called when the banner begins to be dismissed.
         * @param banner      Banner being closed.
         * @param dismissType Type of dismissal performed.
         */
        public void onBannerDismissEvent(AppBannerView banner, int dismissType);

        /**
         * Called when an install event has occurred.
         */
        public void onBannerInstallEvent(AppBannerView banner, int eventType);

        /**
         * Called when the banner needs to have an Activity started for a result.
         * @param banner Banner firing the event.
         * @param intent Intent to fire.
         */
        public boolean onFireIntent(AppBannerView banner, PendingIntent intent);
    }

    // Installation states.
    private static final int INSTALL_STATE_NOT_INSTALLED = 0;
    private static final int INSTALL_STATE_INSTALLING = 1;
    private static final int INSTALL_STATE_INSTALLED = 2;

    // XML layout for the BannerView.
    private static final int BANNER_LAYOUT = R.layout.app_banner_view;

    // True if the layout is in left-to-right layout mode (regular mode).
    private final boolean mIsLayoutLTR;

    // Class to alert about BannerView events.
    private AppBannerView.Observer mObserver;

    // Information about the package.  Shouldn't ever be null after calling {@link #initialize()}.
    private AppData mAppData;

    // Views comprising the app banner.
    private ImageView mIconView;
    private TextView mTitleView;
    private Button mInstallButtonView;
    private RatingView mRatingView;
    private View mLogoView;
    private View mBannerHighlightView;
    private ImageButton mCloseButtonView;

    // Dimension values.
    private int mDefinedMaxWidth;
    private int mPaddingCard;
    private int mPaddingControls;
    private int mMarginLeft;
    private int mMarginRight;
    private int mMarginBottom;
    private int mTouchSlop;

    // Highlight variables.
    private boolean mIsBannerPressed;
    private float mInitialXForHighlight;

    // Initial padding values.
    private final Rect mBackgroundDrawablePadding;

    // Install tracking.
    private boolean mWasInstallDialogShown;
    private InstallerDelegate mInstallTask;
    private int mInstallState;

    /**
     * Creates a BannerView and adds it to the given ContentViewCore.
     * @param contentViewCore ContentViewCore to display the AppBannerView for.
     * @param observer    Class that is alerted for AppBannerView events.
     * @param data        Data about the app.
     * @return            The created banner.
     */
    public static AppBannerView create(
            ContentViewCore contentViewCore, Observer observer, AppData data) {
        Context context = contentViewCore.getContext().getApplicationContext();
        AppBannerView banner =
                (AppBannerView) LayoutInflater.from(context).inflate(BANNER_LAYOUT, null);
        banner.initialize(observer, data);
        banner.addToView(contentViewCore);
        return banner;
    }

    /**
     * Creates a BannerView from an XML layout.
     */
    public AppBannerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIsLayoutLTR = !LocalizationUtils.isLayoutRtl();

        // Store the background Drawable's padding.  The background used for banners is a 9-patch,
        // which means that it already defines padding.  We need to take it into account when adding
        // even more padding to the inside of it.
        mBackgroundDrawablePadding = new Rect();
        mBackgroundDrawablePadding.left = ApiCompatibilityUtils.getPaddingStart(this);
        mBackgroundDrawablePadding.right = ApiCompatibilityUtils.getPaddingEnd(this);
        mBackgroundDrawablePadding.top = getPaddingTop();
        mBackgroundDrawablePadding.bottom = getPaddingBottom();

        mInstallState = INSTALL_STATE_NOT_INSTALLED;
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
        // Drawable.
        Resources res = getResources();
        mDefinedMaxWidth = res.getDimensionPixelSize(R.dimen.app_banner_max_width);
        mPaddingCard = res.getDimensionPixelSize(R.dimen.app_banner_padding);
        mPaddingControls = res.getDimensionPixelSize(R.dimen.app_banner_padding_controls);
        mMarginLeft = res.getDimensionPixelSize(R.dimen.app_banner_margin_sides)
                - mBackgroundDrawablePadding.left;
        mMarginRight = res.getDimensionPixelSize(R.dimen.app_banner_margin_sides)
                - mBackgroundDrawablePadding.right;
        mMarginBottom = res.getDimensionPixelSize(R.dimen.app_banner_margin_bottom)
                - mBackgroundDrawablePadding.bottom;
        if (getLayoutParams() != null) {
            MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
            params.leftMargin = mMarginLeft;
            params.rightMargin = mMarginRight;
            params.bottomMargin = mMarginBottom;
        }

        // Pull out all of the controls we are expecting.
        mIconView = (ImageView) findViewById(R.id.app_icon);
        mTitleView = (TextView) findViewById(R.id.app_title);
        mInstallButtonView = (Button) findViewById(R.id.app_install_button);
        mRatingView = (RatingView) findViewById(R.id.app_rating);
        mLogoView = findViewById(R.id.store_logo);
        mBannerHighlightView = findViewById(R.id.banner_highlight);
        mCloseButtonView = (ImageButton) findViewById(R.id.close_button);

        assert mIconView != null;
        assert mTitleView != null;
        assert mInstallButtonView != null;
        assert mLogoView != null;
        assert mRatingView != null;
        assert mBannerHighlightView != null;
        assert mCloseButtonView != null;

        // Set up the buttons to fire an event.
        mInstallButtonView.setOnClickListener(this);
        mCloseButtonView.setOnClickListener(this);

        // Configure the controls with the package information.
        mTitleView.setText(mAppData.title());
        mIconView.setImageDrawable(mAppData.icon());
        mRatingView.initialize(mAppData.rating());
        setAccessibilityInformation();

        // Determine how much the user can drag sideways before their touch is considered a scroll.
        mTouchSlop = ViewConfiguration.get(getContext()).getScaledTouchSlop();

        // Set up the install button.
        updateButtonStatus();
    }

    /**
     * Creates a succinct description about the app being advertised.
     */
    private void setAccessibilityInformation() {
        String bannerText = getContext().getString(
                R.string.app_banner_view_accessibility, mAppData.title(), mAppData.rating());
        setContentDescription(bannerText);
    }

    @Override
    public void onClick(View view) {
        if (mObserver == null) return;

        // Only allow the button to be clicked when the banner's in a neutral position.
        if (Math.abs(getTranslationX()) > ZERO_THRESHOLD
                || Math.abs(getTranslationY()) > ZERO_THRESHOLD) {
            return;
        }

        if (view == mInstallButtonView) {
            // Check that nothing happened in the background to change the install state of the app.
            int previousState = mInstallState;
            updateButtonStatus();
            if (mInstallState != previousState) return;

            // Ignore button clicks when the app is installing.
            if (mInstallState == INSTALL_STATE_INSTALLING) return;

            mInstallButtonView.setEnabled(false);

            if (mInstallState == INSTALL_STATE_NOT_INSTALLED) {
                // The user initiated an install. Track it happening only once.
                if (!mWasInstallDialogShown) {
                    mObserver.onBannerInstallEvent(this, AppBannerMetricsIds.INSTALL_TRIGGERED);
                    mWasInstallDialogShown = true;
                }

                if (mObserver.onFireIntent(this, mAppData.installIntent())) {
                    // Temporarily hide the banner.
                    createVerticalSnapAnimation(false);
                } else {
                    Log.e(TAG, "Failed to fire install intent.");
                    dismiss(AppBannerMetricsIds.DISMISS_ERROR);
                }
            } else if (mInstallState == INSTALL_STATE_INSTALLED) {
                // The app is installed. Open it.
                try {
                    Intent appIntent = getAppLaunchIntent();
                    if (appIntent != null) getContext().startActivity(appIntent);
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "Failed to find app package: " + mAppData.packageName());
                }

                dismiss(AppBannerMetricsIds.DISMISS_APP_OPEN);
            }
        } else if (view == mCloseButtonView) {
            if (mObserver != null) {
                mObserver.onBannerBlocked(this, mAppData.siteUrl(), mAppData.packageName());
            }

            dismiss(AppBannerMetricsIds.DISMISS_CLOSE_BUTTON);
        }
    }

    @Override
    protected void onViewSwipedAway() {
        if (mObserver == null) return;
        mObserver.onBannerDismissEvent(this, AppBannerMetricsIds.DISMISS_BANNER_SWIPE);
        mObserver.onBannerBlocked(this, mAppData.siteUrl(), mAppData.packageName());
    }

    @Override
    protected void onViewClicked() {
        // Send the user to the app's Play store page.
        try {
            IntentSender sender = mAppData.detailsIntent().getIntentSender();
            getContext().startIntentSender(sender, new Intent(), 0, 0, 0);
        } catch (IntentSender.SendIntentException e) {
            Log.e(TAG, "Failed to launch details intent.");
        }

        dismiss(AppBannerMetricsIds.DISMISS_BANNER_CLICK);
    }

    @Override
    protected void onViewPressed(MotionEvent event) {
        // Highlight the banner when the user has held it for long enough and doesn't move.
        mInitialXForHighlight = event.getRawX();
        mIsBannerPressed = true;
        mBannerHighlightView.setVisibility(View.VISIBLE);
    }

    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode,
            ContentResolver contentResolver, Intent data) {
        if (isDismissed()) return;

        createVerticalSnapAnimation(true);
        if (resultCode == Activity.RESULT_OK) {
            // The user chose to install the app. Watch the PackageManager to see when it finishes
            // installing it.
            mObserver.onBannerInstallEvent(this, AppBannerMetricsIds.INSTALL_STARTED);

            PackageManager pm = getContext().getPackageManager();
            mInstallTask =
                    new InstallerDelegate(Looper.getMainLooper(), pm, this, mAppData.packageName());
            mInstallTask.start();
            mInstallState = INSTALL_STATE_INSTALLING;
        }
        updateButtonStatus();
    }


    @Override
    public void onInstallFinished(InstallerDelegate monitor, boolean success) {
        if (isDismissed() || mInstallTask != monitor) return;

        if (success) {
            // Let the user open the app from here.
            mObserver.onBannerInstallEvent(this, AppBannerMetricsIds.INSTALL_COMPLETED);
            mInstallState = INSTALL_STATE_INSTALLED;
            updateButtonStatus();
        } else {
            dismiss(AppBannerMetricsIds.DISMISS_INSTALL_TIMEOUT);
        }
    }

    @Override
    protected ViewGroup.MarginLayoutParams createLayoutParams() {
        // Define the margin around the entire banner that accounts for the drop shadow.
        ViewGroup.MarginLayoutParams params = super.createLayoutParams();
        params.setMargins(mMarginLeft, 0, mMarginRight, mMarginBottom);
        return params;
    }

    /**
     * Removes this View from its parent and alerts any observers of the dismissal.
     * @return Whether or not the View was successfully dismissed.
     */
    @Override
    boolean removeFromParent() {
        if (super.removeFromParent()) {
            mObserver.onBannerRemoved(this);
            destroy();
            return true;
        }

        return false;
    }

    /**
     * Dismisses the banner.
     * @param eventType Event that triggered the dismissal.  See {@link AppBannerMetricsIds}.
     */
    public void dismiss(int eventType) {
        if (isDismissed() || mObserver == null) return;

        dismiss(eventType == AppBannerMetricsIds.DISMISS_CLOSE_BUTTON);
        mObserver.onBannerDismissEvent(this, eventType);
    }

    /**
     * Destroys the Banner.
     */
    public void destroy() {
        if (!isDismissed()) dismiss(AppBannerMetricsIds.DISMISS_ERROR);

        if (mInstallTask != null) {
            mInstallTask.cancel();
            mInstallTask = null;
        }
    }

    /**
     * Updates the install button (install state, text, color, etc.).
     */
    void updateButtonStatus() {
        if (mInstallButtonView == null) return;

        // Determine if the saved install status of the app is out of date.
        // It is not easily possible to detect if an app is in the process of being installed, so we
        // can't properly transition to that state from here.
        if (getAppLaunchIntent() == null) {
            if (mInstallState == INSTALL_STATE_INSTALLED) {
                mInstallState = INSTALL_STATE_NOT_INSTALLED;
            }
        } else {
            mInstallState = INSTALL_STATE_INSTALLED;
        }

        // Update what the button looks like.
        Resources res = getResources();
        int fgColor;
        String text;
        if (mInstallState == INSTALL_STATE_INSTALLED) {
            ApiCompatibilityUtils.setBackgroundForView(mInstallButtonView,
                    res.getDrawable(R.drawable.app_banner_button_open));
            fgColor = res.getColor(R.color.app_banner_open_button_fg);
            text = res.getString(R.string.app_banner_open);
        } else {
            ApiCompatibilityUtils.setBackgroundForView(mInstallButtonView,
                    res.getDrawable(R.drawable.app_banner_button_install));
            fgColor = res.getColor(R.color.app_banner_install_button_fg);
            if (mInstallState == INSTALL_STATE_NOT_INSTALLED) {
                text = mAppData.installButtonText();
                mInstallButtonView.setContentDescription(
                        getContext().getString(R.string.app_banner_install_accessibility, text));
            } else {
                text = res.getString(R.string.app_banner_installing);
            }
        }

        mInstallButtonView.setTextColor(fgColor);
        mInstallButtonView.setText(text);
        mInstallButtonView.setEnabled(mInstallState != INSTALL_STATE_INSTALLING);
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
     * Passes all touch events through to the parent.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        if (mIsBannerPressed) {
            // Mimic Google Now card behavior, where the card stops being highlighted if the user
            // scrolls a bit to the side.
            float xDifference = Math.abs(event.getRawX() - mInitialXForHighlight);
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL
                    || (action == MotionEvent.ACTION_MOVE && xDifference > mTouchSlop)) {
                mIsBannerPressed = false;
                mBannerHighlightView.setVisibility(View.INVISIBLE);
            }
        }

        return super.onTouchEvent(event);
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
        setAlpha(0.0f);
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

        if (isDismissed()) return;

        // If the card's maximum width hasn't changed, the individual views can't have, either.
        int newDefinedWidth = getResources().getDimensionPixelSize(R.dimen.app_banner_max_width);
        if (mDefinedMaxWidth == newDefinedWidth) return;

        // Cannibalize another version of this layout to get Views using the new resources and
        // sizes.
        while (getChildCount() > 0) removeViewAt(0);
        mIconView = null;
        mTitleView = null;
        mInstallButtonView = null;
        mRatingView = null;
        mLogoView = null;
        mBannerHighlightView = null;

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

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (hasWindowFocus) updateButtonStatus();
    }

    /**
     * @return Intent to launch the app that is being promoted.
     */
    private Intent getAppLaunchIntent() {
        String packageName = mAppData.packageName();
        PackageManager packageManager = getContext().getPackageManager();
        return packageManager.getLaunchIntentForPackage(packageName);
    }

    /**
     * Measures the banner and its children Views for the given space.
     *
     * DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
     * DPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPD
     * DP......                               cPD
     * DP...... TITLE----------------------- XcPD
     * DP.ICON. *****                         cPD
     * DP...... LOGO                    BUTTONcPD
     * DP...... cccccccccccccccccccccccccccccccPD
     * DPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPD
     * DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
     *
     * The three paddings mentioned in the class Javadoc are denoted by:
     * D) Drop shadow padding.
     * P) Inner card padding.
     * c) Control padding.
     *
     * Measurement for components of the banner are performed assuming that components are laid out
     * inside of the banner's background as follows:
     * 1) A maximum width is enforced on the banner to keep the whole thing on screen and keep it a
     *    reasonable size.
     * 2) The icon takes up the left side of the banner.
     * 3) The install button occupies the bottom-right of the banner.
     * 4) The Google Play logo occupies the space to the left of the button.
     * 5) The rating is assigned space above the logo and below the title.
     * 6) The close button (if visible) sits in the top right of the banner.
     * 7) The title is assigned whatever space is left and sits on top of the tallest stack of
     *    controls.
     *
     * See {@link #android.view.View.onMeasure(int, int)} for the parameters.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Enforce a maximum width on the banner, which is defined as the smallest of:
        // 1) The smallest width for the device (in either landscape or portrait mode).
        // 2) The defined maximum width in the dimens.xml files.
        // 3) The width passed in through the MeasureSpec.
        Resources res = getResources();
        float density = res.getDisplayMetrics().density;
        int screenSmallestWidth = (int) (res.getConfiguration().smallestScreenWidthDp * density);
        int specWidth = MeasureSpec.getSize(widthMeasureSpec);
        int bannerWidth = Math.min(Math.min(specWidth, mDefinedMaxWidth), screenSmallestWidth);

        // Track how much space is available inside the banner's card-shaped background Drawable.
        // To calculate this, we need to account for both the padding of the background (which
        // is occupied by the card's drop shadows) as well as the padding defined on the inside of
        // the card.
        int bgPaddingWidth = mBackgroundDrawablePadding.left + mBackgroundDrawablePadding.right;
        int bgPaddingHeight = mBackgroundDrawablePadding.top + mBackgroundDrawablePadding.bottom;
        final int maxControlWidth = bannerWidth - bgPaddingWidth - (mPaddingCard * 2);

        // Control height is constrained to provide a reasonable aspect ratio.
        // In practice, the only controls which can cause an issue are the title and the install
        // button, since they have strings that can change size according to user preference.  The
        // other controls are all defined to be a certain height.
        int specHeight = MeasureSpec.getSize(heightMeasureSpec);
        int reasonableHeight = maxControlWidth / 4;
        int paddingHeight = bgPaddingHeight + (mPaddingCard * 2);
        final int maxControlHeight = Math.min(specHeight, reasonableHeight) - paddingHeight;
        final int maxStackedControlHeight = maxControlWidth / 3;

        // Determine how big each component wants to be.  The icon is measured separately because
        // it is not stacked with the other controls.
        measureChildForSpace(mIconView, maxControlWidth, maxControlHeight);
        for (int i = 0; i < getChildCount(); i++) {
            if (getChildAt(i) != mIconView) {
                measureChildForSpace(getChildAt(i), maxControlWidth, maxStackedControlHeight);
            }
        }

        // Determine how tall the banner needs to be to fit everything by calculating the combined
        // height of the stacked controls.  There are three competing stacks to measure:
        // 1) The icon.
        // 2) The app title + control padding + star rating + store logo.
        // 3) The app title + control padding + install button.
        // The control padding is extra padding that applies only to the non-icon widgets.
        // The close button does not get counted as part of a stack.
        int iconStackHeight = getHeightWithMargins(mIconView);
        int logoStackHeight = getHeightWithMargins(mTitleView) + mPaddingControls
                + getHeightWithMargins(mRatingView) + getHeightWithMargins(mLogoView);
        int buttonStackHeight = getHeightWithMargins(mTitleView) + mPaddingControls
                + getHeightWithMargins(mInstallButtonView);
        int biggestStackHeight =
                Math.max(iconStackHeight, Math.max(logoStackHeight, buttonStackHeight));

        // The icon hugs the banner's starting edge, from the top of the banner to the bottom.
        final int iconSize = biggestStackHeight;
        measureChildForSpaceExactly(mIconView, iconSize, iconSize);

        // The rest of the content is laid out to the right of the icon.
        // Additional padding is defined for non-icon content on the end and bottom.
        final int contentWidth =
                maxControlWidth - getWidthWithMargins(mIconView) - mPaddingControls;
        final int contentHeight = biggestStackHeight - mPaddingControls;
        measureChildForSpace(mLogoView, contentWidth, contentHeight);

        // Restrict the button size to prevent overrunning the Google Play logo.
        int remainingButtonWidth =
                maxControlWidth - getWidthWithMargins(mLogoView) - getWidthWithMargins(mIconView);
        mInstallButtonView.setMaxWidth(remainingButtonWidth);
        measureChildForSpace(mInstallButtonView, contentWidth, contentHeight);

        // Measure the star rating, which sits below the title and above the logo.
        final int ratingWidth = contentWidth;
        final int ratingHeight = contentHeight - getHeightWithMargins(mLogoView);
        measureChildForSpace(mRatingView, ratingWidth, ratingHeight);

        // The close button sits to the right of the title and above the install button.
        final int closeWidth = contentWidth;
        final int closeHeight = contentHeight - getHeightWithMargins(mInstallButtonView);
        measureChildForSpace(mCloseButtonView, closeWidth, closeHeight);

        // The app title spans the top of the banner and sits on top of the other controls, and to
        // the left of the close button. The computation for the width available to the title is
        // complicated by how the button sits in the corner and absorbs the padding that would
        // normally be there.
        int biggerStack = Math.max(getHeightWithMargins(mInstallButtonView),
                getHeightWithMargins(mLogoView) + getHeightWithMargins(mRatingView));
        final int titleWidth = contentWidth - getWidthWithMargins(mCloseButtonView) + mPaddingCard;
        final int titleHeight = contentHeight - biggerStack;
        measureChildForSpace(mTitleView, titleWidth, titleHeight);

        // Set the measured dimensions for the banner.  The banner's height is defined by the
        // tallest stack of components, the padding of the banner's card background, and the extra
        // padding around the banner's components.
        int bannerPadding = mBackgroundDrawablePadding.top + mBackgroundDrawablePadding.bottom
                + (mPaddingCard * 2);
        int bannerHeight = biggestStackHeight + bannerPadding;
        setMeasuredDimension(bannerWidth, bannerHeight);

        // Make the banner highlight view be the exact same size as the banner's card background.
        final int cardWidth = bannerWidth - bgPaddingWidth;
        final int cardHeight = bannerHeight - bgPaddingHeight;
        measureChildForSpaceExactly(mBannerHighlightView, cardWidth, cardHeight);
    }

    /**
     * Lays out the controls according to the algorithm in {@link #onMeasure}.
     * See {@link #android.view.View.onLayout(boolean, int, int, int, int)} for the parameters.
     */
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        int top = mBackgroundDrawablePadding.top;
        int bottom = getMeasuredHeight() - mBackgroundDrawablePadding.bottom;
        int start = mBackgroundDrawablePadding.left;
        int end = getMeasuredWidth() - mBackgroundDrawablePadding.right;

        // The highlight overlay covers the entire banner (minus drop shadow padding).
        mBannerHighlightView.layout(start, top, end, bottom);

        // Lay out the close button in the top-right corner.  Padding that would normally go to the
        // card is applied to the close button so that it has a bigger touch target.
        if (mCloseButtonView.getVisibility() == VISIBLE) {
            int closeWidth = mCloseButtonView.getMeasuredWidth();
            int closeTop =
                    top + ((MarginLayoutParams) mCloseButtonView.getLayoutParams()).topMargin;
            int closeBottom = closeTop + mCloseButtonView.getMeasuredHeight();
            int closeRight = mIsLayoutLTR ? end : (getMeasuredWidth() - end + closeWidth);
            int closeLeft = closeRight - closeWidth;
            mCloseButtonView.layout(closeLeft, closeTop, closeRight, closeBottom);
        }

        // Apply the padding for the rest of the widgets.
        top += mPaddingCard;
        bottom -= mPaddingCard;
        start += mPaddingCard;
        end -= mPaddingCard;

        // Lay out the icon.
        int iconWidth = mIconView.getMeasuredWidth();
        int iconLeft = mIsLayoutLTR ? start : (getMeasuredWidth() - start - iconWidth);
        mIconView.layout(iconLeft, top, iconLeft + iconWidth, top + mIconView.getMeasuredHeight());
        start += getWidthWithMargins(mIconView);

        // Factor in the additional padding, which is only tacked onto the end and bottom.
        end -= mPaddingControls;
        bottom -= mPaddingControls;

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
        int buttonHeight = mInstallButtonView.getMeasuredHeight();
        int buttonWidth = mInstallButtonView.getMeasuredWidth();
        int buttonRight = mIsLayoutLTR ? end : (getMeasuredWidth() - end + buttonWidth);
        int buttonLeft = buttonRight - buttonWidth;
        mInstallButtonView.layout(buttonLeft, bottom - buttonHeight, buttonRight, bottom);
    }

    /**
     * Measures a child for the given space, accounting for defined heights and margins.
     * @param child           View to measure.
     * @param availableWidth  Available width for the view.
     * @param availableHeight Available height for the view.
     */
    private void measureChildForSpace(View child, int availableWidth, int availableHeight) {
        // Handle margins.
        availableWidth -= getMarginWidth(child);
        availableHeight -= getMarginHeight(child);

        // Account for any layout-defined dimensions for the view.
        int childWidth = child.getLayoutParams().width;
        int childHeight = child.getLayoutParams().height;
        if (childWidth >= 0) availableWidth = Math.min(availableWidth, childWidth);
        if (childHeight >= 0) availableHeight = Math.min(availableHeight, childHeight);

        int widthSpec = MeasureSpec.makeMeasureSpec(availableWidth, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(availableHeight, MeasureSpec.AT_MOST);
        child.measure(widthSpec, heightSpec);
    }

    /**
     * Forces a child to exactly occupy the given space.
     * @param child           View to measure.
     * @param availableWidth  Available width for the view.
     * @param availableHeight Available height for the view.
     */
    private void measureChildForSpaceExactly(View child, int availableWidth, int availableHeight) {
        int widthSpec = MeasureSpec.makeMeasureSpec(availableWidth, MeasureSpec.EXACTLY);
        int heightSpec = MeasureSpec.makeMeasureSpec(availableHeight, MeasureSpec.EXACTLY);
        child.measure(widthSpec, heightSpec);
    }

    /**
     * Calculates how wide the margins are for the given View.
     * @param view View to measure.
     * @return     Measured width of the margins.
     */
    private static int getMarginWidth(View view) {
        MarginLayoutParams params = (MarginLayoutParams) view.getLayoutParams();
        return params.leftMargin + params.rightMargin;
    }

    /**
     * Calculates how wide the given View has been measured to be, including its margins.
     * @param view View to measure.
     * @return     Measured width of the view plus its margins.
     */
    private static int getWidthWithMargins(View view) {
        return view.getMeasuredWidth() + getMarginWidth(view);
    }

    /**
     * Calculates how tall the margins are for the given View.
     * @param view View to measure.
     * @return     Measured height of the margins.
     */
    private static int getMarginHeight(View view) {
        MarginLayoutParams params = (MarginLayoutParams) view.getLayoutParams();
        return params.topMargin + params.bottomMargin;
    }

    /**
     * Calculates how tall the given View has been measured to be, including its margins.
     * @param view View to measure.
     * @return     Measured height of the view plus its margins.
     */
    private static int getHeightWithMargins(View view) {
        return view.getMeasuredHeight() + getMarginHeight(view);
    }
}
