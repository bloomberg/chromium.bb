// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.res.Resources;

import com.google.android.libraries.feed.api.stream.ContentChangedListener;
import com.google.android.libraries.feed.api.stream.ScrollListener;
import com.google.android.libraries.feed.api.stream.Stream;

import org.chromium.base.MemoryPressureListener;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

/**
 * A mediator for the {@link FeedNewTabPage} responsible for interacting with the
 * native library and handling business logic.
 */
class FeedNewTabPageMediator implements NewTabPageLayout.ScrollDelegate {
    private final FeedNewTabPage mCoordinator;
    private final SnapScrollHelper mSnapScrollHelper;
    private final PrefChangeRegistrar mPrefChangeRegistrar;

    private ScrollListener mStreamScrollListener;
    private ContentChangedListener mStreamContentChangedListener;
    private SectionHeader mSectionHeader;
    private MemoryPressureCallback mMemoryPressureCallback;

    private boolean mStreamContentChanged;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;

    /**
     * @param feedNewTabPage The {@link FeedNewTabPage} that interacts with this class.
     */
    FeedNewTabPageMediator(FeedNewTabPage feedNewTabPage, SnapScrollHelper snapScrollHelper) {
        mCoordinator = feedNewTabPage;
        mSnapScrollHelper = snapScrollHelper;
        initializeProperties();

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mPrefChangeRegistrar.addObserver(
                Pref.NTP_ARTICLES_LIST_VISIBLE, this::updateSectionHeader);
    }

    /** Clears any dependencies. */
    void destroy() {
        Stream stream = mCoordinator.getStream();
        stream.removeScrollListener(mStreamScrollListener);
        stream.removeOnContentChangedListener(mStreamContentChangedListener);
        mPrefChangeRegistrar.destroy();
        MemoryPressureListener.removeCallback(mMemoryPressureCallback);
    }

    /**
     * Initialize properties for UI components in the {@link FeedNewTabPage}.
     * TODO(huayinz): Introduce a Model for these properties.
     */
    private void initializeProperties() {
        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        mCoordinator.getView().addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    mSnapScrollHelper.handleScroll();
                });

        Stream stream = mCoordinator.getStream();
        mStreamScrollListener = new ScrollListener() {
            @Override
            public void onScrollStateChanged(int state) {}

            @Override
            public void onScrolled(int dx, int dy) {
                mSnapScrollHelper.handleScroll();
            }
        };
        stream.addScrollListener(mStreamScrollListener);

        mStreamContentChangedListener = () -> mStreamContentChanged = true;
        stream.addOnContentChangedListener(mStreamContentChangedListener);

        Resources res = mCoordinator.getSectionHeaderView().getResources();
        mSectionHeader =
                new SectionHeader(res.getString(R.string.ntp_article_suggestions_section_header),
                        PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE),
                        this::onSectionHeaderToggled);
        mCoordinator.getSectionHeaderView().setHeader(mSectionHeader);
        stream.setStreamContentVisibility(mSectionHeader.isExpanded());

        mMemoryPressureCallback = pressure -> mCoordinator.getStream().trim();
        MemoryPressureListener.addCallback(mMemoryPressureCallback);
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

    /** Whether a new thumbnail should be captured. */
    boolean shouldCaptureThumbnail() {
        return mStreamContentChanged || mCoordinator.getView().getWidth() != mThumbnailWidth
                || mCoordinator.getView().getHeight() != mThumbnailHeight
                || getVerticalScrollOffset() != mThumbnailScrollY;
    }

    /** Reset all the properties for thumbnail capturing after a new thumbnail is captured. */
    void onThumbnailCaptured() {
        mThumbnailWidth = mCoordinator.getView().getWidth();
        mThumbnailHeight = mCoordinator.getView().getHeight();
        mThumbnailScrollY = getVerticalScrollOffset();
        mStreamContentChanged = false;
    }

    // ScrollDelegate interface.

    @Override
    public boolean isScrollViewInitialized() {
        Stream stream = mCoordinator.getStream();
        // During startup the view may not be fully initialized, so we check to see if some basic
        // view properties (height of the RecyclerView) are sane.
        return stream != null && stream.getView().getHeight() > 0;
    }

    @Override
    public int getVerticalScrollOffset() {
        // This method returns a valid vertical scroll offset only when the first header view in the
        // Stream is visible. In all other cases, this returns 0.
        if (!isScrollViewInitialized()) return 0;

        int firstChildTop = mCoordinator.getStream().getChildTopAt(0);
        return firstChildTop != Stream.POSITION_NOT_KNOWN ? -firstChildTop : 0;
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        return isScrollViewInitialized()
                && mCoordinator.getStream().isChildAtPositionVisible(position);
    }

    @Override
    public void snapScroll() {
        if (!isScrollViewInitialized()) return;

        int initialScroll = getVerticalScrollOffset();
        int scrollTo = mSnapScrollHelper.calculateSnapPosition(initialScroll);

        // Calculating the snap position should be idempotent.
        assert scrollTo == mSnapScrollHelper.calculateSnapPosition(scrollTo);
        mCoordinator.getStream().smoothScrollBy(0, scrollTo - initialScroll);
    }
}
