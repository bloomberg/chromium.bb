// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.support.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.Date;

/**
 * Java side equivalent of autofill_assistant::DetailsProto.
 */
public class AssistantDetails {
    private final String mTitle;
    private final String mUrl;
    @Nullable
    private final Date mDate;
    private final String mDescription;
    private final String mMid;
    /** Whether user approval is required (i.e., due to changes). */
    private boolean mUserApprovalRequired;
    /** Whether the title should be highlighted. */
    private boolean mHighlightTitle;
    /** Whether the date should be highlighted. */
    private boolean mHighlightDate;
    /** Whether empty fields should have the animated placeholder background. */
    private final boolean mShowPlaceholdersForEmptyFields;
    /**
     * The correctly formatted price for the client locale, including the currency.
     * Example: '$20.50' or '20.50 €'.
     */
    private final String mPrice;
    // NOTE: When adding a new field, update the clearChangedFlags and toJSONObject methods.

    public AssistantDetails(String title, String url, @Nullable Date date, String description,
            String mId, @Nullable String price, boolean userApprovalRequired,
            boolean highlightTitle, boolean highlightDate, boolean showPlaceholdersForEmptyFields) {
        this.mTitle = title;
        this.mUrl = url;
        this.mDate = date;
        this.mDescription = description;
        this.mMid = mId;
        this.mPrice = price;
        this.mUserApprovalRequired = userApprovalRequired;
        this.mHighlightTitle = highlightTitle;
        this.mHighlightDate = highlightDate;
        this.mShowPlaceholdersForEmptyFields = showPlaceholdersForEmptyFields;
    }

    String getTitle() {
        return mTitle;
    }

    String getUrl() {
        return mUrl;
    }

    @Nullable
    Date getDate() {
        return mDate;
    }

    String getDescription() {
        return mDescription;
    }

    private String getMid() {
        return mMid;
    }

    @Nullable
    String getPrice() {
        return mPrice;
    }

    public JSONObject toJSONObject() throws JSONException {
        // Details are part of the feedback form, hence they need a JSON representation.
        JSONObject jsonRepresentation = new JSONObject();
        jsonRepresentation.put("title", mTitle);
        jsonRepresentation.put("url", mUrl);
        jsonRepresentation.put("mId", mMid);
        if (mPrice != null) jsonRepresentation.put("price", mPrice);
        if (mDate != null) jsonRepresentation.put("date", mDate.toString());
        jsonRepresentation.put("description", mDescription);
        jsonRepresentation.put("user_approval_required", mUserApprovalRequired);
        jsonRepresentation.put("highlight_title", mHighlightTitle);
        jsonRepresentation.put("highlight_date", mHighlightDate);
        return jsonRepresentation;
    }

    public boolean getUserApprovalRequired() {
        return mUserApprovalRequired;
    }

    boolean getHighlightTitle() {
        return mHighlightTitle;
    }

    boolean getHighlightDate() {
        return mHighlightDate;
    }

    boolean getShowPlaceholdersForEmptyFields() {
        return mShowPlaceholdersForEmptyFields;
    }

    /**
     * Clears all flags that indicate that this Details object has been changed.
     */
    public void clearChangedFlags() {
        mUserApprovalRequired = false;
        mHighlightTitle = false;
        mHighlightDate = false;
    }
}
