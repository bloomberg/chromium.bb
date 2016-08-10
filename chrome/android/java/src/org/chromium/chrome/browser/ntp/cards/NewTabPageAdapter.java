// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.graphics.Canvas;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetHeaderListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetHeaderViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/**
 * A class that handles merging above the fold elements and below the fold cards into an adapter
 * that will be used to back the NTP RecyclerView. The first element in the adapter should always be
 * the above-the-fold view (containing the logo, search box, and most visited tiles) and subsequent
 * elements will be the cards shown to the user
 */
public class NewTabPageAdapter extends Adapter<NewTabPageViewHolder>
        implements SuggestionsSource.Observer {
    private static final String TAG = "Ntp";

    private final NewTabPageManager mNewTabPageManager;
    private final NewTabPageLayout mNewTabPageLayout;
    private SuggestionsSource mSuggestionsSource;
    private final UiConfig mUiConfig;
    private final ItemTouchCallbacks mItemTouchCallbacks = new ItemTouchCallbacks();
    private NewTabPageRecyclerView mRecyclerView;

    /**
     * List of all item groups (which can themselves contain multiple items. When flattened, this
     * will be a list of all items the adapter exposes.
     */
    private final List<ItemGroup> mGroups = new ArrayList<>();
    private final AboveTheFoldListItem mAboveTheFold = new AboveTheFoldListItem();
    private final SpacingListItem mBottomSpacer = new SpacingListItem();

    /** Maps suggestion categories to sections, with stable iteration ordering. */
    private final Map<Integer, SuggestionsSection> mSections = new TreeMap<>();

    private class ItemTouchCallbacks extends ItemTouchHelper.Callback {
        @Override
        public void onSwiped(ViewHolder viewHolder, int direction) {
            mRecyclerView.onItemDismissStarted(viewHolder.itemView);

            NewTabPageAdapter.this.dismissItem(viewHolder);
        }

        @Override
        public void clearView(RecyclerView recyclerView, ViewHolder viewHolder) {
            // clearView() is called when an interaction with the item is finished, which does
            // not mean that the user went all the way and dismissed the item before releasing it.
            // We need to check that the item has been removed.
            if (viewHolder.getAdapterPosition() == RecyclerView.NO_POSITION) {
                mRecyclerView.onItemDismissFinished(viewHolder.itemView);
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

            ((NewTabPageViewHolder) viewHolder).updateViewStateForDismiss(dX);
            super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);
        }
    }

    /**
     * Constructor to create the manager for all the cards to display on the NTP
     *
     * @param manager the NewTabPageManager to use to interact with the rest of the system.
     * @param newTabPageLayout the layout encapsulating all the above-the-fold elements
     *                         (logo, search box, most visited tiles)
     * @param suggestionsSource the bridge to interact with the content suggestions service.
     * @param uiConfig the NTP UI configuration, to be passed to created views.
     */
    public NewTabPageAdapter(NewTabPageManager manager, NewTabPageLayout newTabPageLayout,
            SuggestionsSource suggestionsSource, UiConfig uiConfig) {
        mNewTabPageManager = manager;
        mNewTabPageLayout = newTabPageLayout;
        mSuggestionsSource = suggestionsSource;
        mUiConfig = uiConfig;

        for (int category : mSuggestionsSource.getCategories()) {
            setSuggestions(category, suggestionsSource.getSuggestionsForCategory(category),
                    suggestionsSource.getCategoryStatus(category));
        }
        suggestionsSource.setObserver(this);
        updateGroups();
    }

    /** Returns callbacks to configure the interactions with the RecyclerView's items. */
    public ItemTouchHelper.Callback getItemTouchCallbacks() {
        return mItemTouchCallbacks;
    }

    @Override
    public void onNewSuggestions(int category) {
        // We never want to refresh the suggestions if we already have some content.
        if (mSections.containsKey(category) && mSections.get(category).hasSuggestions()) return;

        // The status may have changed while the suggestions were loading, perhaps they should not
        // be displayed any more.
        if (!SnippetsBridge.isCategoryStatusInitOrAvailable(
                    mSuggestionsSource.getCategoryStatus(category))) {
            return;
        }

        List<SnippetArticleListItem> suggestions =
                mSuggestionsSource.getSuggestionsForCategory(category);

        Log.d(TAG, "Received %d new suggestions for category %d.", suggestions.size(), category);

        // At first, there might be no suggestions available, we wait until they have been fetched.
        if (suggestions.isEmpty()) return;

        setSuggestions(category, suggestions, mSuggestionsSource.getCategoryStatus(category));
        updateGroups();

        NewTabPageUma.recordSnippetAction(NewTabPageUma.SNIPPETS_ACTION_SHOWN);
    }

    @Override
    public void onCategoryStatusChanged(int category, @CategoryStatusEnum int status) {
        // Observers should not be registered for this state.
        assert status != CategoryStatus.ALL_SUGGESTIONS_EXPLICITLY_DISABLED;

        // If there is no section for this category there is nothing to do.
        if (!mSections.containsKey(category)) return;

        SuggestionsSection section = mSections.get(category);

        // The section already has suggestions but we just got notified about the provider being
        // enabled. Nothing to do.
        if (SnippetsBridge.isCategoryStatusAvailable(status) && section.hasSuggestions()) return;

        setSuggestions(category, Collections.<SnippetArticleListItem>emptyList(), status);
        updateGroups();
    }

    @Override
    @NewTabPageListItem.ViewType
    public int getItemViewType(int position) {
        return getItems().get(position).getType();
    }

    @Override
    public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        assert parent == mRecyclerView;

        if (viewType == NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD) {
            return new NewTabPageViewHolder(mNewTabPageLayout);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_HEADER) {
            return new SnippetHeaderViewHolder(mRecyclerView, mUiConfig);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_SNIPPET) {
            return new SnippetArticleViewHolder(
                    mRecyclerView, mNewTabPageManager, mSuggestionsSource, mUiConfig);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_SPACING) {
            return new NewTabPageViewHolder(SpacingListItem.createView(parent));
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_STATUS) {
            return new StatusListItem.ViewHolder(mRecyclerView, mUiConfig);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_PROGRESS) {
            return new ProgressViewHolder(mRecyclerView);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_ACTION) {
            return new ActionListItem.ViewHolder(mRecyclerView, mNewTabPageManager, mUiConfig);
        }

        return null;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, final int position) {
        holder.onBindViewHolder(getItems().get(position));
    }

    @Override
    public int getItemCount() {
        return getItems().size();
    }

    public int getAboveTheFoldPosition() {
        return getGroupPositionOffset(mAboveTheFold);
    }

    public int getFirstHeaderPosition() {
        List<NewTabPageListItem> items = getItems();
        for (int i = 0; i < items.size(); i++) {
            if (items.get(i) instanceof SnippetHeaderListItem) return i;
        }
        return RecyclerView.NO_POSITION;
    }

    public int getFirstCardPosition() {
        // TODO(mvanouwerkerk): Don't rely on getFirstHeaderPosition() here.
        int firstHeaderPosition = getFirstHeaderPosition();
        if (firstHeaderPosition == RecyclerView.NO_POSITION) return RecyclerView.NO_POSITION;
        return firstHeaderPosition + 1;
    }

    public int getLastContentItemPosition() {
        // TODO(mvanouwerkerk): Don't rely on getBottomSpacerPosition() here.
        int bottomSpacerPosition = getBottomSpacerPosition();
        if (bottomSpacerPosition == RecyclerView.NO_POSITION) return RecyclerView.NO_POSITION;
        return bottomSpacerPosition - 1;
    }

    public int getBottomSpacerPosition() {
        return getGroupPositionOffset(mBottomSpacer);
    }

    /** Start a request for new snippets. */
    public void reloadSnippets() {
        SnippetsBridge.fetchSnippets(/*forceRequest=*/true);
    }

    private void setSuggestions(int category, List<SnippetArticleListItem> suggestions,
            @CategoryStatusEnum int status) {
        if (!mSections.containsKey(category)) {
            SuggestionsCategoryInfo info = mSuggestionsSource.getCategoryInfo(category);
            mSections.put(category, new SuggestionsSection(suggestions, status, info, this));
        } else {
            mSections.get(category).setSuggestions(suggestions, status, this);
        }
    }

    private void updateGroups() {
        mGroups.clear();
        mGroups.add(mAboveTheFold);
        mGroups.addAll(mSections.values());
        if (!mSections.isEmpty()) {
            mGroups.add(mBottomSpacer);
        }

        // TODO(bauerb): Notify about a smaller range.
        notifyDataSetChanged();
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

    public void dismissItem(ViewHolder itemViewHolder) {
        assert itemViewHolder.getItemViewType() == NewTabPageListItem.VIEW_TYPE_SNIPPET;

        int position = itemViewHolder.getAdapterPosition();
        SnippetArticleListItem suggestion = (SnippetArticleListItem) getItems().get(position);

        mSuggestionsSource.getSuggestionVisited(suggestion, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                NewTabPageUma.recordSnippetAction(result
                                ? NewTabPageUma.SNIPPETS_ACTION_DISMISSED_VISITED
                                : NewTabPageUma.SNIPPETS_ACTION_DISMISSED_UNVISITED);
            }
        });

        mSuggestionsSource.dismissSuggestion(suggestion);
        SuggestionsSection section = (SuggestionsSection) getGroup(position);
        section.dismissSuggestion(suggestion);

        if (section.hasSuggestions()) {
            // If one of many suggestions was dismissed, it's a simple item removal, which can be
            // animated smoothly by the RecyclerView.
            notifyItemRemoved(position);
        } else {
            // If the last suggestion was dismissed, multiple items will have changed, so mark
            // everything as changed.
            notifyDataSetChanged();
        }
    }

    /**
     * Returns an unmodifiable list containing all items in the adapter.
     */
    List<NewTabPageListItem> getItems() {
        List<NewTabPageListItem> items = new ArrayList<>();
        for (ItemGroup group : mGroups) {
            items.addAll(group.getItems());
        }
        return Collections.unmodifiableList(items);
    }

    private ItemGroup getGroup(int itemPosition) {
        int itemsSkipped = 0;
        for (ItemGroup group : mGroups) {
            List<NewTabPageListItem> items = group.getItems();
            itemsSkipped += items.size();
            if (itemPosition < itemsSkipped) return group;
        }
        return null;
    }

    private int getGroupPositionOffset(ItemGroup group) {
        int positionOffset = 0;
        for (ItemGroup candidateGroup : mGroups) {
            if (candidateGroup == group) return positionOffset;
            positionOffset += candidateGroup.getItems().size();
        }
        return RecyclerView.NO_POSITION;
    }
}
