// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

public class InputDialogContainerTest extends AndroidTestCase {
    private static int TEXT_INPUT_TYPE_DATE = 0;
    private static int TEXT_INPUT_TYPE_DATETIME = 1;
    private static int TEXT_INPUT_TYPE_DATETIMELOCAL = 2;
    private static int TEXT_INPUT_TYPE_MONTH = 3;
    private static int TEXT_INPUT_TYPE_TIME = 4;
    private static int TEXT_INPUT_TYPE_WEEK = 5;

    // Defined in third_party/WebKit/Source/platform/DateComponents.h
    private static double DATE_DIALOG_DEFAULT_MIN = -62135596800000.0;
    private static double DATE_DIALOG_DEFAULT_MAX = 8640000000000000.0;
    private static double DATETIMELOCAL_DIALOG_DEFAULT_MIN = -62135596800000.0;
    private static double DATETIMELOCAL_DIALOG_DEFAULT_MAX = 8640000000000000.0;
    private static double MONTH_DIALOG_DEFAULT_MIN = -23628.0;
    private static double MONTH_DIALOG_DEFAULT_MAX = 3285488.0;
    private static double TIME_DIALOG_DEFAULT_MIN = 0.0;
    private static double TIME_DIALOG_DEFAULT_MAX = 86399999.0;
    private static double WEEK_DIALOG_DEFAULT_MIN = -62135596800000.0;
    private static double WEEK_DIALOG_DEFAULT_MAX = 8639999568000000.0;

    InputActionDelegateForTests mInputActionDelegate;
    InputDialogContainerForTests mInputDialogContainer;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        InputDialogContainer.initializeInputTypes(TEXT_INPUT_TYPE_DATE,
                                                  TEXT_INPUT_TYPE_DATETIME,
                                                  TEXT_INPUT_TYPE_DATETIMELOCAL,
                                                  TEXT_INPUT_TYPE_MONTH,
                                                  TEXT_INPUT_TYPE_TIME,
                                                  TEXT_INPUT_TYPE_WEEK);
        mInputActionDelegate = new InputActionDelegateForTests();
        mInputDialogContainer = new InputDialogContainerForTests(getContext(),
                                                                 mInputActionDelegate);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDateValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATE,
                1970, 0, 1,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATE, 0.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATE,
                1, 0, 1,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATE, -62135596800000.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATE,
                275760, 8, 13,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATE, 8640000000000000.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATE,
                2013, 10, 7,
                0, 0, 0, 0, 0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATE, 1383782400000.0,
                DATE_DIALOG_DEFAULT_MIN, DATE_DIALOG_DEFAULT_MAX, 1.0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDatetimelocalValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATETIMELOCAL,
                1970, 0, 1,
                0, 0, 0, 0, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATETIMELOCAL, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATETIMELOCAL,
                1, 0, 1,
                0, 0, 0, 0, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATETIMELOCAL, -62135596800000.0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATETIMELOCAL,
                275760, 8, 13,
                0, 0, 0, 0, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATETIMELOCAL, 8640000000000000.0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_DATETIMELOCAL,
                2013, 10, 8,
                1, 1, 2, 196, 0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 0.001);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_DATETIMELOCAL, 1383872462196.0,
                DATETIMELOCAL_DIALOG_DEFAULT_MIN, DATETIMELOCAL_DIALOG_DEFAULT_MAX, 0.001);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testMonthValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_MONTH,
                1970, 0, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_MONTH, 0.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_MONTH,
                1, 0, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_MONTH, -23628.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_MONTH,
                275760, 8, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_MONTH, 3285488.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_MONTH,
                2013, 10, 0,
                0, 0, 0, 0, 0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_MONTH, 526.0,
                MONTH_DIALOG_DEFAULT_MIN, MONTH_DIALOG_DEFAULT_MAX, 1.0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testTimeValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_TIME,
                0, 0, 0,
                0, 0, 0, 0, 0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_TIME, 0.0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);

        // Time dialog only shows the hour and minute fields.
        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_TIME,
                0, 0, 0,
                23, 59, 0, 0, 0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_TIME, 86399999.0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_TIME,
                0, 0, 0,
                15, 23, 0, 0, 0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_TIME, 55425678.0,
                TIME_DIALOG_DEFAULT_MIN, TIME_DIALOG_DEFAULT_MAX, 1.0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testWeekValueParsing() {
        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_WEEK,
                1970, 0, 0,
                0, 0, 0, 0, 1,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_WEEK, -259200000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_WEEK,
                1, 0, 0,
                0, 0, 0, 0, 1,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_WEEK, -62135596800000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_WEEK,
                275760, 0, 0,
                0, 0, 0, 0, 37,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_WEEK, 8639999568000000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);

        mInputDialogContainer.setShowDialogExpectation(TEXT_INPUT_TYPE_WEEK,
                2013, 0, 0,
                0, 0, 0, 0, 44,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
        mInputDialogContainer.showPickerDialog(TEXT_INPUT_TYPE_WEEK, 1382918400000.0,
                WEEK_DIALOG_DEFAULT_MIN, WEEK_DIALOG_DEFAULT_MAX, 1.0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDateValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATE,
                1970, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATE,
                1, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(8640000000000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATE,
                275760, 8, 13,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(1383782400000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATE,
                2013, 10, 7,
                0, 0, 0, 0, 0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testDatetimelocalValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATETIMELOCAL,
                1970, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATETIMELOCAL,
                1, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(8640000000000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATETIMELOCAL,
                275760, 8, 13,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(1383872462196.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_DATETIMELOCAL,
                2013, 10, 8,
                1, 1, 2, 196, 0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testMonthValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_MONTH,
                1970, 0, 0,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_MONTH,
                1, 0, 1,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(8640000000000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_MONTH,
                275760, 8, 0,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(1383872462196.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_MONTH,
                2013, 10, 0,
                0, 0, 0, 0, 0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testTimeValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(0.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_TIME,
                0, 0, 0,
                0, 0, 0, 0, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(86399999.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_TIME,
                0, 0, 0,
                23, 59, 59, 999, 0);

        mInputActionDelegate.setReplaceDateTimeExpectation(55425678.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_TIME,
                2013, 10, 0,
                3, 23, 45, 678, 0);
    }

    @SmallTest
    @Feature({"DateTimeDialog"})
    public void testWeekValueGenerating() {
        mInputActionDelegate.setReplaceDateTimeExpectation(-259200000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_WEEK,
                1970, 0, 0,
                0, 0, 0, 0, 1);

        mInputActionDelegate.setReplaceDateTimeExpectation(-62135596800000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_WEEK,
                1, 0, 0,
                0, 0, 0, 0, 1);

        mInputActionDelegate.setReplaceDateTimeExpectation(8639999568000000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_WEEK,
                275760, 0, 0,
                0, 0, 0, 0, 37);

        mInputActionDelegate.setReplaceDateTimeExpectation(1382918400000.0);
        mInputDialogContainer.setFieldDateTimeValue(TEXT_INPUT_TYPE_WEEK,
                2013, 0, 0,
                0, 0, 0, 0, 44);
    }

    private static class InputActionDelegateForTests
            implements InputDialogContainer.InputActionDelegate {
        private double mExpectedDialogValue;

        public void setReplaceDateTimeExpectation(double dialogValue) {
            mExpectedDialogValue = dialogValue;
        }

        @Override
        public void replaceDateTime(double dialogValue) {
            assertEquals(mExpectedDialogValue, dialogValue);
        }

        @Override
        public void cancelDateTimeDialog() {
        }
    };

    private static class InputDialogContainerForTests extends InputDialogContainer {
        private int mExpectedDialogType;
        private int mExpectedYear;
        private int mExpectedMonth;
        private int mExpectedMonthDay;
        private int mExpectedHourOfDay;
        private int mExpectedMinute;
        private int mExpectedSecond;
        private int mExpectedMillis;
        private int mExpectedWeek;
        private double mExpectedMin;
        private double mExpectedMax;
        private double mExpectedStep;

        public InputDialogContainerForTests(
                Context context,
                InputDialogContainer.InputActionDelegate inputActionDelegate) {
            super(context, inputActionDelegate);
        }

        void setShowDialogExpectation(int dialogType,
                int year, int month, int monthDay,
                int hourOfDay, int minute, int second, int millis, int week,
                double min, double max, double step) {
            mExpectedDialogType = dialogType;
            mExpectedYear = year;
            mExpectedMonth = month;
            mExpectedMonthDay = monthDay;
            mExpectedHourOfDay = hourOfDay;
            mExpectedMinute = minute;
            mExpectedSecond = second;
            mExpectedMillis = millis;
            mExpectedWeek = week;
            mExpectedMin = min;
            mExpectedMax = max;
            mExpectedStep = step;
        }

        @Override
        void showPickerDialog(final int dialogType,
                int year, int month, int monthDay,
                int hourOfDay, int minute, int second, int millis, int week,
                double min, double max, double step) {
            assertEquals(mExpectedDialogType, dialogType);
            assertEquals(mExpectedYear, year);
            assertEquals(mExpectedMonth, month);
            assertEquals(mExpectedMonthDay, monthDay);
            assertEquals(mExpectedHourOfDay, hourOfDay);
            assertEquals(mExpectedMinute, minute);
            assertEquals(mExpectedSecond, second);
            assertEquals(mExpectedMillis, millis);
            assertEquals(mExpectedWeek, week);
            assertEquals(mExpectedMin, min);
            assertEquals(mExpectedMax, max);
            assertEquals(mExpectedStep, step);
        }

        public void setFieldDateTimeValue(int dialogType,
                                       int year, int month, int monthDay,
                                       int hourOfDay, int minute, int second, int millis,
                                       int week) {
            super.setFieldDateTimeValue(dialogType,
                                        year, month, monthDay,
                                        hourOfDay, minute, second, millis,
                                        week);
        }
    }
}
