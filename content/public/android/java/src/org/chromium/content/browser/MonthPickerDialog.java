// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.content.app.AppResource;
import org.chromium.content.browser.MonthPicker.OnMonthChangedListener;

public class MonthPickerDialog extends AlertDialog implements OnClickListener,
        OnMonthChangedListener {

    private static final String YEAR = "year";
    private static final String MONTH = "month";

    private final MonthPicker mMonthPicker;
    private final OnMonthSetListener mCallBack;

    /**
     * The callback used to indicate the user is done filling in the date.
     */
    public interface OnMonthSetListener {

        /**
         * @param view The view associated with this listener.
         * @param year The year that was set.
         * @param monthOfYear The month that was set (0-11) for compatibility
         *  with {@link java.util.Calendar}.
         */
        void onMonthSet(MonthPicker view, int year, int monthOfYear);
    }

    /**
     * @param context The context the dialog is to run in.
     * @param callBack How the parent is notified that the date is set.
     * @param year The initial year of the dialog.
     * @param monthOfYear The initial month of the dialog.
     */
    public MonthPickerDialog(Context context,
            OnMonthSetListener callBack,
            int year,
            int monthOfYear) {
        this(context, 0, callBack, year, monthOfYear);
    }

    /**
     * @param context The context the dialog is to run in.
     * @param theme the theme to apply to this dialog
     * @param callBack How the parent is notified that the date is set.
     * @param year The initial year of the dialog.
     * @param monthOfYear The initial month of the dialog.
     */
    public MonthPickerDialog(Context context,
            int theme,
            OnMonthSetListener callBack,
            int year,
            int monthOfYear) {
        super(context, theme);

        mCallBack = callBack;

        assert AppResource.STRING_DATE_PICKER_DIALOG_SET != 0;
        assert AppResource.STRING_MONTH_PICKER_DIALOG_TITLE != 0;
        assert AppResource.LAYOUT_MONTH_PICKER_DIALOG != 0;
        assert AppResource.ID_DATE_PICKER != 0;

        setButton(BUTTON_POSITIVE, context.getText(
                AppResource.STRING_DATE_PICKER_DIALOG_SET), this);
        setButton(BUTTON_NEGATIVE, context.getText(android.R.string.cancel),
                (OnClickListener) null);
        setIcon(0);
        setTitle(AppResource.STRING_MONTH_PICKER_DIALOG_TITLE);

        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(AppResource.LAYOUT_MONTH_PICKER_DIALOG, null);
        setView(view);
        mMonthPicker = (MonthPicker) view.findViewById(AppResource.ID_DATE_PICKER);
        mMonthPicker.init(year, monthOfYear, this);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        tryNotifyMonthSet();
    }

    private void tryNotifyMonthSet() {
        if (mCallBack != null) {
            mMonthPicker.clearFocus();
            mCallBack.onMonthSet(mMonthPicker, mMonthPicker.getYear(),
                    mMonthPicker.getMonth());
        }
    }

    @Override
    protected void onStop() {
        if (Build.VERSION.SDK_INT >= 16) {
            tryNotifyMonthSet();
        }
        super.onStop();
    }

    @Override
    public void onMonthChanged(MonthPicker view, int year, int month) {
        mMonthPicker.init(year, month, null);
    }

    /**
     * Gets the {@link MonthPicker} contained in this dialog.
     *
     * @return The calendar view.
     */
    public MonthPicker getMonthPicker() {
        return mMonthPicker;
    }

    /**
     * Sets the current date.
     *
     * @param year The date year.
     * @param monthOfYear The date month.
     */
    public void updateDate(int year, int monthOfYear) {
        mMonthPicker.updateMonth(year, monthOfYear);
    }

    @Override
    public Bundle onSaveInstanceState() {
        Bundle state = super.onSaveInstanceState();
        state.putInt(YEAR, mMonthPicker.getYear());
        state.putInt(MONTH, mMonthPicker.getMonth());
        return state;
    }

    @Override
    public void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        int year = savedInstanceState.getInt(YEAR);
        int month = savedInstanceState.getInt(MONTH);
        mMonthPicker.init(year, month, this);
    }
}
