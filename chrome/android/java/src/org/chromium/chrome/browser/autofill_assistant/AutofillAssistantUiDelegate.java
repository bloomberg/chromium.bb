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
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.media.ThumbnailUtils;
import android.support.annotation.ColorInt;
import android.support.annotation.IntDef;
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
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentOptions;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/** Delegate to interact with the assistant UI. */
class AutofillAssistantUiDelegate {
    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";
    private static final int PROGRESS_BAR_INITIAL_PROGRESS = 10;
    private static final int DETAILS_PULSING_DURATION_MS = 1_000;

    /** How long the snackbars created by {@link #showAutofillAssistantStoppedSnackbar} stay up. */
    static final int SNACKBAR_DELAY_MS = 5_000;

    @SuppressLint("ConstantLocale")
    private static final SimpleDateFormat sDetailsTimeFormat;
    static {
        DateFormat df = DateFormat.getTimeInstance(DateFormat.SHORT, Locale.getDefault());
        String timeFormatPattern =
                (df instanceof SimpleDateFormat) ? ((SimpleDateFormat) df).toPattern() : "H:mm";
        sDetailsTimeFormat = new SimpleDateFormat(timeFormatPattern, Locale.getDefault());
    }

    @SuppressLint("ConstantLocale")
    private static final SimpleDateFormat sDetailsDateFormat =
            new SimpleDateFormat("EEE, MMM d", Locale.getDefault());

    private final ChromeActivity mActivity;
    private final Client mClient;
    private final ViewGroup mCoordinatorView;
    private final View mFullContainer;
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
         * Called when the bottom bar is dismissing.
         */
        void onDismiss();

        /**
         * Called when the user chose not to use the assistant from the
         * onboarding screen.
         */
        void onInitRejected();

        /**
         * Called when a script has been selected.
         *
         * @param scriptPath The path for the selected script.
         */
        void onScriptSelected(String scriptPath);

        /** Called when a choice has been selected, with the result of {@link Choice#getData()}. */
        void onChoice(byte[] serverPayload);

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
         * Called when button for acknowledging Details differ was clicked.
         *
         * @param displayedDetails Details that were shown.
         * @param canContinue Whether the permission to continue was granted.
         */
        void onDetailsAcknowledged(Details displayedDetails, boolean canContinue);

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

    /** Describes a chip to display. */
    static class Chip {
        private final String mName;
        private final boolean mHighlight;

        /** Returns the localized name to display. */
        String getName() {
            return mName;
        }

        /** Returns {@code true} if the choice should be highlight. */
        boolean isHighlight() {
            return mHighlight;
        }

        Chip(String name, boolean highlight) {
            mName = name;
            mHighlight = highlight;
        }
    }

    /** Functional interface that acts on a chip. */
    interface ChipAction<T extends Chip> {
        void apply(T chip);
    }

    /** Java side equivalent of autofill_assistant::ScriptHandle. */
    static class ScriptHandle extends Chip {
        private final String mPath;

        /** Constructor. */
        ScriptHandle(String name, boolean highlight, String path) {
            super(name, highlight);
            mPath = path;
        }

        /** Returns the script path. */
        String getPath() {
            return mPath;
        }
    }

    /** Java side equivalent of autofill_assistant::UiController::Choice. */
    static class Choice extends Chip {
        private final byte[] mServerPayload;

        /** Constructor. */
        Choice(String name, boolean highlight, byte[] serverPayload) {
            super(name, highlight);
            mServerPayload = serverPayload;
        }

        /** Returns the serverPayload that corresponds to that choice. */
        byte[] getServerPayload() {
            return mServerPayload;
        }
    }

    // Names borrowed from :
    // - https://guidelines.googleplex.com/googlematerial/components/chips.html
    // - https://guidelines.googleplex.com/googlematerial/components/buttons.html
    @IntDef({ChipStyle.CHIP_ASSISTIVE, ChipStyle.BUTTON_FILLED, ChipStyle.BUTTON_HAIRLINE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ChipStyle {
        int CHIP_ASSISTIVE = 0;
        int BUTTON_FILLED = 1;
        int BUTTON_HAIRLINE = 2;
    }

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
        mTouchEventFilter = (TouchEventFilter) mFullContainer.findViewById(R.id.touch_event_filter);
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
    public void updateScripts(List<ScriptHandle> scriptHandles) {
        if (scriptHandles.isEmpty()) {
            clearCarousel();
            return;
        }

        addChips(scriptHandles, scriptHandle -> {
            clearCarousel();
            mClient.onScriptSelected(scriptHandle.getPath());
        });
    }

    private <T extends Chip> void addChips(Iterable<T> chips, ChipAction<T> onClick) {
        boolean alignRight = hasHighlightedScript(chips);
        @ChipStyle
        int nonHighlightStyle = alignRight ? ChipStyle.BUTTON_HAIRLINE : ChipStyle.CHIP_ASSISTIVE;
        List<View> childViews = new ArrayList<>();
        for (T chip : chips) {
            @ChipStyle
            int chipStyle = chip.isHighlight() ? ChipStyle.BUTTON_FILLED : nonHighlightStyle;
            TextView chipView = createChipView(chip.getName(), chipStyle);
            chipView.setOnClickListener((unusedView) -> onClick.apply(chip));
            childViews.add(chipView);
        }
        setCarouselChildViews(childViews, alignRight);
    }

    private boolean hasHighlightedScript(Iterable<? extends Chip> chips) {
        for (Chip chip : chips) {
            if (chip.isHighlight()) {
                return true;
            }
        }
        return false;
    }

    public void clearCarousel() {
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

    private TextView createChipView(String text, @ChipStyle int style) {
        int resId = -1;
        switch (style) {
            case ChipStyle.CHIP_ASSISTIVE:
                resId = R.layout.autofill_assistant_chip_assistive;
                break;
            case ChipStyle.BUTTON_FILLED:
                resId = R.layout.autofill_assistant_button_filled;
                break;
            case ChipStyle.BUTTON_HAIRLINE:
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
            mTouchEventFilter.init(mClient, mActivity.getFullscreenManager(),
                    mActivity.getActivityTab().getWebContents());
            mFullContainer.setVisibility(View.VISIBLE);

            // Set the initial progress. It is OK to make multiple calls to this method as it will
            // have an effect only on the first one.
            mProgressBar.maybeIncreaseProgress(PROGRESS_BAR_INITIAL_PROGRESS);
        }
    }

    public void hide() {
        mTouchEventFilter.deInit();
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

    /**
     * Closes the activity.
     *
     * Note: The close() logic here assumes that |mActivity| is a CustomTabActivity since in the
     * more general case we probably just want to close the active tab instead of the entire Chrome
     * activity.
     */
    public void close() {
        assert mActivity instanceof CustomTabActivity;
        mActivity.finish();
    }

    /** Called to show overlay. */
    public void showOverlay() {
        mTouchEventFilter.setFullOverlay(true);
    }

    /** Called to hide overlay. */
    public void hideOverlay() {
        mTouchEventFilter.setFullOverlay(false);
    }

    public void hideDetails() {
        mBottomBarAnimations.hideDetails();
    }

    /** Called to show a message in the status bar that autofill assistant is done. */
    public void showGiveUpMessage() {
        showStatusMessage(mActivity.getString(R.string.autofill_assistant_give_up));
    }

    /**
     * Shows the details.
     *
     * <p>If some fields changed compared to the old details, the diff mode is entered.
     * */
    public void showDetails(Details details) {
        Drawable defaultImage = AppCompatResources.getDrawable(
                mActivity, R.drawable.autofill_assistant_default_details);

        updateDetailsAnimation(details, (GradientDrawable) defaultImage);

        mDetailsTitle.setText(details.getTitle());
        mDetailsText.setText(makeDetailsText(details));

        String url = details.getUrl();
        if (!url.isEmpty()) {
            // The URL is safe because it comes from the knowledge graph and is hosted on Google
            // servers.
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

        boolean shouldShowDiffMode = details.getFieldsChanged().size() > 0;
        if (shouldShowDiffMode) {
            enableDiffModeForDetails(details);
        } else {
            mClient.onDetailsAcknowledged(details, /* canContinue= */ true);
        }
    }

    /**
     * Shows additional UI elements asking to confirm details change.
     *
     * TODO(crbug.com/806868): Create own Controller for managing details state.
     */
    private void enableDiffModeForDetails(Details details) {
        enableProgressBarPulsing();
        // For detailsText we compare only Date.
        if (details.getFieldsChanged().contains(Details.DetailsField.DATE)) {
            mDetailsText.setTypeface(mDetailsText.getTypeface(), Typeface.BOLD_ITALIC);
        } else {
            mDetailsText.setTextColor(ApiCompatibilityUtils.getColor(
                    mActivity.getResources(), R.color.modern_grey_300));
        }

        if (!details.getFieldsChanged().contains(Details.DetailsField.TITLE)) {
            mDetailsTitle.setTextColor(ApiCompatibilityUtils.getColor(
                    mActivity.getResources(), R.color.modern_grey_300));
        }

        // Show new UI parts.
        String oldMessage = mClient.getStatusMessage();
        showStatusMessage(mActivity.getString(R.string.autofill_assistant_details_differ));

        ArrayList<View> childViews = new ArrayList<>();
        TextView continueChip = createChipView(
                mActivity.getString(R.string.continue_button), ChipStyle.BUTTON_FILLED);
        continueChip.setOnClickListener(unusedView -> {
            // Reset UI changes.
            ApiCompatibilityUtils.setTextAppearance(mDetailsTitle, R.style.BlackCaptionDefault);
            mDetailsTitle.setTypeface(mDetailsTitle.getTypeface(), Typeface.BOLD);
            ApiCompatibilityUtils.setTextAppearance(mDetailsText, R.style.BlackCaption);
            clearCarousel();
            showStatusMessage(oldMessage);
            disableProgressBarPulsing();

            mClient.onDetailsAcknowledged(details, /* canContinue= */ true);
        });
        childViews.add(continueChip);
        TextView cancelChip = createChipView(
                mActivity.getString(R.string.autofill_assistant_details_differ_go_back),
                ChipStyle.BUTTON_HAIRLINE);
        cancelChip.setOnClickListener(
                unusedView -> mClient.onDetailsAcknowledged(details, /* canContinue= */ false));
        childViews.add(cancelChip);
        setCarouselChildViews(childViews, /* alignRight= */ true);
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
                if (makeDetailsText(details).isEmpty()) {
                    mDetailsText.setBackgroundColor((int) animation.getAnimatedValue());
                }
                defaultImage.setColor((int) animation.getAnimatedValue());
            });
            mDetailsPulseAnimation.start();
        }
    }

    /** Creates text for display based on date and description.*/
    private String makeDetailsText(Details details) {
        List<String> parts = new ArrayList<>();

        Date date = details.getDate();
        if (date != null) {
            parts.add(sDetailsTimeFormat.format(date).toLowerCase(Locale.getDefault()));
            parts.add(sDetailsDateFormat.format(date));
        }

        if (details.getDescription() != null && !details.getDescription().isEmpty()) {
            parts.add(details.getDescription());
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
        mTouchEventFilter.setPartialOverlay(enabled, boxes);
    }

    /** Shows chip with the given choices. */
    public void showChoices(List<Choice> choices) {
        addChips(choices, choice -> {
            clearCarousel();
            mClient.onChoice(choice.getServerPayload());
        });
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
        if (AutofillAssistantPreferencesUtil.getSkipInitScreenPreference()) {
            mClient.onInitOk();
            return;
        }
        showInitScreen();
    }

    /**
     * Shows the init screen and launch the autofill assistant when it succeeds.
     */
    public void showInitScreen() {
        View initView = LayoutInflater.from(mActivity)
                                .inflate(R.layout.init_screen, mCoordinatorView)
                                .findViewById(R.id.init_screen);

        // Set default state to checked.
        ((CheckBox) initView.findViewById(R.id.checkbox_dont_show_init_again)).setChecked(true);
        initView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView -> onInitClicked(true, initView));
        initView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView -> onInitClicked(false, initView));
    }

    private void onInitClicked(boolean accept, View initView) {
        CheckBox checkBox = initView.findViewById(R.id.checkbox_dont_show_init_again);
        AutofillAssistantPreferencesUtil.setInitialPreferences(accept, checkBox.isChecked());
        mCoordinatorView.removeView(initView);
        if (accept)
            mClient.onInitOk();
        else
            mClient.onInitRejected();
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
        // Note: We show and hide (below) the carousel so that the margins are adjusted correctly.
        // This is an intermediate adjustment before the UI refactoring.
        mBottomBarAnimations.showCarousel();
        mPaymentRequest.show(mCarouselScroll, callback);
        enableProgressBarPulsing();
    }

    /** Close and destroy the payment request UI. */
    public void closePaymentRequest() {
        mPaymentRequest.close();
        mPaymentRequest = null;
        mBottomBarAnimations.setBottomBarHeightToFixed();
        mBottomBarAnimations.hideCarousel();
        disableProgressBarPulsing();
    }
}
