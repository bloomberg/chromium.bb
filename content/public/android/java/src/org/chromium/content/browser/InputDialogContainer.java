// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.AlertDialog;
import android.app.DatePickerDialog;
import android.app.TimePickerDialog;
import android.app.DatePickerDialog.OnDateSetListener;
import android.app.TimePickerDialog.OnTimeSetListener;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.text.TextUtils;
import android.text.format.DateFormat;
import android.text.format.Time;
import android.widget.DatePicker;
import android.widget.TimePicker;

import org.chromium.content.browser.DateTimePickerDialog.OnDateTimeSetListener;
import org.chromium.content.browser.MonthPickerDialog.OnMonthSetListener;
import org.chromium.content.R;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;

class InputDialogContainer {

    interface InputActionDelegate {
        void cancelDateTimeDialog();
        void replaceDateTime(String dateTime);
    }

    // Default values used in Time representations of selected date/time before formatting.
    // They are never displayed to the user.
    private static final int YEAR_DEFAULT = 1970;
    private static final int MONTH_DEFAULT = 0;
    private static final int MONTHDAY_DEFAULT = 1;
    private static final int HOUR_DEFAULT = 0;
    private static final int MINUTE_DEFAULT = 0;

    // Date formats as accepted by Time.format.
    private static final String HTML_DATE_FORMAT = "%Y-%m-%d";
    private static final String HTML_TIME_FORMAT = "%H:%M";
    // For datetime we always send selected time as UTC, as we have no timezone selector.
    // This is consistent with other browsers.
    private static final String HTML_DATE_TIME_FORMAT = "%Y-%m-%dT%H:%MZ";
    private static final String HTML_DATE_TIME_LOCAL_FORMAT = "%Y-%m-%dT%H:%M";
    private static final String HTML_MONTH_FORMAT = "%Y-%m";

    // Date formats as accepted by SimpleDateFormat.
    private static final String PARSE_DATE_FORMAT = "yyyy-MM-dd";
    private static final String PARSE_TIME_FORMAT = "HH:mm";
    private static final String PARSE_DATE_TIME_FORMAT = "yyyy-MM-dd'T'HH:mm'Z'";
    private static final String PARSE_DATE_TIME_LOCAL_FORMAT = "yyyy-MM-dd'T'HH:mm";
    private static final String PARSE_MONTH_FORMAT = "yyyy-MM";

    private static int sTextInputTypeDate;
    private static int sTextInputTypeDateTime;
    private static int sTextInputTypeDateTimeLocal;
    private static int sTextInputTypeMonth;
    private static int sTextInputTypeTime;

    private Context mContext;

    // Prevents sending two notifications (from onClick and from onDismiss)
    private boolean mDialogAlreadyDismissed;

    private AlertDialog mDialog;
    private InputActionDelegate mInputActionDelegate;

    static void initializeInputTypes(int textInputTypeDate, int textInputTypeDateTime,
            int textInputTypeDateTimeLocal, int textInputTypeMonth, int textInputTypeTime) {
        sTextInputTypeDate = textInputTypeDate;
        sTextInputTypeDateTime = textInputTypeDateTime;
        sTextInputTypeDateTimeLocal = textInputTypeDateTimeLocal;
        sTextInputTypeMonth = textInputTypeMonth;
        sTextInputTypeTime = textInputTypeTime;
    }

    static boolean isDialogInputType(int type) {
        return type == sTextInputTypeDate || type == sTextInputTypeTime
                || type == sTextInputTypeDateTime || type == sTextInputTypeDateTimeLocal
                || type == sTextInputTypeMonth;
    }

    InputDialogContainer(Context context, InputActionDelegate inputActionDelegate) {
        mContext = context;
        mInputActionDelegate = inputActionDelegate;
    }

    void showDialog(String text, int textInputType) {
        if (isDialogShowing()) mDialog.dismiss();

        Time time = parse(text, textInputType);
        if (textInputType == sTextInputTypeDate) {
            mDialog = new DatePickerDialog(mContext, new DateListener(),
                    time.year, time.month, time.monthDay);
        } else if (textInputType == sTextInputTypeTime) {
            mDialog = new TimePickerDialog(mContext, new TimeListener(),
                    time.hour, time.minute, DateFormat.is24HourFormat(mContext));
        } else if (textInputType == sTextInputTypeDateTime ||
                textInputType == sTextInputTypeDateTimeLocal) {
            mDialog = new DateTimePickerDialog(mContext,
                    new DateTimeListener(textInputType == sTextInputTypeDateTimeLocal),
                    time.year, time.month, time.monthDay,
                    time.hour, time.minute, DateFormat.is24HourFormat(mContext));
        } else if (textInputType == sTextInputTypeMonth) {
            mDialog = new MonthPickerDialog(mContext, new MonthListener(),
                    time.year, time.month);
        }

        mDialog.setButton(DialogInterface.BUTTON_POSITIVE,
                mContext.getText(R.string.date_picker_dialog_set),
                (DialogInterface.OnClickListener) mDialog);

        mDialog.setButton(DialogInterface.BUTTON_NEGATIVE,
                mContext.getText(android.R.string.cancel),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mDialogAlreadyDismissed = true;
                        mInputActionDelegate.cancelDateTimeDialog();
                    }
                });

        mDialog.setButton(DialogInterface.BUTTON_NEUTRAL,
                mContext.getText(R.string.date_picker_dialog_clear),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mDialogAlreadyDismissed = true;
                        mInputActionDelegate.replaceDateTime("");
                    }
                });

        mDialogAlreadyDismissed = false;
        mDialog.show();
    }

    boolean isDialogShowing() {
        return mDialog != null && mDialog.isShowing();
    }

    void dismissDialog() {
        if (isDialogShowing()) mDialog.dismiss();
    }

    private static SimpleDateFormat getParseDateFormat(int textInputType) {
        String formatString = null;
        if (textInputType == sTextInputTypeDate) {
            formatString = PARSE_DATE_FORMAT;
        } else if (textInputType == sTextInputTypeTime) {
            formatString = PARSE_TIME_FORMAT;
        } else if (textInputType == sTextInputTypeDateTime) {
            formatString = PARSE_DATE_TIME_FORMAT;
        } else if (textInputType == sTextInputTypeDateTimeLocal) {
            formatString = PARSE_DATE_TIME_LOCAL_FORMAT;
        } else if (textInputType == sTextInputTypeMonth) {
            formatString = PARSE_MONTH_FORMAT;
        }

        if (formatString != null) {
            return new SimpleDateFormat(formatString);
        }

        return null;
    }

    /**
     * Parse the text String as a date or time according to the provided text input type.
     */
    private static Time parse(String text, int textInputType) {
        Time result = null;
        if (!TextUtils.isEmpty(text)) {
            try {
                SimpleDateFormat format = getParseDateFormat(textInputType);
                if (format != null) {
                    Date date = format.parse(text);
                    result = new Time();
                    result.set(date.getTime());
                }
            } catch (ParseException e) {
                // Leave time as null.
            }
        }

        if (result == null) {
            result = new Time();
            result.setToNow();
        }

        return result;
    }

    private class DateListener implements OnDateSetListener {
        @Override
        public void onDateSet(DatePicker view, int year, int month, int monthDay) {
            if (!mDialogAlreadyDismissed) {
                setFieldDateTimeValue(year, month, monthDay, HOUR_DEFAULT, MINUTE_DEFAULT,
                        HTML_DATE_FORMAT);
            }
        }
    }

    private class TimeListener implements OnTimeSetListener {
        @Override
        public void onTimeSet(TimePicker view, int hourOfDay, int minute) {
            if (!mDialogAlreadyDismissed) {
                setFieldDateTimeValue(YEAR_DEFAULT, MONTH_DEFAULT, MONTHDAY_DEFAULT,
                        hourOfDay, minute, HTML_TIME_FORMAT);
            }
        }
    }

    private class DateTimeListener implements OnDateTimeSetListener {
        private boolean mLocal;

        public DateTimeListener(boolean local) {
            mLocal = local;
        }

        @Override
        public void onDateTimeSet(DatePicker dateView, TimePicker timeView,
                int year, int month, int monthDay,
                int hourOfDay, int minute) {
            if (!mDialogAlreadyDismissed) {
                setFieldDateTimeValue(year, month, monthDay, hourOfDay, minute,
                        mLocal ? HTML_DATE_TIME_LOCAL_FORMAT : HTML_DATE_TIME_FORMAT);
            }
        }
    }

    private class MonthListener implements OnMonthSetListener {
        @Override
        public void onMonthSet(MonthPicker view, int year, int month) {
            if (!mDialogAlreadyDismissed) {
                setFieldDateTimeValue(year, month, MONTHDAY_DEFAULT,
                        HOUR_DEFAULT, MINUTE_DEFAULT, HTML_MONTH_FORMAT);
            }
        }
    }

    private void setFieldDateTimeValue(int year, int month, int monthDay, int hourOfDay,
            int minute, String dateFormat) {
        // Prevents more than one callback being sent to the native
        // side when the dialog triggers multiple events.
        mDialogAlreadyDismissed = true;

        Time time = new Time();
        time.year = year;
        time.month = month;
        time.monthDay = monthDay;
        time.hour = hourOfDay;
        time.minute = minute;
        mInputActionDelegate.replaceDateTime(time.format(dateFormat));
    }
}
