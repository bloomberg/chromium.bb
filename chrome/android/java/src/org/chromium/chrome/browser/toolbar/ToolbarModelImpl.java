// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.dom_distiller.DomDistillerServiceFactory;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarModel.ToolbarModelDelegate;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.WebContents;

/**
 * Contains the data and state for the toolbar.
 */
class ToolbarModelImpl extends ToolbarModel implements ToolbarDataProvider, ToolbarModelDelegate {
    private final BottomSheet mBottomSheet;
    private Tab mTab;
    private boolean mIsIncognito;
    private int mPrimaryColor;
    private boolean mIsUsingBrandColor;

    /**
     * Default constructor for this class.
     */
    public ToolbarModelImpl(@Nullable BottomSheet bottomSheet) {
        super();
        mBottomSheet = bottomSheet;
        mPrimaryColor = ApiCompatibilityUtils.getColor(
                ContextUtils.getApplicationContext().getResources(),
                R.color.default_primary_color);
    }

    /**
     * Handle any initialization that must occur after native has been initialized.
     */
    public void initializeWithNative() {
        initialize(this);
    }

    @Override
    public WebContents getActiveWebContents() {
        Tab tab = getTab();
        if (tab == null) return null;
        return tab.getWebContents();
    }

    /**
     * Sets the tab that contains the information to be displayed in the toolbar.
     * @param tab The tab associated currently with the toolbar.
     * @param isIncognito Whether the incognito model is currently selected, which must match the
     *                    passed in tab if non-null.
     */
    public void setTab(Tab tab, boolean isIncognito) {
        mTab = tab;
        if (mTab != null) {
            assert mTab.isIncognito() == isIncognito;
        }
        mIsIncognito = isIncognito;
    }

    @Override
    public Tab getTab() {
        // TODO(dtrainor, tedchoc): Remove the isInitialized() check when we no longer wait for
        // TAB_CLOSED events to remove this tab.  Otherwise there is a chance we use this tab after
        // {@link ChromeTab#destroy()} is called.
        if (mBottomSheet != null && mBottomSheet.isShowingNewTab()) {
            return null;
        }
        return (mTab == null || !mTab.isInitialized()) ? null : mTab;
    }

    @Override
    public String getCurrentUrl() {
        // TODO(yusufo) : Consider using this for all calls from getTab() for accessing url.
        return getTab() != null ? getTab().getUrl() : null;
    }

    @Override
    public NewTabPage getNewTabPageForCurrentTab() {
        Tab currentTab = getTab();
        if (currentTab != null && currentTab.getNativePage() instanceof NewTabPage) {
            return (NewTabPage) currentTab.getNativePage();
        }
        return null;
    }

    @Override
    public String getText() {
        String displayText = super.getText();

        if (mTab == null || mTab.isFrozen()) return displayText;

        String url = mTab.getUrl().trim();
        if (DomDistillerUrlUtils.isDistilledPage(url)) {
            if (isStoredArticle(url)) {
                DomDistillerService domDistillerService =
                        DomDistillerServiceFactory.getForProfile(mTab.getProfile());
                String originalUrl = domDistillerService.getUrlForEntry(
                        DomDistillerUrlUtils.getValueForKeyInUrl(url, "entry_id"));
                displayText =
                        DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl);
            } else if (DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url) != null) {
                String originalUrl = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
                displayText =
                        DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl);
            }
        } else if (OfflinePageUtils.isOfflinePage(mTab)) {
            String originalUrl = mTab.getOriginalUrl();
            displayText = OfflinePageUtils.stripSchemeFromOnlineUrl(
                  DomDistillerTabUtils.getFormattedUrlFromOriginalDistillerUrl(originalUrl));
        }

        return displayText;
    }

    private boolean isStoredArticle(String url) {
        DomDistillerService domDistillerService =
                DomDistillerServiceFactory.getForProfile(mTab.getProfile());
        String entryIdFromUrl = DomDistillerUrlUtils.getValueForKeyInUrl(url, "entry_id");
        if (TextUtils.isEmpty(entryIdFromUrl)) return false;
        return domDistillerService.hasEntry(entryIdFromUrl);
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public Profile getProfile() {
        Profile lastUsedProfile = Profile.getLastUsedProfile();
        if (mIsIncognito) {
            assert lastUsedProfile.hasOffTheRecordProfile();
            return lastUsedProfile.getOffTheRecordProfile();
        }
        return lastUsedProfile.getOriginalProfile();
    }

    /**
     * Sets the primary color and changes the state for isUsingBrandColor.
     * @param color The primary color for the current tab.
     */
    public void setPrimaryColor(int color) {
        mPrimaryColor = color;
        Context context = ContextUtils.getApplicationContext();
        mIsUsingBrandColor = !isIncognito()
                && mPrimaryColor != ApiCompatibilityUtils.getColor(context.getResources(),
                        R.color.default_primary_color)
                && getTab() != null && !getTab().isNativePage();
    }

    @Override
    public int getPrimaryColor() {
        if (mBottomSheet != null) {
            int colorId =
                    isIncognito() ? R.color.incognito_primary_color : R.color.default_primary_color;
            return ApiCompatibilityUtils.getColor(
                    ContextUtils.getApplicationContext().getResources(), colorId);
        }
        return mPrimaryColor;
    }

    @Override
    public boolean isUsingBrandColor() {
        return mIsUsingBrandColor && mBottomSheet == null;
    }
}
