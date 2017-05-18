// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.support.design.widget.TabLayout;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.LinearLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.translate.TranslateMenu;
import org.chromium.chrome.browser.infobar.translate.TranslateMenuHelper;
import org.chromium.chrome.browser.infobar.translate.TranslateTabLayout;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.ui.widget.Toast;

/**
 * Java version of the compact translate infobar.
 */
class TranslateCompactInfoBar extends InfoBar
        implements TabLayout.OnTabSelectedListener, TranslateMenuHelper.TranslateMenuListener {
    public static final int TRANSLATING_INFOBAR = 1;

    private static final int SOURCE_TAB_INDEX = 0;
    private static final int TARGET_TAB_INDEX = 1;

    // Action ID for Snackbar.
    // Actions performed by clicking on on the overflow menu.
    public static final int ACTION_OVERFLOW_ALWAYS_TRANSLATE = 0;
    public static final int ACTION_OVERFLOW_NEVER_SITE = 1;
    public static final int ACTION_OVERFLOW_NEVER_LANGUAGE = 2;
    // Actions triggered automatically. (when translation or denied count reaches the threshold.)
    public static final int ACTION_AUTO_ALWAYS_TRANSLATE = 3;
    public static final int ACTION_AUTO_NEVER_LANGUAGE = 4;

    private final int mInitialStep;
    private final TranslateOptions mOptions;

    private long mNativeTranslateInfoBarPtr;
    private TranslateTabLayout mTabLayout;

    // Metric to track the total number of translations in a page, including reverts to original.
    private int mTotalTranslationCount = 0;

    // Histogram names for logging metrics.
    private static final String INFOBAR_HISTOGRAM_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.Translate";
    private static final String INFOBAR_HISTOGRAM_MORE_LANGUAGES_LANGUAGE =
            "Translate.CompactInfobar.Language.MoreLanguages";
    private static final String INFOBAR_HISTOGRAM_PAGE_NOT_IN_LANGUAGE =
            "Translate.CompactInfobar.Language.PageNotIn";
    private static final String INFOBAR_HISTOGRAM_ALWAYS_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.AlwaysTranslate";
    private static final String INFOBAR_HISTOGRAM_NEVER_TRANSLATE_LANGUAGE =
            "Translate.CompactInfobar.Language.NeverTranslate";
    private static final String INFOBAR_HISTOGRAM = "Translate.CompactInfobar.Event";
    private static final String INFOBAR_HISTOGRAM_TRANSLATION_COUNT =
            "Translate.CompactInfobar.TranslationsPerPage";

    /**
     * This is used to back a UMA histogram, so it should be treated as
     * append-only. The values should not be changed or reused, and
     * INFOBAR_HISTOGRAM_BOUNDARY should be the last.
     */
    private static final int INFOBAR_IMPRESSION = 0;
    private static final int INFOBAR_TRANSLATE = 1;
    private static final int INFOBAR_DECLINE = 2;
    private static final int INFOBAR_OPTIONS = 3;
    private static final int INFOBAR_MORE_LANGUAGES = 4;
    private static final int INFOBAR_MORE_LANGUAGES_TRANSLATE = 5;
    private static final int INFOBAR_PAGE_NOT_IN = 6;
    private static final int INFOBAR_ALWAYS_TRANSLATE = 7;
    private static final int INFOBAR_NEVER_TRANSLATE = 8;
    private static final int INFOBAR_NEVER_TRANSLATE_SITE = 9;
    private static final int INFOBAR_SCROLL_HIDE = 10;
    private static final int INFOBAR_SCROLL_SHOW = 11;
    private static final int INFOBAR_REVERT = 12;
    private static final int INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION = 13;
    private static final int INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION = 14;
    private static final int INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION = 15;
    private static final int INFOBAR_SNACKBAR_CANCEL_ALWAYS = 16;
    private static final int INFOBAR_SNACKBAR_CANCEL_NEVER_SITE = 17;
    private static final int INFOBAR_SNACKBAR_CANCEL_NEVER = 18;
    private static final int INFOBAR_ALWAYS_TRANSLATE_UNDO = 19;
    private static final int INFOBAR_CLOSE = 20;
    private static final int INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION = 21;
    private static final int INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION = 22;
    private static final int INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS = 23;
    private static final int INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER = 24;
    private static final int INFOBAR_HISTOGRAM_BOUNDARY = 25;

    // Need 2 instances of TranslateMenuHelper to prevent a race condition bug which happens when
    // showing language menu after dismissing overflow menu.
    private TranslateMenuHelper mOverflowMenuHelper;
    private TranslateMenuHelper mLanguageMenuHelper;
    private TintedImageButton mMenuButton;
    private boolean mUserInteracted;

    /** The controller for translate UI snackbars. */
    class TranslateSnackbarController implements SnackbarController {
        private final int mActionId;

        public TranslateSnackbarController(int actionId) {
            mActionId = actionId;
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            handleTranslateOptionPostSnackbar(mActionId);
        }

        @Override
        public void onAction(Object actionData) {
            switch (mActionId) {
                case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_ALWAYS);
                    return;
                case ACTION_AUTO_ALWAYS_TRANSLATE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS);
                    return;
                case ACTION_OVERFLOW_NEVER_LANGUAGE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_NEVER);
                    return;
                case ACTION_AUTO_NEVER_LANGUAGE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER);
                    // This snackbar is triggered automatically after a close button click.  Need to
                    // dismiss the infobar even if the user cancels the "Never Translate".
                    performCloseButtonActionWithoutDeniedCheck();
                    return;
                case ACTION_OVERFLOW_NEVER_SITE:
                    recordInfobarAction(INFOBAR_SNACKBAR_CANCEL_NEVER_SITE);
                    return;
                default:
                    assert false : "Unsupported Menu Item Id, when handling snackbar action";
                    return;
            }
        }
    };

    @CalledByNative
    private static InfoBar create(int initialStep, String sourceLanguageCode,
            String targetLanguageCode, boolean alwaysTranslate, boolean triggeredFromMenu,
            String[] languages, String[] languageCodes, int[] hashCodes) {
        recordInfobarAction(INFOBAR_IMPRESSION);
        return new TranslateCompactInfoBar(initialStep, sourceLanguageCode, targetLanguageCode,
                alwaysTranslate, triggeredFromMenu, languages, languageCodes, hashCodes);
    }

    TranslateCompactInfoBar(int initialStep, String sourceLanguageCode, String targetLanguageCode,
            boolean alwaysTranslate, boolean triggeredFromMenu, String[] languages,
            String[] languageCodes, int[] hashCodes) {
        super(R.drawable.infobar_translate_compact, null, null);

        mInitialStep = initialStep;
        mOptions = TranslateOptions.create(sourceLanguageCode, targetLanguageCode, languages,
                languageCodes, alwaysTranslate, triggeredFromMenu, hashCodes);
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout parent) {
        LinearLayout content =
                (LinearLayout) LayoutInflater.from(getContext())
                        .inflate(R.layout.infobar_translate_compact_content, parent, false);

        mTabLayout = (TranslateTabLayout) content.findViewById(R.id.translate_infobar_tabs);
        mTabLayout.addTabs(mOptions.sourceLanguageName(), mOptions.targetLanguageName());

        // Set translating status in the beginning for pages translated automatically.
        if (mInitialStep == TRANSLATING_INFOBAR) {
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            mUserInteracted = true;
        }

        mTabLayout.addOnTabSelectedListener(this);

        mMenuButton = (TintedImageButton) content.findViewById(R.id.translate_infobar_menu_button);
        mMenuButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                recordInfobarAction(INFOBAR_OPTIONS);
                initMenuHelper(TranslateMenu.MENU_OVERFLOW);
                mOverflowMenuHelper.show(TranslateMenu.MENU_OVERFLOW);
            }
        });

        parent.addContent(content, 1.0f);
    }

    private void initMenuHelper(int menuType) {
        switch (menuType) {
            case TranslateMenu.MENU_OVERFLOW:
                if (mOverflowMenuHelper == null) {
                    mOverflowMenuHelper =
                            new TranslateMenuHelper(getContext(), mMenuButton, mOptions, this);
                }
                return;
            case TranslateMenu.MENU_TARGET_LANGUAGE:
            case TranslateMenu.MENU_SOURCE_LANGUAGE:
                if (mLanguageMenuHelper == null) {
                    mLanguageMenuHelper =
                            new TranslateMenuHelper(getContext(), mMenuButton, mOptions, this);
                }
                return;
            default:
                assert false : "Unsupported Menu Item Id";
        }
    }

    private void startTranslating(int tabPosition) {
        if (TARGET_TAB_INDEX == tabPosition) {
            // Already on the target tab.
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            onButtonClicked(ActionType.TRANSLATE);
            mUserInteracted = true;
        } else {
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
        }
    }

    @CalledByNative
    private void onPageTranslated(int errorType) {
        incrementAndRecordTranslationsPerPageCount();
        if (mTabLayout != null) {
            mTabLayout.hideProgressBar();
            if (errorType != 0) {
                Toast.makeText(getContext(), R.string.translate_infobar_error, Toast.LENGTH_SHORT)
                        .show();
                // Disable OnTabSelectedListener then revert selection.
                mTabLayout.removeOnTabSelectedListener(this);
                mTabLayout.getTabAt(SOURCE_TAB_INDEX).select();
                // Add OnTabSelectedListener back.
                mTabLayout.addOnTabSelectedListener(this);
            }
        }
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        mNativeTranslateInfoBarPtr = nativePtr;
    }

    @CalledByNative
    private void setAutoAlwaysTranslate() {
        createAndShowSnackbar(getContext().getString(R.string.translate_snackbar_always_translate,
                                      mOptions.sourceLanguageName(), mOptions.targetLanguageName()),
                Snackbar.UMA_TRANSLATE_ALWAYS, ACTION_AUTO_ALWAYS_TRANSLATE);
    }

    private void setAutoNeverTranslate() {
        createAndShowSnackbar(getContext().getString(R.string.translate_snackbar_language_never,
                                      mOptions.sourceLanguageName()),
                Snackbar.UMA_TRANSLATE_NEVER, ACTION_AUTO_NEVER_LANGUAGE);
    }

    @Override
    protected void onNativeDestroyed() {
        mNativeTranslateInfoBarPtr = 0;
        super.onNativeDestroyed();
    }

    @Override
    public void onCloseButtonClicked() {
        // If mUserInteracted is false, it is regarded as a translation denied.
        if (!mUserInteracted) {
            recordInfobarAction(INFOBAR_DECLINE);
            // This will increment the denied count.
            onButtonClicked(ActionType.CANCEL);

            // Check if it should trigger the auto "never translate".
            if (!mOptions.neverTranslateLanguageState()
                    && nativeShouldAutoNeverTranslate(mNativeTranslateInfoBarPtr)) {
                setAutoNeverTranslate();
                // Postpone the infobar dismiss until the snackbar finished showing.  Otherwise, the
                // reference to the native infobar is killed and there is no way for the snackbar to
                // perform the action.
                return;
            }
        }
        recordInfobarAction(INFOBAR_CLOSE);
        // This line will dismiss this infobar.
        performCloseButtonActionWithoutDeniedCheck();
    }

    private void performCloseButtonActionWithoutDeniedCheck() {
        super.onCloseButtonClicked();
    }

    @Override
    public void onTabSelected(TabLayout.Tab tab) {
        switch (tab.getPosition()) {
            case SOURCE_TAB_INDEX:
                incrementAndRecordTranslationsPerPageCount();
                recordInfobarAction(INFOBAR_REVERT);
                onButtonClicked(ActionType.TRANSLATE_SHOW_ORIGINAL);
                return;
            case TARGET_TAB_INDEX:
                recordInfobarAction(INFOBAR_TRANSLATE);
                recordInfobarLanguageData(
                        INFOBAR_HISTOGRAM_TRANSLATE_LANGUAGE, mOptions.targetLanguageCode());
                startTranslating(TARGET_TAB_INDEX);
                return;
            default:
                assert false : "Unexpected Tab Index";
        }
    }

    @Override
    public void onTabUnselected(TabLayout.Tab tab) {}

    @Override
    public void onTabReselected(TabLayout.Tab tab) {}

    @Override
    public void onOverflowMenuItemClicked(int itemId) {
        mUserInteracted = true;
        switch (itemId) {
            case TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE:
                recordInfobarAction(INFOBAR_MORE_LANGUAGES);
                initMenuHelper(TranslateMenu.MENU_TARGET_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_TARGET_LANGUAGE);
                return;
            case TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE:
                // Only show snackbar when "Always Translate" is enabled.
                if (!mOptions.alwaysTranslateLanguageState()) {
                    recordInfobarAction(INFOBAR_ALWAYS_TRANSLATE);
                    recordInfobarLanguageData(INFOBAR_HISTOGRAM_ALWAYS_TRANSLATE_LANGUAGE,
                            mOptions.sourceLanguageCode());
                    createAndShowSnackbar(
                            getContext().getString(R.string.translate_snackbar_always_translate,
                                    mOptions.sourceLanguageName(), mOptions.targetLanguageName()),
                            Snackbar.UMA_TRANSLATE_ALWAYS, ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                } else {
                    recordInfobarAction(INFOBAR_ALWAYS_TRANSLATE_UNDO);
                    handleTranslateOptionPostSnackbar(ACTION_OVERFLOW_ALWAYS_TRANSLATE);
                }
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE:
                recordInfobarAction(INFOBAR_NEVER_TRANSLATE);
                recordInfobarLanguageData(
                        INFOBAR_HISTOGRAM_NEVER_TRANSLATE_LANGUAGE, mOptions.sourceLanguageCode());
                createAndShowSnackbar(
                        getContext().getString(R.string.translate_snackbar_language_never,
                                mOptions.sourceLanguageName()),
                        Snackbar.UMA_TRANSLATE_NEVER, ACTION_OVERFLOW_NEVER_LANGUAGE);
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_SITE:
                recordInfobarAction(INFOBAR_NEVER_TRANSLATE_SITE);
                createAndShowSnackbar(
                        getContext().getString(R.string.translate_snackbar_site_never),
                        Snackbar.UMA_TRANSLATE_NEVER_SITE, ACTION_OVERFLOW_NEVER_SITE);
                return;
            case TranslateMenu.ID_OVERFLOW_NOT_THIS_LANGUAGE:
                recordInfobarAction(INFOBAR_PAGE_NOT_IN);
                initMenuHelper(TranslateMenu.MENU_SOURCE_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_SOURCE_LANGUAGE);
                return;
            default:
                assert false : "Unexpected overflow menu code";
        }
    }

    @Override
    public void onTargetMenuItemClicked(String code) {
        // Reset target code in both UI and native.
        if (mOptions.setTargetLanguage(code)) {
            recordInfobarLanguageData(
                    INFOBAR_HISTOGRAM_MORE_LANGUAGES_LANGUAGE, mOptions.targetLanguageCode());
            nativeApplyStringTranslateOption(
                    mNativeTranslateInfoBarPtr, TranslateOption.TARGET_CODE, code);
            // Adjust UI.
            mTabLayout.replaceTabTitle(TARGET_TAB_INDEX, mOptions.getRepresentationFromCode(code));
            startTranslating(mTabLayout.getSelectedTabPosition());
        }
    }

    @Override
    public void onSourceMenuItemClicked(String code) {
        // Reset source code in both UI and native.
        if (mOptions.setSourceLanguage(code)) {
            recordInfobarLanguageData(
                    INFOBAR_HISTOGRAM_PAGE_NOT_IN_LANGUAGE, mOptions.sourceLanguageCode());
            nativeApplyStringTranslateOption(
                    mNativeTranslateInfoBarPtr, TranslateOption.SOURCE_CODE, code);
            // Adjust UI.
            mTabLayout.replaceTabTitle(SOURCE_TAB_INDEX, mOptions.getRepresentationFromCode(code));
            startTranslating(mTabLayout.getSelectedTabPosition());
        }
    }

    @Override
    protected void onStartedHiding() {
        if (mOverflowMenuHelper != null) mOverflowMenuHelper.dismiss();
        if (mLanguageMenuHelper != null) mLanguageMenuHelper.dismiss();
        if (getSnackbarManager() != null) getSnackbarManager().dismissAllSnackbars();
        super.onStartedHiding();
    }

    private void createAndShowSnackbar(String title, int umaType, int actionId) {
        if (getSnackbarManager() == null) {
            // Directly apply menu option, if snackbar system is not working.
            handleTranslateOptionPostSnackbar(actionId);
            return;
        }
        switch (actionId) {
            case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
                recordInfobarAction(INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION);
                break;
            case ACTION_AUTO_ALWAYS_TRANSLATE:
                recordInfobarAction(INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION);
                break;
            case ACTION_OVERFLOW_NEVER_LANGUAGE:
                recordInfobarAction(INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION);
                break;
            case ACTION_AUTO_NEVER_LANGUAGE:
                recordInfobarAction(INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION);
                break;
            case ACTION_OVERFLOW_NEVER_SITE:
                recordInfobarAction(INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION);
                break;
            default:
                assert false : "Unsupported Menu Item Id, to show snackbar.";
        }

        getSnackbarManager().showSnackbar(
                Snackbar.make(title, new TranslateSnackbarController(actionId),
                                Snackbar.TYPE_NOTIFICATION, umaType)
                        .setSingleLine(false)
                        .setAction(
                                getContext().getString(R.string.translate_snackbar_cancel), null));
    }

    private void handleTranslateOptionPostSnackbar(int actionId) {
        switch (actionId) {
            case ACTION_OVERFLOW_ALWAYS_TRANSLATE:
            case ACTION_AUTO_ALWAYS_TRANSLATE:
                mOptions.toggleAlwaysTranslateLanguageState(
                        !mOptions.alwaysTranslateLanguageState());
                nativeApplyBoolTranslateOption(mNativeTranslateInfoBarPtr,
                        TranslateOption.ALWAYS_TRANSLATE, mOptions.alwaysTranslateLanguageState());
                return;
            case ACTION_OVERFLOW_NEVER_LANGUAGE:
            case ACTION_AUTO_NEVER_LANGUAGE:
                // After applying this option, the infobar will dismiss.
                nativeApplyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateOption.NEVER_TRANSLATE, true);
                return;
            case ACTION_OVERFLOW_NEVER_SITE:
                // After applying this option, the infobar will dismiss.
                nativeApplyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateOption.NEVER_TRANSLATE_SITE, true);
                return;
            default:
                assert false : "Unsupported Menu Item Id, in handle post snackbar";
        }
    }

    private static void recordInfobarAction(int action) {
        RecordHistogram.recordEnumeratedHistogram(
                INFOBAR_HISTOGRAM, action, INFOBAR_HISTOGRAM_BOUNDARY);
    }

    private void recordInfobarLanguageData(String histogram, String langCode) {
        Integer hashCode = mOptions.getUMAHashCodeFromCode(langCode);
        if (hashCode != null) {
            RecordHistogram.recordSparseSlowlyHistogram(histogram, hashCode);
        }
    }

    private void incrementAndRecordTranslationsPerPageCount() {
        RecordHistogram.recordCountHistogram(
                INFOBAR_HISTOGRAM_TRANSLATION_COUNT, ++mTotalTranslationCount);
    }

    private native void nativeApplyStringTranslateOption(
            long nativeTranslateCompactInfoBar, int option, String value);
    private native void nativeApplyBoolTranslateOption(
            long nativeTranslateCompactInfoBar, int option, boolean value);
    private native boolean nativeShouldAutoNeverTranslate(long nativeTranslateCompactInfoBar);
}
