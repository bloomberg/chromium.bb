// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.chromium.ui.DropdownItemBase;

/**
 * Autofill suggestion container used to store information needed for each Autofill popup entry.
 */
public class AutofillSuggestion extends DropdownItemBase {
    /**
     * The constant used to specify a http warning message in a list of Autofill suggestions.
     * Has to be kept in sync with enum in popup_item_ids.h
     * TODO(crbug.com/666529): Generate java constants from C++ enum.
     */
    private static final int ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE = -10;

    private final String mLabel;
    private final String mSublabel;
    private final int mIconId;
    private final int mSuggestionId;
    private final boolean mDeletable;
    private final boolean mIsMultilineLabel;

    /**
     * Constructs a Autofill suggestion container.
     * @param label The main label of the Autofill suggestion.
     * @param sublabel The describing sublabel of the Autofill suggestion.
     * @param suggestionId The type of suggestion.
     * @param deletable Whether the item can be deleted by the user.
     * @param multilineLabel Whether the label is displayed over multiple lines.
     */
    public AutofillSuggestion(String label, String sublabel, int iconId, int suggestionId,
            boolean deletable, boolean multilineLabel) {
        mLabel = label;
        mSublabel = sublabel;
        mIconId = iconId;
        mSuggestionId = suggestionId;
        mDeletable = deletable;
        mIsMultilineLabel = multilineLabel;
    }

    @Override
    public String getLabel() {
        return mLabel;
    }

    @Override
    public String getSublabel() {
        return mSublabel;
    }

    @Override
    public int getIconId() {
        return mIconId;
    }

    @Override
    public boolean isMultilineLabel() {
        return mIsMultilineLabel;
    }

    @Override
    public int getLabelFontColorResId() {
        if (mSuggestionId == ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE) {
            return R.color.http_bad_warning_message_text;
        }
        return super.getLabelFontColorResId();
    }

    @Override
    public int getLabelFontSizeResId() {
        if (mSuggestionId == ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE) {
            return R.dimen.dropdown_item_smaller_label_font_size;
        }
        return super.getLabelFontSizeResId();
    }

    @Override
    public boolean isLabelAndSublabelOnSameLine() {
        if (mSuggestionId == ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE) {
            return true;
        }
        return super.isLabelAndSublabelOnSameLine();
    }

    @Override
    public boolean isIconAtStart() {
        if (mSuggestionId == ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE) {
            return true;
        }
        return super.isIconAtStart();
    }

    public int getSuggestionId() {
        return mSuggestionId;
    }

    public boolean isDeletable() {
        return mDeletable;
    }

    public boolean isFillable() {
        // Negative suggestion ID indiciates a tool like "settings" or "scan credit card."
        // Non-negative suggestion ID indicates suggestions that can be filled into the form.
        return mSuggestionId >= 0;
    }
}
