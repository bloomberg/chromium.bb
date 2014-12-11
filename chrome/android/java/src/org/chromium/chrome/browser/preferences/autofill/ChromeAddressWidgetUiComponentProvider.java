// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.TextView;

import com.android.i18n.addressinput.AddressField.WidthType;
import com.android.i18n.addressinput.AddressWidgetUiComponentProvider;

import org.chromium.chrome.R;

import java.util.HashMap;
import java.util.Map;

/**
 * Customizes the appearance of inputs used by addressinput.
 */
class ChromeAddressWidgetUiComponentProvider extends AddressWidgetUiComponentProvider {
    /**
     * A map of English language strings to translated string identifiers.
     * TODO(rouslan): Upstream AddressWidget translations. http://crbug.com/433959.
     */
    private final Map<String, Integer> mI18nLabelMap;

    /**
     * The label used for untranslated strings.
     */
    private static final String NOT_TRANSLATED = "NOT TRANSLATED";

    /**
     * The label that's never translated.
     */
    private static final String CEDEX = "CEDEX";

    public ChromeAddressWidgetUiComponentProvider(Context context) {
        super(context);
        mI18nLabelMap = new HashMap<String, Integer>();
        mI18nLabelMap.put(
                context.getString(
                    com.android.i18n.addressinput.R.string.i18n_country_or_region_label),
                    com.android.i18n.addressinput.R.string.libaddressinput_country_or_region_label);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_locality_label),
                com.android.i18n.addressinput.R.string.libaddressinput_locality_label);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_post_town),
                com.android.i18n.addressinput.R.string.libaddressinput_post_town);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_suburb),
                com.android.i18n.addressinput.R.string.libaddressinput_suburb);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_village_township),
                com.android.i18n.addressinput.R.string.libaddressinput_village_township);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_address_line1_label),
                com.android.i18n.addressinput.R.string.libaddressinput_address_line_1_label);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_postal_code_label),
                com.android.i18n.addressinput.R.string.libaddressinput_postal_code_label);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_zip_code_label),
                com.android.i18n.addressinput.R.string.libaddressinput_zip_code_label);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_area),
                com.android.i18n.addressinput.R.string.libaddressinput_area);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_county),
                com.android.i18n.addressinput.R.string.libaddressinput_county);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_department),
                com.android.i18n.addressinput.R.string.libaddressinput_department);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_district),
                com.android.i18n.addressinput.R.string.libaddressinput_district);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_do_si),
                com.android.i18n.addressinput.R.string.libaddressinput_do_si);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_emirate),
                com.android.i18n.addressinput.R.string.libaddressinput_emirate);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_island),
                com.android.i18n.addressinput.R.string.libaddressinput_island);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_oblast),
                com.android.i18n.addressinput.R.string.libaddressinput_oblast);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_parish),
                com.android.i18n.addressinput.R.string.libaddressinput_parish);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_prefecture),
                com.android.i18n.addressinput.R.string.libaddressinput_prefecture);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_province),
                com.android.i18n.addressinput.R.string.libaddressinput_province);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_state),
                com.android.i18n.addressinput.R.string.libaddressinput_state);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_recipient_label),
                com.android.i18n.addressinput.R.string.libaddressinput_recipient_label);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_neighborhood),
                com.android.i18n.addressinput.R.string.libaddressinput_neighborhood);
        mI18nLabelMap.put(
                context.getString(com.android.i18n.addressinput.R.string.i18n_organization_label),
                com.android.i18n.addressinput.R.string.libaddressinput_organization_label);
    }

    @Override
    protected TextView createUiLabel(CharSequence label, WidthType widthType) {
        TextView textView = (TextView) LayoutInflater.from(context).inflate(
                R.layout.autofill_profile_label, null);
        assert mI18nLabelMap.containsKey(label) || CEDEX.equals(label);
        if (mI18nLabelMap.containsKey(label)) {
            textView.setText(mI18nLabelMap.get(label));
        } else if (CEDEX.equals(label)) {
            textView.setText(label);
        } else {
            textView.setText(NOT_TRANSLATED);
        }
        textView.setFocusableInTouchMode(true);
        return textView;
    }
}
