// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.media.ThumbnailUtils;
import android.os.AsyncTask;
import android.support.annotation.Nullable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.support.v7.widget.AppCompatImageView;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.Promise;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

import java.io.InputStream;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/** Delegate to interact with the assistant UI. */
class AutofillAssistantUiDelegate {
    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";
    private static final int PROGRESS_BAR_INITIAL_PROGRESS = 10;

    // TODO(crbug.com/806868): Use correct user locale.
    private static final SimpleDateFormat sDetailsTimeFormat =
            new SimpleDateFormat("H:mma", Locale.getDefault());
    private static final SimpleDateFormat sDetailsDateFormat =
            new SimpleDateFormat("EEE, MMM d", Locale.getDefault());

    private final ChromeActivity mActivity;
    private final Client mClient;
    private final View mFullContainer;
    private final View mOverlay;
    private final LinearLayout mBottomBar;
    private final HorizontalScrollView mCarouselScroll;
    private final ViewGroup mChipsViewContainer;
    private final TextView mStatusMessageView;
    private final AnimatedProgressBar mProgressBar;

    private final ViewGroup mDetails;
    private final AppCompatImageView mDetailsImage;
    private final TextView mDetailsTitle;
    private final TextView mDetailsText;
    private final int mDetailsImageWidth;
    private final int mDetailsImageHeight;

    /**
     * This is a client interface that relays interactions from the UI.
     *
     * Java version of the native autofill_assistant::UiDelegate.
     */
    public interface Client {
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
    protected static class Details {
        private final String mTitle;
        private final String mUrl;
        @Nullable
        private final Date mDate;
        private final String mDescription;

        public Details(String title, String url, @Nullable Date date, String description) {
            this.mTitle = title;
            this.mUrl = url;
            this.mDate = date;
            this.mDescription = description;
        }

        public String getTitle() {
            return mTitle;
        }

        public String getUrl() {
            return mUrl;
        }

        @Nullable
        public Date getDate() {
            return mDate;
        }

        public String getDescription() {
            return mDescription;
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

        mFullContainer = LayoutInflater.from(mActivity)
                                 .inflate(R.layout.autofill_assistant_sheet,
                                         ((ViewGroup) mActivity.findViewById(R.id.coordinator)))
                                 .findViewById(R.id.autofill_assistant);
        // TODO(crbug.com/806868): Set hint text on overlay.
        mOverlay = mFullContainer.findViewById(R.id.overlay);
        mOverlay.setOnClickListener(unusedView -> mClient.onClickOverlay());
        mBottomBar = mFullContainer.findViewById(R.id.bottombar);
        mBottomBar.findViewById(R.id.close_button)
                .setOnClickListener(unusedView -> mClient.onDismiss());
        mBottomBar.findViewById(R.id.feedback_button)
                .setOnClickListener(unusedView
                        -> HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity,
                                Profile.getLastUsedProfile(), mActivity.getActivityTab().getUrl(),
                                FEEDBACK_CATEGORY_TAG));
        mCarouselScroll = mBottomBar.findViewById(R.id.carousel_scroll);
        mChipsViewContainer = mCarouselScroll.findViewById(R.id.carousel);
        mStatusMessageView = mBottomBar.findViewById(R.id.status_message);
        mProgressBar = new AnimatedProgressBar(mBottomBar.findViewById(R.id.progress_bar),
                mActivity.getColor(R.color.modern_blue_600),
                mActivity.getColor(R.color.modern_blue_600_alpha_38_opaque));

        mDetails = (ViewGroup) mBottomBar.findViewById(R.id.details);
        mDetailsImage = (AppCompatImageView) mDetails.findViewById(R.id.details_image);
        mDetailsTitle = (TextView) mDetails.findViewById(R.id.details_title);
        mDetailsText = (TextView) mDetails.findViewById(R.id.details_text);
        mDetailsImageWidth = mActivity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mDetailsImageHeight = mActivity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);

        setCarouselTopPadding();

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
        clearChipsViewContainer();

        if (scriptHandles.isEmpty()) {
            return;
        }

        boolean hasHighlightedScript = hasHighlightedScript(scriptHandles);
        ChipStyle nonHighlightStyle =
                hasHighlightedScript ? ChipStyle.BUTTON_HAIRLINE : ChipStyle.CHIP_ASSISTIVE;

        for (int i = 0; i < scriptHandles.size(); i++) {
            // Add scripts in reverse order if the chips are right aligned.
            int j = hasHighlightedScript ? scriptHandles.size() - i - 1 : i;
            ScriptHandle scriptHandle = scriptHandles.get(j);
            ChipStyle chipStyle =
                    scriptHandle.isHighlight() ? ChipStyle.BUTTON_FILLED : nonHighlightStyle;
            TextView chipView = createChipView(scriptHandle.getName(), chipStyle);
            chipView.setOnClickListener((unusedView) -> {
                clearChipsViewContainer();
                mClient.onScriptSelected(scriptHandle.getPath());
            });

            addChipViewToContainer(chipView);
        }

        setChipViewContainerGravity(hasHighlightedScript);
        show();
    }

    private boolean hasHighlightedScript(ArrayList<ScriptHandle> scripts) {
        for (int i = 0; i < scripts.size(); i++) {
            if (scripts.get(i).isHighlight()) {
                return true;
            }
        }
        return false;
    }

    private void setChipViewContainerGravity(boolean alignRight) {
        ViewGroup.LayoutParams currentLayoutParams = mCarouselScroll.getLayoutParams();
        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(currentLayoutParams);
        layoutParams.gravity = alignRight ? Gravity.END : Gravity.START;
        mCarouselScroll.setLayoutParams(layoutParams);

        // Reset the scroll position.
        mCarouselScroll.post(
                () -> mCarouselScroll.fullScroll(alignRight ? View.FOCUS_RIGHT : View.FOCUS_LEFT));
    }

    private void addChipViewToContainer(TextView newChild) {
        // Add a left margin if it's not the first child.
        if (mChipsViewContainer.getChildCount() > 0) {
            LinearLayout.LayoutParams layoutParams =
                    new LinearLayout.LayoutParams(newChild.getLayoutParams());
            int leftMargin = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 16,
                    newChild.getContext().getResources().getDisplayMetrics());
            layoutParams.setMargins(leftMargin, 0, 0, 0);
            newChild.setLayoutParams(layoutParams);
        }

        mChipsViewContainer.addView(newChild);
        mChipsViewContainer.setVisibility(View.VISIBLE);
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
        if (!mFullContainer.isShown()) {
            mFullContainer.setVisibility(View.VISIBLE);

            // Set the initial progress. It is OK to make multiple calls to this method as it will
            // have an effect only on the first one.
            mProgressBar.maybeIncreaseProgress(PROGRESS_BAR_INITIAL_PROGRESS);
        }
    }

    public void hide() {
        mFullContainer.setVisibility(View.GONE);
    }

    public void showAutofillAssistantStoppedSnackbar(
            SnackbarManager.SnackbarController controller) {
        int durationMs = SnackbarManager.DEFAULT_SNACKBAR_DURATION_MS;
        Snackbar snackBar =
                Snackbar.make(mActivity.getString(
                                      R.string.autofill_assistant_stopped, durationMs / 1_000),
                                controller, Snackbar.TYPE_ACTION,
                                Snackbar.UMA_AUTOFILL_ASSISTANT_STOP_UNDO)
                        .setAction(mActivity.getString(R.string.undo), /* actionData= */ null);
        snackBar.setDuration(durationMs);
        mActivity.getSnackbarManager().showSnackbar(snackBar);
    }

    public void dismissSnackbar(SnackbarManager.SnackbarController controller) {
        mActivity.getSnackbarManager().dismissSnackbars(controller);
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
        mDetails.setVisibility(View.GONE);
        setCarouselTopPadding();
    }

    /** Called to show contextual information. */
    public void showDetails(Details details) {
        mDetailsTitle.setText(details.getTitle());
        mDetailsText.setText(getDetailsText(details));

        mDetailsImage.setVisibility(View.INVISIBLE);
        mDetails.setVisibility(View.VISIBLE);
        setCarouselTopPadding();
        show();

        String url = details.getUrl();
        if (!url.isEmpty()) {
            // The URL is safe given because it comes from the knowledge graph and is hosted on
            // Google servers.
            downloadImage(url).then(image -> {
                mDetailsImage.setImageDrawable(getRoundedImage(image));
                mDetailsImage.setVisibility(View.VISIBLE);
            }, ignoredError -> {});
        }
    }

    private void setCarouselTopPadding() {
        int topPadding = 0;
        if (mDetails.getVisibility() != View.VISIBLE) {
            topPadding = (int) TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, 16, mActivity.getResources().getDisplayMetrics());
        }
        mChipsViewContainer.setPadding(mChipsViewContainer.getPaddingLeft(), topPadding,
                mChipsViewContainer.getPaddingRight(), mChipsViewContainer.getPaddingBottom());
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

    /**
     * Show profiles in the bar.
     *
     * @param profiles List of profiles to show.
     */
    public void showProfiles(ArrayList<AutofillProfile> profiles) {
        if (profiles.isEmpty()) {
            mClient.onAddressSelected("");
            return;
        }

        clearChipsViewContainer();

        for (int i = 0; i < profiles.size(); i++) {
            AutofillProfile profile = profiles.get(i);
            // TODO(crbug.com/806868): Show more information than the street.
            TextView chipView =
                    createChipView(profile.getStreetAddress(), ChipStyle.CHIP_ASSISTIVE);
            chipView.setOnClickListener((unusedView) -> {
                clearChipsViewContainer();
                mClient.onAddressSelected(profile.getGUID());
            });
            addChipViewToContainer(chipView);
        }

        setChipViewContainerGravity(false);
        show();
    }

    private void clearChipsViewContainer() {
        mChipsViewContainer.removeAllViews();
        mChipsViewContainer.setVisibility(View.GONE);
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

        clearChipsViewContainer();

        for (int i = 0; i < cards.size(); i++) {
            CreditCard card = cards.get(i);
            // TODO(crbug.com/806868): Show more information than the card number.
            TextView chipView =
                    createChipView(card.getObfuscatedNumber(), ChipStyle.CHIP_ASSISTIVE);
            chipView.setOnClickListener((unusedView) -> {
                clearChipsViewContainer();
                mClient.onCardSelected(card.getGUID());
            });
            addChipViewToContainer(chipView);
        }

        setChipViewContainerGravity(false);
        show();
    }

    private Promise<Bitmap> downloadImage(String url) {
        Promise<Bitmap> promise = new Promise<>();
        new DownloadImageTask(promise).execute(url);
        return promise;
    }

    private static class DownloadImageTask extends AsyncTask<String, Void, Bitmap> {
        private final Promise<Bitmap> mPromise;
        private Exception mError = null;

        private DownloadImageTask(Promise<Bitmap> promise) {
            this.mPromise = promise;
        }

        @Override
        protected Bitmap doInBackground(String... urls) {
            try (InputStream in = new URL(urls[0]).openStream()) {
                return BitmapFactory.decodeStream(in);
            } catch (Exception e) {
                mError = e;
                return null;
            }
        }

        @Override
        protected void onPostExecute(Bitmap bitmap) {
            if (mError != null) {
                mPromise.reject(mError);
                return;
            }

            mPromise.fulfill(bitmap);
        }
    }
}
