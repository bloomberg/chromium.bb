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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.translate.TranslateMenu;
import org.chromium.chrome.browser.infobar.translate.TranslateMenuHelper;
import org.chromium.chrome.browser.infobar.translate.TranslateTabLayout;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.ui.widget.Toast;

/**
 * Java version of the compcat translate infobar
 */
class TranslateCompactInfoBar extends InfoBar
        implements TabLayout.OnTabSelectedListener, TranslateMenuHelper.TranslateMenuListener {
    public static final int TRANSLATING_INFOBAR = 1;

    private static final int SOURCE_TAB_INDEX = 0;
    private static final int TARGET_TAB_INDEX = 1;

    private final int mInitialStep;
    private final TranslateOptions mOptions;

    private long mNativeTranslateInfoBarPtr;
    private TranslateTabLayout mTabLayout;

    // Need 2 instances of TranslateMenuHelper to prevent a race condition bug which happens when
    // showing language menu after dismissing overflow menu.
    private TranslateMenuHelper mOverflowMenuHelper;
    private TranslateMenuHelper mLanguageMenuHelper;

    private TintedImageButton mMenuButton;

    @CalledByNative
    private static InfoBar create(int initialStep, String sourceLanguageCode,
            String targetLanguageCode, boolean alwaysTranslate, boolean triggeredFromMenu,
            String[] languages, String[] codes) {
        return new TranslateCompactInfoBar(initialStep, sourceLanguageCode, targetLanguageCode,
                alwaysTranslate, triggeredFromMenu, languages, codes);
    }

    TranslateCompactInfoBar(int initialStep, String sourceLanguageCode, String targetLanguageCode,
            boolean alwaysTranslate, boolean triggeredFromMenu, String[] languages,
            String[] codes) {
        super(R.drawable.infobar_translate_compact, null, null);
        mInitialStep = initialStep;
        mOptions = TranslateOptions.create(sourceLanguageCode, targetLanguageCode, languages, codes,
                alwaysTranslate, triggeredFromMenu);
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
        }

        mTabLayout.addOnTabSelectedListener(this);

        mMenuButton = (TintedImageButton) content.findViewById(R.id.translate_infobar_menu_button);
        mMenuButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
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

    private void startTranslating(int tabPostion) {
        if (TARGET_TAB_INDEX == tabPostion) {
            // Already on the target tab.
            mTabLayout.showProgressBarOnTab(TARGET_TAB_INDEX);
            onButtonClicked(ActionType.TRANSLATE);
        } else {
            mTabLayout.getTabAt(TARGET_TAB_INDEX).select();
        }
    }

    @CalledByNative
    private void onPageTranslated(int errorType) {
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

    @Override
    protected void onNativeDestroyed() {
        mNativeTranslateInfoBarPtr = 0;
        super.onNativeDestroyed();
    }

    @Override
    public void onTabSelected(TabLayout.Tab tab) {
        switch (tab.getPosition()) {
            case SOURCE_TAB_INDEX:
                onButtonClicked(ActionType.TRANSLATE_SHOW_ORIGINAL);
                return;
            case TARGET_TAB_INDEX:
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
        switch (itemId) {
            case TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE:
                initMenuHelper(TranslateMenu.MENU_TARGET_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_TARGET_LANGUAGE);
                return;
            case TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE:
                nativeApplyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateOption.ALWAYS_TRANSLATE, true);
                showSnackbar(TranslateSnackbarType.ALWAYS_TRANSLATE);
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE:
                nativeApplyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateOption.NEVER_TRANSLATE, true);
                showSnackbar(TranslateSnackbarType.NEVER_TRANSLATE);
                return;
            case TranslateMenu.ID_OVERFLOW_NEVER_SITE:
                nativeApplyBoolTranslateOption(
                        mNativeTranslateInfoBarPtr, TranslateOption.NEVER_TRANSLATE_SITE, true);
                showSnackbar(TranslateSnackbarType.NEVER_TRANSLATE_SITE);
                return;
            case TranslateMenu.ID_OVERFLOW_NOT_THIS_LANGUAGE:
                initMenuHelper(TranslateMenu.MENU_SOURCE_LANGUAGE);
                mLanguageMenuHelper.show(TranslateMenu.MENU_SOURCE_LANGUAGE);
                return;
            default:
                assert false : "Unexpected overflow menu code";
        }
    }

    private void showSnackbar(int snackbarType) {
        // TODO(ramyasharma): Show snackbar.
    }

    @Override
    public void onTargetMenuItemClicked(String code) {
        // Reset target code in both UI and native.
        if (mOptions.setTargetLanguage(code)) {
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
            nativeApplyStringTranslateOption(
                    mNativeTranslateInfoBarPtr, TranslateOption.SOURCE_CODE, code);
            // Adjust UI.
            mTabLayout.replaceTabTitle(SOURCE_TAB_INDEX, mOptions.getRepresentationFromCode(code));
            startTranslating(mTabLayout.getSelectedTabPosition());
        }
    }

    private native void nativeApplyStringTranslateOption(
            long nativeTranslateCompactInfoBar, int option, String value);
    private native void nativeApplyBoolTranslateOption(
            long nativeTranslateCompactInfoBar, int option, boolean value);
}
