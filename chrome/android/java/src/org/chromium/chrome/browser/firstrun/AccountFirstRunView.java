// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.ImageCarousel.ImageCarouselPositionChangeListener;
import org.chromium.chrome.browser.profiles.ProfileDownloader;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.List;

/**
 * This view allows the user to select an account to log in to, add an account,
 * cancel account selection, etc. Users of this class should
 * {@link AccountFirstRunView#setListener(Listener)} after the view has been
 * inflated.
 */
public class AccountFirstRunView extends FrameLayout
        implements ImageCarouselPositionChangeListener, ProfileDownloader.Observer {

    /**
     * Callbacks for various account selection events.
     */
    public interface Listener {
        /**
         * The user canceled account selection.
         */
        public void onAccountSelectionCanceled();

        /**
         * The user wants to make a new account.
         */
        public void onNewAccount();

        /**
         * The user selected an account.
         * This call will be followed by either {@link #onSettingsClicked} or
         * {@link #onDoneClicked}.
         * @param accountName The name of the account
         */
        public void onAccountSelected(String accountName);

        /**
         * The user has completed the dialog flow.
         * This will only be called after {@link #onAccountSelected} and should exit the View.
         */
        public void onDoneClicked();

        /**
         * The user has selected to view sync settings.
         * This will only be called after {@link #onAccountSelected} and should exit the View.
         */
        public void onSettingsClicked();

        /**
         * Failed to set the forced account because it wasn't found.
         * @param forcedAccountName The name of the forced-sign-in account
         */
        public void onFailedToSetForcedAccount(String forcedAccountName);
    }

    private class SpinnerOnItemSelectedListener implements AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            String accountName = parent.getItemAtPosition(pos).toString();
            if (accountName.equals(mAddAnotherAccount)) {
                // Don't allow "add account" to remain selected. http://crbug.com/421052
                int oldPosition = mArrayAdapter.getPosition(mAccountName);
                if (oldPosition == -1) oldPosition = 0;
                mSpinner.setSelection(oldPosition, false);

                mListener.onNewAccount();
            } else {
                mAccountName = accountName;
                if (!mPositionSetProgrammatically) mImageCarousel.scrollTo(pos, false, false);
                mPositionSetProgrammatically = false;
            }
        }
        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mAccountName = parent.getItemAtPosition(0).toString();
        }
    }

    private static final String TAG = "AccountFirstRunView";

    private static final int EXPERIMENT_TITLE_VARIANT_MASK = 1;
    private static final int EXPERIMENT_SUMMARY_VARIANT_MASK = 2;
    private static final int EXPERIMENT_LAYOUT_VARIANT_MASK = 4;
    private static final int EXPERIMENT_MAX_VALUE = 7;

    private static final String SETTINGS_LINK_OPEN = "<LINK1>";
    private static final String SETTINGS_LINK_CLOSE = "</LINK1>";

    private AccountManagerHelper mAccountManagerHelper;
    private List<String> mAccountNames;
    private ArrayAdapter<CharSequence> mArrayAdapter;
    private ImageCarousel mImageCarousel;
    private Button mPositiveButton;
    private Button mNegativeButton;
    private TextView mTitle;
    private TextView mDescriptionText;
    private Listener mListener;
    private Spinner mSpinner;
    private Drawable mSpinnerBackground;
    private String mForcedAccountName;
    private String mAccountName;
    private String mAddAnotherAccount;
    private ProfileDataCache mProfileData;
    private boolean mSignedIn;
    private boolean mPositionSetProgrammatically;
    private int mDescriptionTextId;
    private int mCancelButtonTextId;
    private boolean mIsChildAccount;
    private boolean mHorizontalModeEnabled = true;
    private boolean mShowSettingsSpan = true;

    public AccountFirstRunView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAccountManagerHelper = AccountManagerHelper.get(getContext().getApplicationContext());
    }

    /**
     * Initializes this view with profile images and full names.
     * @param profileData ProfileDataCache that will be used to call to retrieve user account info.
     */
    public void init(ProfileDataCache profileData) {
        setProfileDataCache(profileData);
    }

    /**
     * Sets the profile data cache.
     * @param profileData ProfileDataCache that will be used to call to retrieve user account info.
     */
    public void setProfileDataCache(ProfileDataCache profileData) {
        mProfileData = profileData;
        mProfileData.setObserver(this);
        updateProfileImages();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mImageCarousel = (ImageCarousel) findViewById(R.id.image_slider);
        mImageCarousel.setListener(this);

        mPositiveButton = (Button) findViewById(R.id.positive_button);
        mNegativeButton = (Button) findViewById(R.id.negative_button);

        // A workaround for Android support library ignoring padding set in XML. b/20307607
        int padding = getResources().getDimensionPixelSize(R.dimen.fre_button_padding);
        ApiCompatibilityUtils.setPaddingRelative(mPositiveButton, padding, 0, padding, 0);
        ApiCompatibilityUtils.setPaddingRelative(mNegativeButton, padding, 0, padding, 0);

        mTitle = (TextView) findViewById(R.id.title);
        mDescriptionText = (TextView) findViewById(R.id.description);
        // For the spans to be clickable.
        mDescriptionText.setMovementMethod(LinkMovementMethod.getInstance());
        mDescriptionTextId = R.string.fre_account_choice_description;

        // TODO(peconn): Ensure this is changed to R.string.cancel when used in Settings > Sign In.
        mCancelButtonTextId = R.string.fre_skip_text;

        // Set the invisible TextView to contain the longest text the visible TextView can hold.
        // It assumes that the signed in description for child accounts is the longest text.
        ((TextView) findViewById(R.id.longest_description)).setText(getSignedInDescription(true));

        mAddAnotherAccount = getResources().getString(R.string.fre_add_account);

        mSpinner = (Spinner) findViewById(R.id.google_accounts_spinner);
        mSpinnerBackground = mSpinner.getBackground();
        mArrayAdapter = new ArrayAdapter<CharSequence>(
                getContext().getApplicationContext(), R.layout.fre_spinner_text);

        mArrayAdapter.setDropDownViewResource(R.layout.fre_spinner_dropdown);
        mSpinner.setAdapter(mArrayAdapter);
        mSpinner.setOnItemSelectedListener(new SpinnerOnItemSelectedListener());

        // Only set the spinner's content description right before the accessibility action is going
        // to be performed. Otherwise, the the content description is read when the
        // AccountFirstRunView is created because setting the spinner's adapter causes a
        // TYPE_VIEW_SELECTED event. ViewPager loads the next and previous pages according to
        // it's off-screen page limit, which is one by default, so without this the content
        // description ends up being read when the card before this one shown.
        mSpinner.setAccessibilityDelegate(new AccessibilityDelegate() {
            @Override
            public boolean performAccessibilityAction(View host, int action, Bundle args) {
                if (mSpinner.getContentDescription() == null) {
                    mSpinner.setContentDescription(getResources().getString(
                            R.string.accessibility_fre_account_spinner));
                }
                return super.performAccessibilityAction(host, action, args);
            }
        });

        showSignInPage();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        updateAccounts();
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (visibility == View.VISIBLE) {
            if (updateAccounts()) {
                // A new account has been added and the visibility has returned to us.
                // The updateAccounts function will have selected the new account.
                // Shortcut to confirm sign in page.
                showConfirmSignInPage();
            }
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // This assumes that view's layout_width is set to match_parent.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);
        LinearLayout content = (LinearLayout) findViewById(R.id.fre_content);
        int paddingStart = 0;
        if (mHorizontalModeEnabled
                && width >= 2 * getResources().getDimension(R.dimen.fre_image_carousel_width)
                && width > height) {
            content.setOrientation(LinearLayout.HORIZONTAL);
            paddingStart = getResources().getDimensionPixelSize(R.dimen.fre_margin);
        } else {
            content.setOrientation(LinearLayout.VERTICAL);
        }
        ApiCompatibilityUtils.setPaddingRelative(content,
                paddingStart,
                content.getPaddingTop(),
                ApiCompatibilityUtils.getPaddingEnd(content),
                content.getPaddingBottom());
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Changes the visuals slightly for when this view appears in the recent tabs page instead of
     * in first run. For example, the title text is changed as well as the button style.
     * This is currently used in the Recent Tabs Promo and the Enhanced Bookmark page.
     */
    public void configureForRecentTabsOrBookmarksPage() {
        mHorizontalModeEnabled = false;
        mShowSettingsSpan = false;

        setBackgroundResource(R.color.ntp_bg);
        mTitle.setText(R.string.sign_in_to_chrome);

        mCancelButtonTextId = R.string.cancel;
        setUpCancelButton();

        setPadding(0, 0, 0,
                getResources().getDimensionPixelOffset(R.dimen.sign_in_promo_padding_bottom));
    }

    /**
     * Changes the visuals slightly for when this view is shown in a subsequent run after user adds
     * a Google account to the device.
     */
    public void configureForAddAccountPromo() {
        int experimentGroup = SigninManager.getAndroidSigninPromoExperimentGroup();
        assert experimentGroup >= 0 && experimentGroup <= EXPERIMENT_MAX_VALUE;

        if ((experimentGroup & EXPERIMENT_TITLE_VARIANT_MASK) != 0) {
            mTitle.setText(R.string.make_chrome_yours);
        }

        mDescriptionTextId = (experimentGroup & EXPERIMENT_SUMMARY_VARIANT_MASK) != 0
                ? R.string.sign_in_to_chrome_summary_variant : R.string.sign_in_to_chrome_summary;

        if ((experimentGroup & EXPERIMENT_LAYOUT_VARIANT_MASK) != 0) {
            mImageCarousel.setVisibility(GONE);

            ImageView illustrationView = new ImageView(getContext());
            illustrationView.setImageResource(R.drawable.signin_promo_illustration);
            illustrationView.setBackgroundColor(ApiCompatibilityUtils.getColor(getResources(),
                    R.color.illustration_background_color));

            LinearLayout linearLayout = (LinearLayout) findViewById(R.id.fre_account_linear_layout);
            linearLayout.addView(illustrationView, 0);
        }
    }

    /**
     * Enable or disable UI elements so the user can't select an account, cancel, etc.
     *
     * @param enabled The state to change to.
     */
    public void setButtonsEnabled(boolean enabled) {
        mPositiveButton.setEnabled(enabled);
        mNegativeButton.setEnabled(enabled);
    }

    /**
     * Set the account selection event listener.  See {@link Listener}
     *
     * @param listener The listener.
     */
    public void setListener(Listener listener) {
        mListener = listener;
    }

    /**
     * Refresh the list of available system account.
     * @return Whether any new accounts were added (the first newly added account will now be
     *         selected).
     */
    private boolean updateAccounts() {
        if (mSignedIn) return false;

        List<String> oldAccountNames = mAccountNames;
        mAccountNames = mAccountManagerHelper.getGoogleAccountNames();
        int accountToSelect = 0;
        if (isInForcedAccountMode()) {
            accountToSelect = mAccountNames.indexOf(mForcedAccountName);
            if (accountToSelect < 0) {
                mListener.onFailedToSetForcedAccount(mForcedAccountName);
                return false;
            }
        } else {
            accountToSelect = getIndexOfNewElement(
                    oldAccountNames, mAccountNames, mSpinner.getSelectedItemPosition());
        }

        mArrayAdapter.clear();
        if (!mAccountNames.isEmpty()) {
            mSpinner.setVisibility(View.VISIBLE);
            mArrayAdapter.addAll(mAccountNames);
            mArrayAdapter.add(mAddAnotherAccount);

            setUpSignInButton(true);
            mDescriptionText.setText(mDescriptionTextId);

        } else {
            mSpinner.setVisibility(View.GONE);
            mArrayAdapter.add(mAddAnotherAccount);
            setUpSignInButton(false);
            mDescriptionText.setText(R.string.fre_no_account_choice_description);
        }

        if (mProfileData != null) mProfileData.update();
        updateProfileImages();

        mSpinner.setSelection(accountToSelect);
        mAccountName = mArrayAdapter.getItem(accountToSelect).toString();
        mImageCarousel.scrollTo(accountToSelect, false, false);

        return oldAccountNames != null
                && !(oldAccountNames.size() == mAccountNames.size()
                    && oldAccountNames.containsAll(mAccountNames));
    }

    /**
     * Attempt to select a new element that is in the new list, but not in the old list.
     * If no such element exist and both the new and the old lists are the same then keep
     * the selection. Otherwise select the first element.
     * @param oldList Old list of user accounts.
     * @param newList New list of user accounts.
     * @param oldIndex Index of the selected account in the old list.
     * @return The index of the new element, if it does not exist but lists are the same the
     *         return the old index, otherwise return 0.
     */
    private static int getIndexOfNewElement(
            List<String> oldList, List<String> newList, int oldIndex) {
        if (oldList == null || newList == null) return 0;
        if (oldList.size() == newList.size() && oldList.containsAll(newList)) return oldIndex;
        if (oldList.size() + 1 == newList.size()) {
            for (int i = 0; i < newList.size(); i++) {
                if (!oldList.contains(newList.get(i))) return i;
            }
        }
        return 0;
    }

    @Override
    public void onProfileDownloaded(String accountId, String fullName, String givenName,
            Bitmap bitmap) {
        updateProfileImages();
    }

    private void updateProfileImages() {
        if (mProfileData == null) return;

        int count = mAccountNames.size();

        Bitmap[] images;
        if (count == 0) {
            images = new Bitmap[1];
            images[0] = mProfileData.getImage(null);
        } else {
            images = new Bitmap[count];
            for (int i = 0; i < count; ++i) {
                images[i] = mProfileData.getImage(mAccountNames.get(i));
            }
        }

        mImageCarousel.setImages(images);
        updateProfileName();
    }

    private void updateProfileName() {
        if (!mSignedIn) return;

        String name = null;
        if (mProfileData != null) {
            if (mIsChildAccount) name = mProfileData.getGivenName(mAccountName);
            if (name == null) name = mProfileData.getFullName(mAccountName);
        }
        if (name == null) name = mAccountName;
        String text = String.format(getResources().getString(R.string.fre_hi_name), name);
        mTitle.setText(text);
    }

    /**
     * Updates the view to show that sign in has completed.
     */
    public void switchToSignedMode() {
        showConfirmSignInPage();
    }

    private void showSignInPage() {
        mSignedIn = false;
        mTitle.setText(R.string.sign_in_to_chrome);

        mSpinner.setEnabled(true);
        mSpinner.setBackground(mSpinnerBackground);
        mImageCarousel.setVisibility(VISIBLE);

        setUpCancelButton();
        updateAccounts();

        mImageCarousel.setSignedInMode(false);
    }

    private void showConfirmSignInPage() {
        mSignedIn = true;
        updateProfileName();

        mSpinner.setEnabled(false);
        mSpinner.setBackground(null);
        setUpConfirmButton();
        setUpUndoButton();

        if (mShowSettingsSpan) {
            ClickableSpan settingsSpan = new ClickableSpan() {
                @Override
                public void onClick(View widget) {
                    mListener.onAccountSelected(mAccountName);
                    mListener.onSettingsClicked();
                }

                @Override
                public void updateDrawState(TextPaint textPaint) {
                    textPaint.setColor(textPaint.linkColor);
                    textPaint.setUnderlineText(false);
                }
            };
            mDescriptionText.setText(SpanApplier.applySpans(getSignedInDescription(mIsChildAccount),
                    new SpanInfo(SETTINGS_LINK_OPEN, SETTINGS_LINK_CLOSE, settingsSpan)));
        } else {
            // If we aren't showing the span, get rid of the LINK1 annotations.
            mDescriptionText.setText(getSignedInDescription(mIsChildAccount)
                                             .replace(SETTINGS_LINK_OPEN, "")
                                             .replace(SETTINGS_LINK_CLOSE, ""));
        }

        mImageCarousel.setVisibility(VISIBLE);
        mImageCarousel.setSignedInMode(true);
    }

    private void setUpCancelButton() {
        setNegativeButtonVisible(true);

        mNegativeButton.setText(getResources().getText(mCancelButtonTextId));
        mNegativeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                setButtonsEnabled(false);
                mListener.onAccountSelectionCanceled();
            }
        });
    }

    private void setUpSignInButton(boolean hasAccounts) {
        if (hasAccounts) {
            mPositiveButton.setText(R.string.choose_account_sign_in);
            mPositiveButton.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    showConfirmSignInPage();
                }
            });
        } else {
            mPositiveButton.setText(R.string.fre_no_accounts);
            mPositiveButton.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    mListener.onNewAccount();
                }
            });
        }
    }

    private void setUpUndoButton() {
        setNegativeButtonVisible(!isInForcedAccountMode());
        if (isInForcedAccountMode()) return;

        mNegativeButton.setText(getResources().getText(R.string.undo));
        mNegativeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                showSignInPage();
            }
        });
    }

    private void setUpConfirmButton() {
        mPositiveButton.setText(getResources().getText(R.string.fre_accept));
        mPositiveButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mListener.onAccountSelected(mAccountName);
                mListener.onDoneClicked();
            }
        });
    }

    private void setNegativeButtonVisible(boolean enabled) {
        if (enabled) {
            mNegativeButton.setVisibility(View.VISIBLE);
            findViewById(R.id.positive_button_end_padding).setVisibility(View.GONE);
        } else {
            mNegativeButton.setVisibility(View.GONE);
            findViewById(R.id.positive_button_end_padding).setVisibility(View.INVISIBLE);
        }
    }

    private String getSignedInDescription(boolean childAccount) {
        if (childAccount) {
            return getResources().getString(R.string.fre_signed_in_description) + '\n'
                    + getResources().getString(R.string.fre_signed_in_description_uca_addendum);
        } else {
            return getResources().getString(R.string.fre_signed_in_description);
        }
    }

    /**
     * @param isChildAccount Whether this view is for a child account.
     */
    public void setIsChildAccount(boolean isChildAccount) {
        mIsChildAccount = isChildAccount;
    }

    /**
     * Switches the view to "no choice, just a confirmation" forced-account mode.
     * @param forcedAccountName An account that should be force-selected.
     */
    public void switchToForcedAccountMode(String forcedAccountName) {
        mForcedAccountName = forcedAccountName;
        updateAccounts();
        assert TextUtils.equals(mAccountName, mForcedAccountName);
        switchToSignedMode();
        assert TextUtils.equals(mAccountName, mForcedAccountName);
    }

    /**
     * @return Whether the view is in signed in mode.
     */
    public boolean isSignedIn() {
        return mSignedIn;
    }

    /**
     * @return Whether the view is in "no choice, just a confirmation" forced-account mode.
     */
    public boolean isInForcedAccountMode() {
        return mForcedAccountName != null;
    }

    @Override
    public void onPositionChanged(int i) {
        mPositionSetProgrammatically = true;
        mSpinner.setSelection(i);
    }
}
