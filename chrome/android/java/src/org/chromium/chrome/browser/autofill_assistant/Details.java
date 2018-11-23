// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.support.annotation.Nullable;

import org.json.JSONObject;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Java side equivalent of autofill_assistant::DetailsProto.
 */
class Details {
    enum DetailsField { TITLE, URL, DATE, DESCRIPTION, MID, IS_FINAL }

    private final String mTitle;
    private final String mUrl;
    @Nullable
    private final Date mDate;
    private final String mDescription;
    private final String mMid;
    private final boolean mIsFinal;
    /** Contains the fields that have changed when merging with other Details object. */
    private final Set<DetailsField> mFieldsChanged;
    // NOTE: When adding a new field, update the isEmpty and toJSONObject methods.

    static final Details EMPTY_DETAILS =
            new Details("", "", null, "", "", false, Collections.emptySet());

    private static final String RFC_3339_FORMAT_WITHOUT_TIMEZONE = "yyyy'-'MM'-'dd'T'HH':'mm':'ss";

    Details(String title, String url, @Nullable Date date, String description, String mId,
            boolean isFinal, Set<DetailsField> fieldsChanged) {
        this.mTitle = title;
        this.mUrl = url;
        this.mDate = date;
        this.mDescription = description;
        this.mMid = mId;
        this.mIsFinal = isFinal;
        this.mFieldsChanged = fieldsChanged;
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

    JSONObject toJSONObject() {
        // Details are part of the feedback form, hence they need a JSON representation.
        Map<String, String> movieDetails = new HashMap<>();
        movieDetails.put("title", mTitle);
        movieDetails.put("url", mUrl);
        movieDetails.put("mId", mMid);
        if (mDate != null) movieDetails.put("date", mDate.toString());
        movieDetails.put("description", mDescription);
        return new JSONObject(movieDetails);
    }

    /**
     * Whether the details are not subject to change anymore. If set to false the animated
     * placeholders will be displayed in place of missing data.
     */
    boolean isFinal() {
        return mIsFinal;
    }

    Set<DetailsField> getFieldsChanged() {
        return mFieldsChanged;
    }

    boolean isEmpty() {
        return mTitle.isEmpty() && mUrl.isEmpty() && mDescription.isEmpty() && mDate == null
                && mMid.isEmpty();
    }

    // TODO(crbug.com/806868): Create a fallback when there are no parameters for details.
    static Details makeFromParameters(Map<String, String> parameters) {
        String title = "";
        String description = "";
        Date date = null;
        String mId = "";
        for (String key : parameters.keySet()) {
            if (key.endsWith("E_NAME")) {
                title = parameters.get(key);
                continue;
            }

            if (key.endsWith("R_NAME")) {
                description = parameters.get(key);
                continue;
            }

            if (key.endsWith("MID")) {
                mId = parameters.get(key);
                continue;
            }

            if (key.contains("DATETIME")) {
                try {
                    // The parameter contains the timezone shift from the current location, that we
                    // don't care about.
                    date = new SimpleDateFormat(RFC_3339_FORMAT_WITHOUT_TIMEZONE, Locale.ROOT)
                                   .parse(parameters.get(key));
                } catch (ParseException e) {
                    // Ignore.
                }
            }
        }

        return new Details(title, /* url= */ "", date, description, mId, /* isFinal= */ false,
                /* fieldsChanged= */ Collections.emptySet());
    }

    /**
     * Merges {@param oldDetails} with the {@param newDetails} filling the missing fields.
     *
     * <p>The distinction is important, as the fields from old version take precedence, with the
     * exception of date and isFinal fields.
     *
     * <p>If {@param oldDetails} are empty copy of {@param newDetails} with empty
     * {@code changedFields} is returned
     */
    static Details merge(Details oldDetails, Details newDetails) {
        if (oldDetails.isEmpty()) {
            return new Details(newDetails.getTitle(), newDetails.getUrl(), newDetails.getDate(),
                    newDetails.getDescription(), newDetails.getMid(), newDetails.isFinal(),
                    Collections.emptySet());
        }

        Set<DetailsField> changedFields = new HashSet<>();

        String title = oldDetails.getTitle();
        String mId = oldDetails.getMid();
        // Title and mId are tightly connected. If mId is different then title should also be
        // different, but it's not true the other way (titles might slightly differ).
        if (!oldDetails.getMid().isEmpty() && !newDetails.getMid().isEmpty()
                && !oldDetails.getMid().equals(newDetails.getMid())
                && !oldDetails.getTitle().equals(newDetails.getTitle())) {
            changedFields.add(DetailsField.TITLE);
            title = newDetails.getTitle();
            mId = newDetails.getMid();
        }
        String url = oldDetails.getUrl().isEmpty() ? newDetails.getUrl() : oldDetails.getUrl();

        Date date = oldDetails.getDate();
        if (oldDetails.getDate() == null) {
            // There was no date. Use the new one unconditionally.
            date = newDetails.getDate();
        } else if (newDetails.getDate() != null
                && !oldDetails.getDate().equals(newDetails.getDate())) {
            // Only if new date is different than old (which wasn't null) mark it as changedField.
            date = newDetails.getDate();
            changedFields.add(DetailsField.DATE);
        }

        String description = oldDetails.getDescription().isEmpty() ? newDetails.getDescription()
                                                                   : oldDetails.getDescription();

        boolean isFinal = newDetails.isFinal();
        return new Details(title, url, date, description, mId, isFinal, changedFields);
    }
}