// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.browser.widget.prefeditor.EditableOption;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the generic superclass for all autofill-assistant payment request sections.
 *
 * @param <T> The type of |EditableOption| that a concrete instance of this class is created for,
 * such as |AutofillContact|, |AutofillPaymentMethod|, etc.
 */
public abstract class AssistantPaymentRequestSection<T extends EditableOption> {
    private final View mTitleAddButton;
    private final AssistantVerticalExpander mSectionExpander;
    private final AssistantChoiceList mItemsView;
    private final View mSummaryView;
    private final int mFullViewResId;
    private final List<Item> mItems;

    protected final Context mContext;
    protected T mSelectedOption;

    private boolean mIgnoreItemSelectedNotifications;
    private Callback<T> mListener;

    private class Item {
        Item(View fullView, T option) {
            this.mFullView = fullView;
            this.mOption = option;
        }
        View mFullView;
        T mOption;
    }

    /**
     *
     * @param context The context to use.
     * @param parent The parent view to add this payment request section to.
     * @param summaryViewResId The resource ID of the summary view to inflate.
     * @param fullViewResId The resource ID of the full view to inflate.
     * @param titleToContentPadding The amount of padding between title and content views.
     * @param title The title string to display.
     * @param titleAddButton The string to display in the title add button.
     * @param listAddButton The string to display in the add button at the bottom of the list.
     */
    public AssistantPaymentRequestSection(Context context, ViewGroup parent, int summaryViewResId,
            int fullViewResId, int titleToContentPadding, String title, String titleAddButton,
            String listAddButton) {
        mContext = context;
        mFullViewResId = fullViewResId;
        mItems = new ArrayList<>();

        LayoutInflater inflater = LayoutInflater.from(context);
        mSectionExpander = new AssistantVerticalExpander(context, null);
        View sectionTitle =
                inflater.inflate(R.layout.autofill_assistant_payment_request_section_title, null);
        mSummaryView = inflater.inflate(summaryViewResId, null);
        mItemsView = (AssistantChoiceList) inflater.inflate(
                R.layout.autofill_assistant_payment_request_choice_list, null);
        mItemsView.setAddButtonLabel(listAddButton);

        mSectionExpander.setTitleView(sectionTitle,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setCollapsedView(mSummaryView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setExpandedView(mItemsView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // Adjust margins such that title and collapsed views are indented, but expanded view is
        // full-width.
        int horizontalMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        setHorizontalMargins(sectionTitle, horizontalMargin, horizontalMargin);
        setHorizontalMargins(mSectionExpander.getChevronButton(), 0, horizontalMargin);
        setHorizontalMargins(mSummaryView, horizontalMargin, 0);
        setHorizontalMargins(mItemsView, 0, 0);

        // Add some space between title and summary / expanded section.
        mSectionExpander.setTitleToContentMargin(titleToContentPadding);

        TextView titleView = mSectionExpander.findViewById(R.id.section_title);
        titleView.setText(title);

        TextView titleAddButtonLabelView =
                mSectionExpander.findViewById(R.id.section_title_add_button_label);
        titleAddButtonLabelView.setText(titleAddButton);

        mTitleAddButton = mSectionExpander.findViewById(R.id.section_title_add_button);
        mTitleAddButton.setOnClickListener(unusedView -> createOrEditItem(null, null));

        mItemsView.setOnAddButtonClickedListener(() -> createOrEditItem(null, null));
        parent.addView(mSectionExpander,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        updateVisibility();
    }

    View getView() {
        return mSectionExpander;
    }

    void setVisible(boolean visible) {
        mSectionExpander.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setListener(Callback<T> listener) {
        mListener = listener;
    }

    /**
     * Replaces the set of displayed items.
     *
     * @param options The new items.
     * @param selectedItemIndex The index of the item in |items| to select. If < 0, the first
     * complete item will automatically be selected.
     */
    void setItems(List<T> options, int selectedItemIndex) {
        // Automatically pre-select unless already specified outside.
        if (selectedItemIndex < 0 && !options.isEmpty()) {
            for (int i = 0; i < options.size(); i++) {
                if (options.get(i).isComplete()) {
                    selectedItemIndex = i;
                }
            }
            // Fallback: if there are no complete items, select the first (incomplete) one.
            if (selectedItemIndex == SectionInformation.NO_SELECTION) {
                selectedItemIndex = 0;
            }
        }

        Item initiallySelectedItem = null;
        mItems.clear();
        mItemsView.clearItems();
        mSelectedOption = null;
        for (int i = 0; i < options.size(); i++) {
            Item item = createItem(options.get(i));
            addItem(item);

            if (i == selectedItemIndex) {
                initiallySelectedItem = item;
            }
        }
        updateVisibility();

        if (initiallySelectedItem != null) {
            mIgnoreItemSelectedNotifications = true;
            selectItem(initiallySelectedItem);
            mIgnoreItemSelectedNotifications = false;
        }
    }

    private void addOrUpdateItem(T option, boolean select) {
        // Update existing item if possible.
        Item item = null;
        for (int i = 0; i < mItems.size(); i++) {
            if (TextUtils.equals(mItems.get(i).mOption.getIdentifier(), option.getIdentifier())) {
                item = mItems.get(i);
                item.mOption = option;
                updateFullView(item.mFullView, item.mOption);
                break;
            }
        }

        if (item == null) {
            item = createItem(option);
            addItem(item);
        } else {
            updateSummaryView(mSummaryView, item.mOption);
        }
        onItemAddedOrUpdated(option);

        if (select) {
            mIgnoreItemSelectedNotifications = true;
            selectItem(item);
            mIgnoreItemSelectedNotifications = false;
        }
    }

    private void selectItem(Item item) {
        mSelectedOption = item.mOption;
        mItemsView.setCheckedItem(item.mFullView);
        updateSummaryView(mSummaryView, item.mOption);
        updateVisibility();

        if (mListener != null) {
            mListener.onResult(
                    item.mOption != null && item.mOption.isComplete() ? item.mOption : null);
        }
    }

    /**
     * Creates a new |Item| from |option|.
     */
    private Item createItem(T option) {
        View fullView = LayoutInflater.from(mContext).inflate(mFullViewResId, null);
        updateFullView(fullView, option);
        Item item = new Item(fullView, option);
        return item;
    }

    /**
     * Adds |item| to the UI.
     */
    private void addItem(Item item) {
        mItems.add(item);
        mItemsView.addItem(item.mFullView, /*hasEditButton=*/true, selected -> {
            if (mIgnoreItemSelectedNotifications || !selected) {
                return;
            }
            mIgnoreItemSelectedNotifications = true;
            selectItem(item.mFullView, item.mOption);
            mIgnoreItemSelectedNotifications = false;
            if (item.mOption.isComplete()) {
                // Workaround for Android bug: a layout transition may cause the newly checked
                // radiobutton to not render properly.
                mSectionExpander.post(() -> mSectionExpander.setExpanded(false));
            } else {
                createOrEditItem(item.mFullView, item.mOption);
            }
        }, () -> createOrEditItem(item.mFullView, item.mOption));
        updateVisibility();
    }

    /**
     * Asks the subclass to edit an item or create a new one (if |oldItem| is null). Subclasses
     * should call |onItemCreatedOrEdited| when they are done.
     * @param oldFullView The view associated with |oldItem|.
     * @param oldItem The item to be edited (null if a new item should be created).
     */
    protected abstract void createOrEditItem(@Nullable View oldFullView, @Nullable T oldItem);

    /**
     * Asks the subclass to update the contents of |fullView|, which was previously created by
     * |createFullView|.
     */
    protected abstract void updateFullView(View fullView, T option);

    /**
     * Asks the subclass to update the contents of the summary view.
     */
    protected abstract void updateSummaryView(View summaryView, T option);

    /**
     * An item was added to the list or updated in place. Subclasses may react to this event. A
     * common use case is for subclasses to add the information of the new profile to the
     * autocomplete fields of their editor.
     */
    protected abstract void onItemAddedOrUpdated(T option);

    /**
     * For convenience. Hides |view| if it is empty.
     */
    void hideIfEmpty(TextView view) {
        view.setVisibility(view.length() == 0 ? View.GONE : View.VISIBLE);
    }

    /**
     * An old item was edited or a new item was created.
     *
     * @param oldItem The item that was edited. null to indicate that a new item was created.
     * @param oldFullView The view associated with |oldItem|. Null if |oldItem| is null.
     * @param newItem The new or edited item. Cancelling an 'edit' flow will yield the old item.
     */
    void onItemCreatedOrEdited(
            @Nullable T oldItem, @Nullable View oldFullView, @Nullable T newItem) {
        if (newItem == null) {
            // User cancelled 'add' flow.
            return;
        } else if (!newItem.isComplete()) {
            // User cancelled 'edit' flow.
            return;
            // TODO(crbug.com/806868) add special case for cancelling edit of incomplete item
        } else {
            addOrUpdateItem(newItem, /*select = */ true);
        }
    }

    private boolean isEmpty() {
        return mItems.isEmpty();
    }

    private void setHorizontalMargins(View view, int marginStart, int marginEnd) {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(marginStart);
        lp.setMarginEnd(marginEnd);
        view.setLayoutParams(lp);
    }

    private void selectItem(View fullView, T option) {
        mItemsView.setCheckedItem(fullView);
        updateSummaryView(mSummaryView, option);
        updateVisibility();

        if (mListener != null) {
            mListener.onResult(option != null && option.isComplete() ? option : null);
        }
    }

    private void updateVisibility() {
        mTitleAddButton.setVisibility(isEmpty() ? View.VISIBLE : View.GONE);
        mSectionExpander.setFixed(isEmpty());
        mSectionExpander.setCollapsedVisible(!isEmpty());
        mSectionExpander.setExpandedVisible(!isEmpty());
        if (isEmpty()) {
            mSectionExpander.setExpanded(false);
        }
    }
}
