// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.database.DataSetObserver;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.SpinnerAdapter;
import android.widget.TextView;

import org.chromium.base.JNINamespace;
import org.chromium.chrome.R;

/**
 * Android wrapper for CountryComboboxModel.
 *
 * Only useable from the UI layer. Used in the Android settings UI.
 * See chrome/browser/ui/android/autofill/country_adapter_android.h for more details.
 */
@JNINamespace("autofill")
public class CountryAdapter implements SpinnerAdapter {
    /**
     * The items to show in the spinner.
     *
     * Even indices are display names, odd indices are country codes.
     */
    private String[] mItems;

    private LayoutInflater mInflater;
    private final long mCountryAdapterAndroid;

    public CountryAdapter(LayoutInflater inflater) {
        mInflater = inflater;
        mCountryAdapterAndroid = nativeInit();
        mItems = nativeGetItems(mCountryAdapterAndroid);
    }

    @Override
    public int getCount() {
        return mItems.length / 2;
    }

    @Override
    public Object getItem(int position) {
        return mItems[position * 2 + 1];
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public int getItemViewType(int position) {
        return 0;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        TextView textView = null;
        if (convertView instanceof TextView) {
            textView = (TextView) convertView;
        }
        if (textView == null) {
            textView = (TextView) mInflater.inflate(R.layout.country_text, parent, false);
        }

        textView.setText(mItems[position * 2]);
        return textView;
    }

    @Override
    public boolean hasStableIds() {
        return true;
    }

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public boolean isEmpty() {
        return false;
    }

    @Override
    public void registerDataSetObserver(DataSetObserver observer) {}

    @Override
    public void unregisterDataSetObserver(DataSetObserver observer) {}

    @Override
    public View getDropDownView(int position, View convertView, ViewGroup parent) {
        TextView textView = null;
        if (convertView instanceof TextView) {
            textView = (TextView) convertView;
        }
        if (textView == null) {
            textView = (TextView) mInflater.inflate(R.layout.country_item, parent, false);
        }

        textView.setText(mItems[position * 2]);
        return textView;
    }

    /**
     * Gets the index in the model for the given country code.
     */
    public int getIndexForCountryCode(String countryCode) {
        for (int i = 0; i < getCount(); i++) {
            if (countryCode.equals(getItem(i))) {
                return i;
            }
        }
        return 0;
    }

    private native long nativeInit();
    private native String[] nativeGetItems(long nativeCountryAdapterAndroid);
}
