// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.annotation.SuppressLint;
import android.graphics.Canvas;
import android.support.annotation.StringRes;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;

/**
 * A class that handles merging above the fold elements and below the fold cards into an adapter
 * that will be used to back the NTP RecyclerView. The first element in the adapter should always be
 * the above-the-fold view (containing the logo, search box, and most visited tiles) and subsequent
 * elements will be the cards shown to the user
 */
public class NewTabPageAdapter extends Adapter<NewTabPageViewHolder> implements NodeParent {
    private static final String TAG = "Ntp";

    private final NewTabPageManager mNewTabPageManager;
    private final View mAboveTheFoldView;
    private final UiConfig mUiConfig;
    private final ItemTouchCallbacks mItemTouchCallbacks = new ItemTouchCallbacks();
    private NewTabPageRecyclerView mRecyclerView;

    private final InnerNode mRoot;

    private final AboveTheFoldItem mAboveTheFold = new AboveTheFoldItem();
    private final SectionList mSections;
    private final SignInPromo mSigninPromo;
    private final AllDismissedItem mAllDismissed;
    private final Footer mFooter;
    private final SpacingItem mBottomSpacer = new SpacingItem();

    private class ItemTouchCallbacks extends ItemTouchHelper.Callback {
        @Override
        public void onSwiped(ViewHolder viewHolder, int direction) {
            mRecyclerView.onItemDismissStarted(viewHolder);
            NewTabPageAdapter.this.dismissItem(viewHolder.getAdapterPosition());
        }

        @Override
        public void clearView(RecyclerView recyclerView, ViewHolder viewHolder) {
            // clearView() is called when an interaction with the item is finished, which does
            // not mean that the user went all the way and dismissed the item before releasing it.
            // We need to check that the item has been removed.
            if (viewHolder.getAdapterPosition() == RecyclerView.NO_POSITION) {
                mRecyclerView.onItemDismissFinished(viewHolder);
            }

            super.clearView(recyclerView, viewHolder);
        }

        @Override
        public boolean onMove(RecyclerView recyclerView, ViewHolder viewHolder, ViewHolder target) {
            assert false; // Drag and drop not supported, the method will never be called.
            return false;
        }

        @Override
        public int getMovementFlags(RecyclerView recyclerView, ViewHolder viewHolder) {
            assert viewHolder instanceof NewTabPageViewHolder;

            int swipeFlags = 0;
            if (((NewTabPageViewHolder) viewHolder).isDismissable()) {
                swipeFlags = ItemTouchHelper.START | ItemTouchHelper.END;
            }

            return makeMovementFlags(0 /* dragFlags */, swipeFlags);
        }

        @Override
        public void onChildDraw(Canvas c, RecyclerView recyclerView, ViewHolder viewHolder,
                float dX, float dY, int actionState, boolean isCurrentlyActive) {
            assert viewHolder instanceof NewTabPageViewHolder;

            // The item has already been removed. We have nothing more to do.
            // In some cases a removed children may call this method when unrelated items are
            // interacted with, but this check also covers the case.
            // See https://crbug.com/664466, b/32900699
            if (viewHolder.getAdapterPosition() == RecyclerView.NO_POSITION) return;

            // We use our own implementation of the dismissal animation, so we don't call the
            // parent implementation. (by default it changes the translation-X and elevation)
            mRecyclerView.updateViewStateForDismiss(dX, (NewTabPageViewHolder) viewHolder);

            // If there is another item that should be animated at the same time, do the same to it.
            NewTabPageViewHolder siblingViewHolder = getDismissSibling(viewHolder);
            if (siblingViewHolder != null) {
                mRecyclerView.updateViewStateForDismiss(dX, siblingViewHolder);
            }
        }
    }

    /**
     * Creates the adapter that will manage all the cards to display on the NTP.
     *
     * @param manager the NewTabPageManager to use to interact with the rest of the system.
     * @param aboveTheFoldView the layout encapsulating all the above-the-fold elements
     *                         (logo, search box, most visited tiles)
     * @param uiConfig the NTP UI configuration, to be passed to created views.
     * @param offlinePageBridge the OfflinePageBridge used to determine if articles are available
     *                              offline.
     *
     */
    public NewTabPageAdapter(NewTabPageManager manager, View aboveTheFoldView, UiConfig uiConfig,
            OfflinePageBridge offlinePageBridge) {
        mNewTabPageManager = manager;
        mAboveTheFoldView = aboveTheFoldView;
        mUiConfig = uiConfig;
        mRoot = new InnerNode();

        mSections = new SectionList(mNewTabPageManager, offlinePageBridge);
        mSigninPromo = new SignInPromo(mNewTabPageManager);
        mAllDismissed = new AllDismissedItem();
        mFooter = new Footer();

        mRoot.addChildren(
                mAboveTheFold, mSections, mSigninPromo, mAllDismissed, mFooter, mBottomSpacer);

        updateAllDismissedVisibility();
        mRoot.setParent(this);
    }

    /** Returns callbacks to configure the interactions with the RecyclerView's items. */
    public ItemTouchHelper.Callback getItemTouchCallbacks() {
        return mItemTouchCallbacks;
    }

    @Override
    @ItemViewType
    public int getItemViewType(int position) {
        return mRoot.getItemViewType(position);
    }

    @Override
    public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        assert parent == mRecyclerView;

        switch (viewType) {
            case ItemViewType.ABOVE_THE_FOLD:
                return new NewTabPageViewHolder(mAboveTheFoldView);

            case ItemViewType.HEADER:
                return new SectionHeaderViewHolder(mRecyclerView, mUiConfig);

            case ItemViewType.SNIPPET:
                return new SnippetArticleViewHolder(mRecyclerView, mNewTabPageManager, mUiConfig);

            case ItemViewType.SPACING:
                return new NewTabPageViewHolder(SpacingItem.createView(parent));

            case ItemViewType.STATUS:
                return new StatusCardViewHolder(mRecyclerView, mNewTabPageManager, mUiConfig);

            case ItemViewType.PROGRESS:
                return new ProgressViewHolder(mRecyclerView);

            case ItemViewType.ACTION:
                return new ActionItem.ViewHolder(mRecyclerView, mNewTabPageManager, mUiConfig);

            case ItemViewType.PROMO:
                return new SignInPromo.ViewHolder(mRecyclerView, mNewTabPageManager, mUiConfig);

            case ItemViewType.FOOTER:
                return new Footer.ViewHolder(mRecyclerView, mNewTabPageManager);

            case ItemViewType.ALL_DISMISSED:
                return new AllDismissedItem.ViewHolder(mRecyclerView, mSections);
        }

        assert false : viewType;
        return null;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, final int position) {
        mRoot.onBindViewHolder(holder, position);
    }

    @Override
    public int getItemCount() {
        return mRoot.getItemCount();
    }

    public int getAboveTheFoldPosition() {
        return getChildPositionOffset(mAboveTheFold);
    }

    public int getFirstHeaderPosition() {
        return getFirstPositionForType(ItemViewType.HEADER);
    }

    public int getFirstCardPosition() {
        for (int i = 0; i < getItemCount(); ++i) {
            if (CardViewHolder.isCard(getItemViewType(i))) return i;
        }
        return RecyclerView.NO_POSITION;
    }

    int getLastContentItemPosition() {
        return getChildPositionOffset(hasAllBeenDismissed() ? mAllDismissed : mFooter);
    }

    int getBottomSpacerPosition() {
        return getChildPositionOffset(mBottomSpacer);
    }

    private void updateAllDismissedVisibility() {
        boolean showAllDismissed = hasAllBeenDismissed();
        mAllDismissed.setVisible(showAllDismissed);
        mFooter.setVisible(!showAllDismissed);
    }

    @Override
    public void onItemRangeChanged(TreeNode child, int itemPosition, int itemCount) {
        assert child == mRoot;
        notifyItemRangeChanged(itemPosition, itemCount);
    }

    @Override
    public void onItemRangeInserted(TreeNode child, int itemPosition, int itemCount) {
        assert child == mRoot;
        notifyItemRangeInserted(itemPosition, itemCount);
        mBottomSpacer.refresh();

        updateAllDismissedVisibility();
    }

    @Override
    public void onItemRangeRemoved(TreeNode child, int itemPosition, int itemCount) {
        assert child == mRoot;
        notifyItemRangeRemoved(itemPosition, itemCount);
        mBottomSpacer.refresh();

        updateAllDismissedVisibility();
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);

        // We are assuming for now that the adapter is used with a single RecyclerView.
        // Getting the reference as we are doing here is going to be broken if that changes.
        assert mRecyclerView == null;

        // FindBugs chokes on the cast below when not checked, raising BC_UNCONFIRMED_CAST
        assert recyclerView instanceof NewTabPageRecyclerView;

        mRecyclerView = (NewTabPageRecyclerView) recyclerView;
    }

    /**
     * Dismisses the item at the provided adapter position. Can also cause the dismissal of other
     * items or even entire sections.
     */
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("SwitchIntDef")
    public void dismissItem(int position) {
        int itemViewType = getItemViewType(position);

        // TODO(dgn): Polymorphism is supposed to allow to avoid that kind of stuff.
        switch (itemViewType) {
            case ItemViewType.STATUS:
            case ItemViewType.ACTION:
                dismissSection(position);
                return;

            case ItemViewType.SNIPPET:
                dismissSuggestion(position);
                return;

            case ItemViewType.PROMO:
                dismissPromo();
                return;

            default:
                Log.wtf(TAG, "Unsupported dismissal of item of type %d", itemViewType);
                return;
        }
    }

    private void dismissSection(int position) {
        SuggestionsSection section = getSuggestionsSection(position);
        mSections.dismissSection(section);
        announceItemRemoved(section.getHeaderText());
    }

    private void dismissSuggestion(int position) {
        SnippetArticle suggestion = mRoot.getSuggestionAt(position);
        SuggestionsSource suggestionsSource = mNewTabPageManager.getSuggestionsSource();
        if (suggestionsSource == null) {
            // It is possible for this method to be called after the NewTabPage has had destroy()
            // called. This can happen when NewTabPageRecyclerView.dismissWithAnimation() is called
            // and the animation ends after the user has navigated away. In this case we cannot
            // inform the native side that the snippet has been dismissed (http://crbug.com/649299).
            return;
        }

        announceItemRemoved(suggestion.mTitle);

        suggestionsSource.dismissSuggestion(suggestion);
        SuggestionsSection section = getSuggestionsSection(position);
        section.removeSuggestion(suggestion);
    }

    private void dismissPromo() {
        announceItemRemoved(mSigninPromo.getHeader());
        mSigninPromo.dismiss();
    }

    /**
     * Returns another view holder that should be dismissed at the same time as the provided one.
     */
    public NewTabPageViewHolder getDismissSibling(ViewHolder viewHolder) {
        int swipePos = viewHolder.getAdapterPosition();
        int siblingPosDelta = mRoot.getDismissSiblingPosDelta(swipePos);
        if (siblingPosDelta == 0) return null;

        return (NewTabPageViewHolder) mRecyclerView.findViewHolderForAdapterPosition(
                siblingPosDelta + swipePos);
    }

    private boolean hasAllBeenDismissed() {
        return mSections.isEmpty() && !mSigninPromo.isVisible();
    }

    /**
     * @param itemPosition The position of an item in the adapter.
     * @return Returns the {@link SuggestionsSection} that contains the item at
     *     {@code itemPosition}, or null if the item is not part of one.
     */
    private SuggestionsSection getSuggestionsSection(int itemPosition) {
        int relativePosition = itemPosition - mRoot.getStartingOffsetForChild(mSections);
        assert relativePosition >= 0;
        TreeNode child = mSections.getChildForPosition(relativePosition);
        if (!(child instanceof SuggestionsSection)) return null;
        return (SuggestionsSection) child;
    }

    private int getChildPositionOffset(TreeNode child) {
        return mRoot.getStartingOffsetForChild(child);
    }

    @VisibleForTesting
    SnippetArticle getSuggestionAt(int position) {
        return mRoot.getSuggestionAt(position);
    }

    @VisibleForTesting
    int getFirstPositionForType(@ItemViewType int viewType) {
        int count = getItemCount();
        for (int i = 0; i < count; i++) {
            if (getItemViewType(i) == viewType) return i;
        }
        return RecyclerView.NO_POSITION;
    }

    SectionList getSectionListForTesting() {
        return mSections;
    }

    InnerNode getRootForTesting() {
        return mRoot;
    }

    private void announceItemRemoved(String itemTitle) {
        // In tests the RecyclerView can be null.
        if (mRecyclerView == null) return;

        mRecyclerView.announceForAccessibility(mRecyclerView.getResources().getString(
                R.string.ntp_accessibility_item_removed, itemTitle));
    }

    private void announceItemRemoved(@StringRes int stringToAnnounce) {
        // In tests the RecyclerView can be null.
        if (mRecyclerView == null) return;

        announceItemRemoved(mRecyclerView.getResources().getString(stringToAnnounce));
    }
}
