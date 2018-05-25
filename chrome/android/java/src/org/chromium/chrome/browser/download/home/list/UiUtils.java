// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;

import java.util.Calendar;
import java.util.Date;

/** A set of helper utility methods for the UI. */
public final class UiUtils {
    private UiUtils() {}

    /**
     * Converts {@code date} into a string meant to be used as a list header.
     * @param date The {@link Date} to convert.
     * @return     The {@link CharSequence} representing the header.
     */
    public static CharSequence dateToHeaderString(Date date) {
        Context context = ContextUtils.getApplicationContext();

        Calendar calendar1 = CalendarFactory.get();
        Calendar calendar2 = CalendarFactory.get();

        calendar1.setTimeInMillis(System.currentTimeMillis());
        calendar2.setTime(date);

        StringBuilder builder = new StringBuilder();
        if (CalendarUtils.isSameDay(calendar1, calendar2)) {
            builder.append(context.getString(R.string.today)).append(" - ");
        } else {
            calendar1.add(Calendar.DATE, -1);
            if (CalendarUtils.isSameDay(calendar1, calendar2)) {
                builder.append(context.getString(R.string.yesterday)).append(" - ");
            }
        }

        builder.append(DateUtils.formatDateTime(context, date.getTime(),
                DateUtils.FORMAT_ABBREV_WEEKDAY | DateUtils.FORMAT_ABBREV_MONTH
                        | DateUtils.FORMAT_SHOW_YEAR));

        return builder;
    }
}