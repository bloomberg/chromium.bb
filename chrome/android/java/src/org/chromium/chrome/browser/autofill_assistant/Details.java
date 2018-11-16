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
    enum DetailsField { TITLE, URL, DATE, DESCRIPTION, IS_FINAL }

    private final String mTitle;
    private final String mUrl;
    @Nullable
    private final Date mDate;
    private final String mDescription;
    private final boolean mIsFinal;
    /** Contains the fields that have changed when merging with other Details object. */
    private final Set<DetailsField> mFieldsChanged;
    // NOTE: When adding a new field, update the isEmpty method.

    static final Details EMPTY_DETAILS =
            new Details("", "", null, "", false, Collections.emptySet());

    private static final String RFC_3339_FORMAT_WITHOUT_TIMEZONE = "yyyy'-'MM'-'dd'T'HH':'mm':'ss";

    Details(String title, String url, @Nullable Date date, String description, boolean isFinal,
            Set<DetailsField> fieldsChanged) {
        this.mTitle = title;
        this.mUrl = url;
        this.mDate = date;
        this.mDescription = description;
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

    JSONObject toJSONObject() {
        // Details are part of the feedback form, hence they need a JSON representation.
        Map<String, String> movieDetails = new HashMap<>();
        movieDetails.put("title", mTitle);
        movieDetails.put("url", mUrl);
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
        return mTitle.isEmpty() && mUrl.isEmpty() && mDescription.isEmpty() && mDate == null;
    }

    // TODO(crbug.com/806868): Create a fallback when there are no parameters for details.
    static Details makeFromParameters(Map<String, String> parameters) {
        String title = "";
        String description = "";
        Date date = null;
        for (String key : parameters.keySet()) {
            if (key.contains("E_NAME")) {
                title = parameters.get(key);
                continue;
            }

            if (key.contains("R_NAME")) {
                description = parameters.get(key);
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

        return new Details(title, /* url= */ "", date, description, /* isFinal= */ false,
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
                    newDetails.getDescription(), newDetails.isFinal(), Collections.emptySet());
        }

        Set<DetailsField> changedFields = new HashSet<>();

        // TODO(crbug.com/806868): Change title if mid doesn't match.
        String title =
                oldDetails.getTitle().isEmpty() ? newDetails.getTitle() : oldDetails.getTitle();
        String url = oldDetails.getUrl().isEmpty() ? newDetails.getUrl() : oldDetails.getUrl();

        Date date = oldDetails.getDate();
        if (newDetails.getDate() != null && oldDetails.getDate() != null
                && !newDetails.getDate().equals(oldDetails.getDate())) {
            changedFields.add(DetailsField.DATE);
            date = newDetails.getDate();
        }

        String description = oldDetails.getDescription().isEmpty() ? newDetails.getDescription()
                                                                   : oldDetails.getDescription();

        boolean isFinal = newDetails.isFinal();
        return new Details(title, url, date, description, isFinal, changedFields);
    }
}