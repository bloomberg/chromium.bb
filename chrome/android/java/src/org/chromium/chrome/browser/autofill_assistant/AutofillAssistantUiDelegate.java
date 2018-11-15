// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.media.ThumbnailUtils;
import android.support.annotation.ColorInt;
import android.support.annotation.Nullable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.support.v4.text.TextUtilsCompat;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.json.JSONObject;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill_assistant.ui.BottomBarAnimations;
import org.chromium.chrome.browser.autofill_assistant.ui.TouchEventFilter;
import org.chromium.chrome.browser.cached_image_fetcher.CachedImageFetcher;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentOptions;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Delegate to interact with the assistant UI. */
class AutofillAssistantUiDelegate {
    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";
    private static final int PROGRESS_BAR_INITIAL_PROGRESS = 10;
    private static final int DETAILS_PULSING_DURATION_MS = 1_000;

    /** How long the snackbars created by {@link #showAutofillAssistantStoppedSnackbar} stay up. */
    static final int SNACKBAR_DELAY_MS = 5_000;

    // TODO(crbug.com/806868): Use correct user locale and remove suppressions.
    @SuppressLint("ConstantLocale")
    private static final SimpleDateFormat sDetailsTimeFormat =
            new SimpleDateFormat("H:mma", Locale.getDefault());
    @SuppressLint("ConstantLocale")
    private static final SimpleDateFormat sDetailsDateFormat =
            new SimpleDateFormat("EEE, MMM d", Locale.getDefault());

    private final ChromeActivity mActivity;
    private final Client mClient;
    private final ViewGroup mCoordinatorView;
    private final View mFullContainer;
    private final View mOverlay;
    private final TouchEventFilter mTouchEventFilter;
    private final LinearLayout mBottomBar;
    private final HorizontalScrollView mCarouselScroll;
    private final ViewGroup mChipsViewContainer;
    private final TextView mStatusMessageView;
    private final AnimatedProgressBar mProgressBar;

    private final ViewGroup mDetailsViewContainer;
    private final ImageView mDetailsImage;
    private final TextView mDetailsTitle;
    private final TextView mDetailsText;
    private final int mDetailsImageWidth;
    private final int mDetailsImageHeight;

    private final BottomBarAnimations mBottomBarAnimations;
    private final boolean mIsRightToLeftLayout;
    private ValueAnimator mDetailsPulseAnimation;

    private AutofillAssistantPaymentRequest mPaymentRequest;

    /**
     * This is a client interface that relays interactions from the UI.
     */
    public interface Client extends TouchEventFilter.Client {
        /**
         * Called when clicking on the overlay.
         */
        void onClickOverlay();

        /**
         * Called when the bottom bar is dismissing.
         */
        void onDismiss();

        /**
         * Called when a script has been selected.
         *
         * @param scriptPath The path for the selected script.
         */
        void onScriptSelected(String scriptPath);

        /**
         * Called when an address has been selected.
         *
         * @param guid The GUID of the selected address.
         */
        void onAddressSelected(String guid);

        /**
         * Called when a credit card has been selected.
         *
         * @param guid The GUID of the selected card.
         */
        void onCardSelected(String guid);

        /**
         * Returns current details.
         */
        Details getDetails();

        /**
         * Returns current status message.
         */
        String getStatusMessage();

        /**
         * Used to access information relevant for debugging purposes.
         *
         * @return A string describing the current execution context.
         */
        String getDebugContext();

        /**
         * Called when the init was successful.
         */
        void onInitOk();
    }

    /**
     * Java side equivalent of autofill_assistant::ScriptHandle.
     */
    protected static class ScriptHandle {
        /** The display name of this script. */
        private final String mName;
        /** The script path. */
        private final String mPath;
        /** Whether the script should be highlighted. */
        private final boolean mHighlight;

        /** Constructor. */
        public ScriptHandle(String name, String path, boolean highlight) {
            mName = name;
            mPath = path;
            mHighlight = highlight;
        }

        /** Returns the display name. */
        public String getName() {
            return mName;
        }

        /** Returns the script path. */
        public String getPath() {
            return mPath;
        }

        /** Returns whether the script should be highlighted. */
        public boolean isHighlight() {
            return mHighlight;
        }
    }

    /**
     * Java side equivalent of autofill_assistant::DetailsProto.
     */
    static class Details {
        private final String mTitle;
        private final String mUrl;
        @Nullable
        private final Date mDate;
        private final String mDescription;
        private final boolean mIsFinal;

        private static final Details EMPTY_DETAILS = new Details("", "", null, "", false);

        public Details(String title, String url, @Nullable Date date, String description,
                boolean isFinal) {
            this.mTitle = title;
            this.mUrl = url;
            this.mDate = date;
            this.mDescription = description;
            this.mIsFinal = isFinal;
        }

        String getTitle() {
            return mTitle;
        }

        String getUrl() {
            return mUrl;
        }

        @Nullable
        Date getDate() {
            return mDate;
        }

        String getDescription() {
            return mDescription;
        }

        JSONObject toJSONObject() {
            // Details are part of the feedback form, hence they need a JSON representation.
            Map<String, String> movieDetails = new HashMap<>();
            movieDetails.put("title", mTitle);
            movieDetails.put("url", mUrl);
            if (mDate != null) movieDetails.put("date", mDate.toString());
            movieDetails.put("description", mDescription);
            return new JSONObject(movieDetails);
        }

        /**
         * Whether the details are not subject to change anymore. If set to false the animated
         * placeholders will be displayed in place of missing data.
         */
        boolean isFinal() {
            return mIsFinal;
        }

        boolean isEmpty() {
            return mTitle.isEmpty() && mUrl.isEmpty() && mDescription.isEmpty() && mDate == null;
        }

        /**
         * Returns true  {@code details} are similar to {@code this}. In order for details to be
         * similar the conditions apply:
         *
         * <p>
         * <ul>
         *   <li> Same date.
         *   <li> TODO(crbug.com/806868): 60% of characters match within title.
         * </ul>
         */
        boolean isSimilarTo(AutofillAssistantUiDelegate.Details details) {
            if (this.isEmpty() || details.isEmpty()) {
                return true;
            }

            return this.getDate().equals(details.getDate());
        }

        static Details getEmptyDetails() {
            return EMPTY_DETAILS;
        }

        /**
         * Merges {@code oldDetails} with the {@code newDetails} filling the missing fields. The
         * distinction is important, as the fields from old version take precedence, with the
         * exception of isFinal field.
         */
        static Details merge(Details oldDetails, Details newDetails) {
            String title =
                    oldDetails.getTitle().isEmpty() ? newDetails.getTitle() : oldDetails.getTitle();
            String url = oldDetails.getUrl().isEmpty() ? newDetails.getUrl() : oldDetails.getUrl();
            Date date = oldDetails.getDate() == null ? newDetails.getDate() : oldDetails.getDate();
            String description = oldDetails.getDescription().isEmpty()
                    ? newDetails.getDescription()
                    : oldDetails.getDescription();
            boolean isFinal = newDetails.isFinal();
            return new Details(title, url, date, description, isFinal);
        }
    }

    // Names borrowed from :
    // - https://guidelines.googleplex.com/googlematerial/components/chips.html
    // - https://guidelines.googleplex.com/googlematerial/components/buttons.html
    private enum ChipStyle { CHIP_ASSISTIVE, BUTTON_FILLED, BUTTON_HAIRLINE }

    /**
     * Constructs an assistant UI delegate.
     *
     * @param activity The ChromeActivity
     * @param client The client to forward events to
     */
    public AutofillAssistantUiDelegate(ChromeActivity activity, Client client) {
        mActivity = activity;
        mClient = client;

        mCoordinatorView = (ViewGroup) mActivity.findViewById(R.id.coordinator);
        mFullContainer = LayoutInflater.from(mActivity)
                                 .inflate(R.layout.autofill_assistant_sheet, mCoordinatorView)
                                 .findViewById(R.id.autofill_assistant);
        // TODO(crbug.com/806868): Set hint text on overlay.
        mOverlay = mFullContainer.findViewById(R.id.overlay);
        mOverlay.setOnClickListener(unusedView -> mClient.onClickOverlay());
        mTouchEventFilter = (TouchEventFilter) mFullContainer.findViewById(R.id.touch_event_filter);
        mTouchEventFilter.init(client, activity.getFullscreenManager(),
                activity.getActivityTab().getWebContents());
        mBottomBar = mFullContainer.findViewById(R.id.bottombar);
        mBottomBar.findViewById(R.id.close_button)
                .setOnClickListener(unusedView -> mClient.onDismiss());
        mBottomBar.findViewById(R.id.feedback_button)
                .setOnClickListener(unusedView
                        -> HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity,
                                Profile.getLastUsedProfile(), mActivity.getActivityTab().getUrl(),
                                FEEDBACK_CATEGORY_TAG,
                                FeedbackContext.buildContextString(mActivity, mClient,
                                        client.getDetails(), client.getStatusMessage(), 4)));
        mCarouselScroll = mBottomBar.findViewById(R.id.carousel_scroll);
        mChipsViewContainer = mCarouselScroll.findViewById(R.id.carousel);
        mStatusMessageView = mBottomBar.findViewById(R.id.status_message);
        mProgressBar = new AnimatedProgressBar(mBottomBar.findViewById(R.id.progress_bar),
                ApiCompatibilityUtils.getColor(mActivity.getResources(), R.color.modern_blue_600),
                ApiCompatibilityUtils.getColor(
                        mActivity.getResources(), R.color.modern_blue_600_alpha_38_opaque));

        mDetailsViewContainer = (ViewGroup) mBottomBar.findViewById(R.id.details);
        mDetailsImage = mDetailsViewContainer.findViewById(R.id.details_image);
        mDetailsTitle = (TextView) mDetailsViewContainer.findViewById(R.id.details_title);
        mDetailsText = (TextView) mDetailsViewContainer.findViewById(R.id.details_text);
        mDetailsImageWidth = mActivity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mDetailsImageHeight = mActivity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);

        mBottomBarAnimations = new BottomBarAnimations(mBottomBar, mDetailsViewContainer,
                mChipsViewContainer, mActivity.getResources().getDisplayMetrics());
        mIsRightToLeftLayout = TextUtilsCompat.getLayoutDirectionFromLocale(Locale.getDefault())
                == ViewCompat.LAYOUT_DIRECTION_RTL;

        // Finch experiment to adjust overlay color
        Integer overlayColor = AutofillAssistantStudy.getOverlayColor();
        if (overlayColor != null) {
            mOverlay.setBackgroundColor(overlayColor);
            mTouchEventFilter.setGrayOutColor(overlayColor);
        }

        // TODO(crbug.com/806868): Listen for contextual search shown so as to hide this UI.
    }

    /**
     * Shows a message in the status bar.
     *
     * @param message Message to display.
     */
    public void showStatusMessage(@Nullable String message) {
        show();
        mStatusMessageView.setText(message);
    }

    /**
     * Updates the list of scripts in the bar.
     *
     * @param scriptHandles List of scripts to show.
     */
    public void updateScripts(ArrayList<ScriptHandle> scriptHandles) {
        if (scriptHandles.isEmpty()) {
            clearCarousel();
            return;
        }

        boolean alignRight = hasHighlightedScript(scriptHandles);
        ChipStyle nonHighlightStyle =
                alignRight ? ChipStyle.BUTTON_HAIRLINE : ChipStyle.CHIP_ASSISTIVE;
        ArrayList<View> childViews = new ArrayList<>();
        for (int i = 0; i < scriptHandles.size(); i++) {
            ScriptHandle scriptHandle = scriptHandles.get(i);
            ChipStyle chipStyle =
                    scriptHandle.isHighlight() ? ChipStyle.BUTTON_FILLED : nonHighlightStyle;
            TextView chipView = createChipView(scriptHandle.getName(), chipStyle);
            chipView.setOnClickListener((unusedView) -> {
                clearCarousel();
                mClient.onScriptSelected(scriptHandle.getPath());
            });
            childViews.add(chipView);
        }
        setCarouselChildViews(childViews, alignRight);
    }

    private boolean hasHighlightedScript(ArrayList<ScriptHandle> scripts) {
        for (int i = 0; i < scripts.size(); i++) {
            if (scripts.get(i).isHighlight()) {
                return true;
            }
        }
        return false;
    }

    private void clearCarousel() {
        setCarouselChildViews(Collections.emptyList(), /* alignRight= */ false);
    }

    private void setCarouselChildViews(List<View> children, boolean alignRight) {
        // TODO(crbug.com/806868): Pull the carousel logic into its own MVC component.

        // Reverse alignRight if we are in a RTL layout.
        alignRight = mIsRightToLeftLayout ? !alignRight : alignRight;

        // Replace children.
        // TODO(crbug.com/806868): We might want to animate children change using fade in/out
        // animations.
        mChipsViewContainer.removeAllViews();
        setCarouselAlignment(alignRight);
        for (int i = 0; i < children.size(); i++) {
            // Add children in reverse order if the chips are right aligned.
            int j = alignRight ? children.size() - i - 1 : i;
            View child = children.get(j);
            if (i > 0) {
                LinearLayout.LayoutParams layoutParams =
                        (LinearLayout.LayoutParams) child.getLayoutParams();
                int leftMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 16,
                        child.getContext().getResources().getDisplayMetrics());
                layoutParams.setMargins(leftMargin, 0, 0, 0);
                child.setLayoutParams(layoutParams);
            }
            mChipsViewContainer.addView(child);
        }

        if (children.isEmpty()) {
            mBottomBarAnimations.hideCarousel();
        } else {
            // Make sure the Autofill Assistant is visible.
            show();
            mBottomBarAnimations.showCarousel();
        }
    }

    private void setCarouselAlignment(boolean alignRight) {
        // Set carousel scroll gravity.
        ViewGroup.LayoutParams currentLayoutParams = mCarouselScroll.getLayoutParams();
        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(currentLayoutParams);
        layoutParams.gravity = alignRight ? Gravity.END : Gravity.START;
        mCarouselScroll.setLayoutParams(layoutParams);

        // Reset the scroll position.
        mCarouselScroll.post(
                () -> mCarouselScroll.fullScroll(alignRight ? View.FOCUS_RIGHT : View.FOCUS_LEFT));
    }

    private TextView createChipView(String text, ChipStyle style) {
        int resId = -1;
        switch (style) {
            case CHIP_ASSISTIVE:
                resId = R.layout.autofill_assistant_chip_assistive;
                break;
            case BUTTON_FILLED:
                resId = R.layout.autofill_assistant_button_filled;
                break;
            case BUTTON_HAIRLINE:
                resId = R.layout.autofill_assistant_button_hairline;
                break;
        }

        TextView chipView = (TextView) (LayoutInflater.from(mActivity).inflate(
                resId, mChipsViewContainer, false));
        chipView.setText(text);
        return chipView;
    }

    public void show() {
        if (mFullContainer.getVisibility() != View.VISIBLE) {
            mFullContainer.setVisibility(View.VISIBLE);

            // Set the initial progress. It is OK to make multiple calls to this method as it will
            // have an effect only on the first one.
            mProgressBar.maybeIncreaseProgress(PROGRESS_BAR_INITIAL_PROGRESS);
        }
    }

    public void hide() {
        mFullContainer.setVisibility(View.GONE);
    }

    public void showAutofillAssistantStoppedSnackbar(SnackbarManager.SnackbarController controller,
            int stringResourceId, Object... formatArgs) {
        Snackbar snackBar =
                Snackbar.make(mActivity.getString(stringResourceId, formatArgs), controller,
                                Snackbar.TYPE_ACTION, Snackbar.UMA_AUTOFILL_ASSISTANT_STOP_UNDO)
                        .setAction(mActivity.getString(R.string.undo), /* actionData= */ null);
        snackBar.setSingleLine(false);
        snackBar.setDuration(SNACKBAR_DELAY_MS);
        mActivity.getSnackbarManager().showSnackbar(snackBar);
    }

    public void dismissSnackbar(SnackbarManager.SnackbarController controller) {
        mActivity.getSnackbarManager().dismissSnackbars(controller);
    }

    /**
     * Enters a simplified pre-shutdown state, with only a goodbye message - the current status
     * message - and a close button.
     */
    public void enterGracefulShutdownMode() {
        // TODO(crbug.com/806868): Introduce a proper, separate shutdown dialog, with enough space
        // to display longer messages. Setting the max lines and hiding the feedback button are
        // hacks to give enough space to display long messages.
        mStatusMessageView.setMaxLines(4);
        mBottomBar.findViewById(R.id.feedback_button).setVisibility(View.GONE);
        hideOverlay();
        mTouchEventFilter.setVisibility(View.GONE);
        hideProgressBar();
        hideDetails();
        mBottomBarAnimations.hideCarousel();
    }

    /** Called to show overlay. */
    public void showOverlay() {
        mOverlay.setVisibility(View.VISIBLE);
    }

    /** Called to hide overlay. */
    public void hideOverlay() {
        mOverlay.setVisibility(View.GONE);
    }

    public boolean isOverlayVisible() {
        return mOverlay.getVisibility() == View.VISIBLE;
    }

    public void hideDetails() {
        mBottomBarAnimations.hideDetails();
    }

    /** Called to show a message in the status bar that autofill assistant is done. */
    public void showGiveUpMessage() {
        showStatusMessage(mActivity.getString(R.string.autofill_assistant_give_up));
    }

    /** Called to show contextual information. */
    public void showDetails(Details details) {
        Drawable defaultImage = AppCompatResources.getDrawable(
                mActivity, R.drawable.autofill_assistant_default_details);

        updateDetailsAnimation(details, (GradientDrawable) defaultImage);

        mDetailsTitle.setText(details.getTitle());
        String detailsText = getDetailsText(details);
        mDetailsText.setText(detailsText);

        String url = details.getUrl();
        if (!url.isEmpty()) {
            // The URL is safe given because it comes from the knowledge graph and is hosted on
            // Google servers.
            CachedImageFetcher.getInstance().fetchImage(url, image -> {
                if (image != null) {
                    mDetailsImage.setImageDrawable(getRoundedImage(image));
                    mDetailsImage.setVisibility(View.VISIBLE);
                } else {
                    mDetailsImage.setVisibility(View.GONE);
                }
            });
        } else {
            mDetailsImage.setVisibility(View.GONE);
            if (!details.isFinal()) {
                mDetailsImage.setImageDrawable(defaultImage);
                mDetailsImage.setVisibility(View.VISIBLE);
            }
        }

        // Make sure the Autofill Assistant is visible.
        show();
        mBottomBarAnimations.showDetails();
    }

    private void updateDetailsAnimation(Details details, GradientDrawable defaultImage) {
        if (details.isFinal()) {
            if (mDetailsPulseAnimation != null) {
                mDetailsPulseAnimation.cancel();
            }
            return;
        } else {
            @ColorInt
            int startColor = ApiCompatibilityUtils.getColor(
                    mActivity.getResources(), R.color.modern_grey_100);
            @ColorInt
            int endColor = ApiCompatibilityUtils.getColor(
                    mActivity.getResources(), R.color.modern_grey_50);

            mDetailsPulseAnimation = ValueAnimator.ofInt(startColor, endColor);
            mDetailsPulseAnimation.setDuration(DETAILS_PULSING_DURATION_MS);
            mDetailsPulseAnimation.setEvaluator(new ArgbEvaluator());
            mDetailsPulseAnimation.setRepeatCount(ValueAnimator.INFINITE);
            mDetailsPulseAnimation.setRepeatMode(ValueAnimator.REVERSE);
            mDetailsPulseAnimation.setInterpolator(ChromeAnimation.getAccelerateInterpolator());
            mDetailsPulseAnimation.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationCancel(Animator animation) {
                    mDetailsTitle.setBackgroundColor(Color.WHITE);
                    mDetailsText.setBackgroundColor(Color.WHITE);
                }
            });
            mDetailsPulseAnimation.addUpdateListener(animation -> {
                if (details.getTitle().isEmpty()) {
                    mDetailsTitle.setBackgroundColor((int) animation.getAnimatedValue());
                }
                if (getDetailsText(details).isEmpty()) {
                    mDetailsText.setBackgroundColor((int) animation.getAnimatedValue());
                }
                defaultImage.setColor((int) animation.getAnimatedValue());
            });
            mDetailsPulseAnimation.start();
        }
    }

    private String getDetailsText(Details details) {
        List<String> parts = new ArrayList<>();
        Date date = details.getDate();
        if (date != null) {
            parts.add(sDetailsTimeFormat.format(date).toLowerCase(Locale.getDefault()));
            parts.add(sDetailsDateFormat.format(date));
        }

        String description = details.getDescription();
        if (description != null && !description.isEmpty()) {
            parts.add(description);
        }

        // TODO(crbug.com/806868): Use a view instead of this dot text.
        return TextUtils.join(" • ", parts);
    }

    private Drawable getRoundedImage(Bitmap bitmap) {
        RoundedBitmapDrawable roundedBitmap = RoundedBitmapDrawableFactory.create(
                mActivity.getResources(),
                ThumbnailUtils.extractThumbnail(bitmap, mDetailsImageWidth, mDetailsImageHeight));
        roundedBitmap.setCornerRadius(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 4, mActivity.getResources().getDisplayMetrics()));
        return roundedBitmap;
    }

    public void showProgressBar(int progress, String message) {
        show();
        mProgressBar.show();
        mProgressBar.maybeIncreaseProgress(progress);

        if (!message.isEmpty()) {
            mStatusMessageView.setText(message);
        }
    }

    public void hideProgressBar() {
        mProgressBar.hide();
    }

    public void enableProgressBarPulsing() {
        mProgressBar.enablePulsing();
    }

    public void disableProgressBarPulsing() {
        mProgressBar.disablePulsing();
    }

    public void updateTouchableArea(boolean enabled, List<RectF> boxes) {
        mTouchEventFilter.updateTouchableArea(enabled, boxes);
    }

    /**
     * Show profiles in the bar.
     *
     * @param profiles List of profiles to show.
     */
    public void showProfiles(ArrayList<AutofillProfile> profiles) {
        if (profiles.isEmpty()) {
            clearCarousel();
            mClient.onAddressSelected("");
            return;
        }

        ArrayList<View> childViews = new ArrayList<>();
        for (int i = 0; i < profiles.size(); i++) {
            AutofillProfile profile = profiles.get(i);
            // TODO(crbug.com/806868): Show more information than the street.
            TextView chipView =
                    createChipView(profile.getStreetAddress(), ChipStyle.CHIP_ASSISTIVE);
            chipView.setOnClickListener((unusedView) -> {
                clearCarousel();
                mClient.onAddressSelected(profile.getGUID());
            });
            childViews.add(chipView);
        }
        setCarouselChildViews(childViews, /* alignRight= */ false);
    }

    /**
     * Show credit cards in the bar.
     *
     * @param cards List of cards to show.
     */
    public void showCards(ArrayList<CreditCard> cards) {
        if (cards.isEmpty()) {
            mClient.onCardSelected("");
            return;
        }

        ArrayList<View> childViews = new ArrayList<>();
        for (int i = 0; i < cards.size(); i++) {
            CreditCard card = cards.get(i);
            // TODO(crbug.com/806868): Show more information than the card number.
            TextView chipView =
                    createChipView(card.getObfuscatedNumber(), ChipStyle.CHIP_ASSISTIVE);
            chipView.setOnClickListener((unusedView) -> {
                clearCarousel();
                mClient.onCardSelected(card.getGUID());
            });
            childViews.add(chipView);
        }
        setCarouselChildViews(childViews, /* alignRight= */ false);
    }

    /**
     * Starts the init screen unless it has been marked to be skipped.
     */
    public void startOrSkipInitScreen() {
        if (InitScreenController.skip()) {
            mClient.onInitOk();
            return;
        }
        showInitScreen(new InitScreenController(mClient));
    }

    /**
     * Shows the init screen and launch the autofill assistant when it succeeds.
     */
    public void showInitScreen(InitScreenController controller) {
        View initView = LayoutInflater.from(mActivity)
                                .inflate(R.layout.init_screen, mCoordinatorView)
                                .findViewById(R.id.init_screen);

        initView.findViewById(R.id.close_button)
                .setOnClickListener(unusedView -> onInitClicked(controller, false, initView));
        initView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView -> onInitClicked(controller, true, initView));
        initView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView -> onInitClicked(controller, false, initView));
    }

    private void onInitClicked(InitScreenController controller, Boolean initOk, View initView) {
        CheckBox checkBox = initView.findViewById(R.id.checkbox_dont_show_init_again);
        controller.onInitFinished(initOk, checkBox.isChecked());
        mCoordinatorView.removeView(initView);
    }

    /**
     * Show the payment request UI.
     *
     * Show the UI and return the selected information via |callback| when done.
     *
     * @param webContents The webContents.
     * @param paymentOptions Options to request payment information.
     * @param title Unused title.
     * @param supportedBasicCardNetworks Optional array of supported basic card networks.
     * @param callback Callback to return selected info.
     */
    public void showPaymentRequest(WebContents webContents, PaymentOptions paymentOptions,
            String unusedTitle, String[] supportedBasicCardNetworks,
            Callback<AutofillAssistantPaymentRequest.SelectedPaymentInformation> callback) {
        assert mPaymentRequest == null;
        mPaymentRequest = new AutofillAssistantPaymentRequest(
                webContents, paymentOptions, unusedTitle, supportedBasicCardNetworks);
        // Make sure we wrap content in the container.
        mBottomBarAnimations.setBottomBarHeightToWrapContent();
        mPaymentRequest.show(mCarouselScroll, callback);
    }

    /** Close and destroy the payment request UI. */
    public void closePaymentRequest() {
        mPaymentRequest.close();
        mPaymentRequest = null;
        mBottomBarAnimations.setBottomBarHeightToFixed();
    }
}
