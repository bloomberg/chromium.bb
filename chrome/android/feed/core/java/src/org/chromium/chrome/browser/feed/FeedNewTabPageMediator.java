// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.res.Resources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

/**
 * A mediator for the {@link FeedNewTabPage} responsible for interacting with the
 * native library and handling business logic.
 */
class FeedNewTabPageMediator {
    private final FeedNewTabPage mCoordinator;
    private final PrefChangeRegistrar mPrefChangeRegistrar;

    private SectionHeader mSectionHeader;

    /**
     * @param feedNewTabPage The {@link FeedNewTabPage} that interacts with this class.
     */
    FeedNewTabPageMediator(FeedNewTabPage feedNewTabPage) {
        mCoordinator = feedNewTabPage;
        initializeProperties();

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mPrefChangeRegistrar.addObserver(
                Pref.NTP_ARTICLES_LIST_VISIBLE, this::updateSectionHeader);
    }

    /** Clears any dependencies. */
    void destroy() {
        mPrefChangeRegistrar.destroy();
    }

    /**
     * Initialize properties for UI components in the {@link FeedNewTabPage}.
     * TODO(huayinz): Introduce a Model for these properties.
     */
    private void initializeProperties() {
        Resources res = mCoordinator.getSectionHeaderView().getResources();
        mSectionHeader =
                new SectionHeader(res.getString(R.string.ntp_article_suggestions_section_header),
                        PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE),
                        this::onSectionHeaderToggled);
        mCoordinator.getSectionHeaderView().setHeader(mSectionHeader);
        mCoordinator.getStream().setStreamContentVisibility(mSectionHeader.isExpanded());
    }

    /** Update whether the section header should be expanded. */
    private void updateSectionHeader() {
        if (mSectionHeader.isExpanded()
                != PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE)) {
            mSectionHeader.toggleHeader();
        }
    }

    /**
     * Callback on section header toggled. This will update the visibility of the Feed and the
     * expand icon on the section header view.
     */
    private void onSectionHeaderToggled() {
        PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_LIST_VISIBLE, mSectionHeader.isExpanded());
        mCoordinator.getStream().setStreamContentVisibility(mSectionHeader.isExpanded());
        // TODO(huayinz): Update the section header view through a ModelChangeProcessor.
        mCoordinator.getSectionHeaderView().updateIconDrawable();
    }
}
