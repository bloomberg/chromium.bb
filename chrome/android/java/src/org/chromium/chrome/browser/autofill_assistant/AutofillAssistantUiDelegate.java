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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.Promise;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;

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
    private final ViewGroup mChipsViewContainer;
    private final TextView mStatusMessageView;

    private final ViewGroup mDetails;
    private final AppCompatImageView mDetailsImage;
    private final TextView mDetailsTitle;
    private final TextView mDetailsText;
    private final TextView mDetailsTime;
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

        /** Constructor. */
        public ScriptHandle(String name, String path) {
            mName = name;
            mPath = path;
        }

        /** Returns the display name. */
        public String getName() {
            return mName;
        }

        /** Returns the script path. */
        public String getPath() {
            return mPath;
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
        mBottomBar.findViewById(R.id.close_button).setOnClickListener(unusedView -> shutdown());
        mBottomBar.findViewById(R.id.feedback_button)
                .setOnClickListener(unusedView
                        -> HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity,
                                Profile.getLastUsedProfile(), mActivity.getActivityTab().getUrl(),
                                FEEDBACK_CATEGORY_TAG));
        mChipsViewContainer = mBottomBar.findViewById(R.id.carousel);
        mStatusMessageView = mBottomBar.findViewById(R.id.status_message);

        mDetails = (ViewGroup) mBottomBar.findViewById(R.id.details);
        mDetailsImage = (AppCompatImageView) mDetails.findViewById(R.id.details_image);
        mDetailsTitle = (TextView) mDetails.findViewById(R.id.details_title);
        mDetailsText = (TextView) mDetails.findViewById(R.id.details_text);
        mDetailsTime = (TextView) mDetails.findViewById(R.id.details_time);
        mDetailsImageWidth = mActivity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mDetailsImageHeight = mActivity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);

        // TODO(crbug.com/806868): Listen for contextual search shown so as to hide this UI.
    }

    /**
     * Shows a message in the status bar.
     *
     * @param message Message to display.
     */
    public void showStatusMessage(@Nullable String message) {
        ensureFullContainerIsShown();

        mStatusMessageView.setText(message);
    }

    private void ensureFullContainerIsShown() {
        if (!mFullContainer.isShown()) mFullContainer.setVisibility(View.VISIBLE);
    }

    /**
     * Updates the list of scripts in the bar.
     *
     * @param scriptHandles List of scripts to show.
     */
    public void updateScripts(ArrayList<ScriptHandle> scriptHandles) {
        mChipsViewContainer.removeAllViews();

        if (scriptHandles.isEmpty()) {
            return;
        }

        for (int i = 0; i < scriptHandles.size(); i++) {
            ScriptHandle scriptHandle = scriptHandles.get(i);
            TextView chipView = createChipView(scriptHandle.getName());
            chipView.setOnClickListener((unusedView) -> {
                mChipsViewContainer.removeAllViews();
                mClient.onScriptSelected(scriptHandle.getPath());
            });
            mChipsViewContainer.addView(chipView);
        }

        ensureFullContainerIsShown();
    }

    private TextView createChipView(String text) {
        TextView chipView = (TextView) (LayoutInflater.from(mActivity).inflate(
                R.layout.autofill_assistant_chip, null /* root */));
        chipView.setText(text);
        return chipView;
    }

    /** Called to show overlay. */
    public void showOverlay() {
        mOverlay.setVisibility(View.VISIBLE);
    }

    /** Called to hide overlay. */
    public void hideOverlay() {
        mOverlay.setVisibility(View.GONE);
    }

    public void hideDetails() {
        mDetails.setVisibility(View.GONE);
    }

    /** Called to show contextual information. */
    public void showDetails(Details details) {
        mDetailsTitle.setText(details.getTitle());
        mDetailsText.setText(getDetailsText(details));
        mDetailsTime.setText(getDetailsTime(details.getDate()));

        mDetailsImage.setVisibility(View.GONE);
        mDetails.setVisibility(View.VISIBLE);
        ensureFullContainerIsShown();

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

    private String getDetailsText(Details details) {
        List<String> parts = new ArrayList<>();
        Date date = details.getDate();
        if (date != null) {
            parts.add(sDetailsDateFormat.format(date));
        }

        String description = details.getDescription();
        if (description != null && !description.isEmpty()) {
            parts.add(description);
        }

        // TODO(crbug.com/806868): Use a view instead of this dot text.
        return TextUtils.join(" • ", parts);
    }

    private String getDetailsTime(@Nullable Date date) {
        return date != null ? sDetailsTimeFormat.format(date).toLowerCase(Locale.getDefault()) : "";
    }

    private Drawable getRoundedImage(Bitmap bitmap) {
        RoundedBitmapDrawable roundedBitmap = RoundedBitmapDrawableFactory.create(
                mActivity.getResources(),
                ThumbnailUtils.extractThumbnail(bitmap, mDetailsImageWidth, mDetailsImageHeight));
        // TODO(crbug.com/806868): Get radius from resources dimensions.
        roundedBitmap.setCornerRadius(10);
        return roundedBitmap;
    }

    /**
     * Shuts down the Autofill Assistant. The UI disappears and any associated state goes away.
     */
    public void shutdown() {
        mFullContainer.setVisibility(View.GONE);
        mClient.onDismiss();
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

        mChipsViewContainer.removeAllViews();

        for (int i = 0; i < profiles.size(); i++) {
            AutofillProfile profile = profiles.get(i);
            // TODO(crbug.com/806868): Show more information than the street.
            TextView chipView = createChipView(profile.getStreetAddress());
            chipView.setOnClickListener((unusedView) -> {
                mChipsViewContainer.removeAllViews();
                mClient.onAddressSelected(profile.getGUID());
            });
            mChipsViewContainer.addView(chipView);
        }

        ensureFullContainerIsShown();
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

        mChipsViewContainer.removeAllViews();

        for (int i = 0; i < cards.size(); i++) {
            CreditCard card = cards.get(i);
            // TODO(crbug.com/806868): Show more information than the card number.
            TextView chipView = createChipView(card.getObfuscatedNumber());
            chipView.setOnClickListener((unusedView) -> {
                mChipsViewContainer.removeAllViews();
                mClient.onCardSelected(card.getGUID());
            });
            mChipsViewContainer.addView(chipView);
        }

        ensureFullContainerIsShown();
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
