// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;

import org.chromium.chrome.R;

/**
 * A check box used to determine if a page should always be translated.
 */
public class TranslateCheckBox extends CheckBox implements OnCheckedChangeListener {
    private static final int TEXT_SIZE_SP = 13;

    private final SubPanelListener mListener;
    private final TranslateOptions mOptions;

    public TranslateCheckBox(Context context, TranslateOptions options, SubPanelListener listener) {
        super(context);
        mOptions = options;
        mListener = listener;

        setId(R.id.infobar_extra_check);
        setText(context.getString(R.string.translate_always_text, mOptions.sourceLanguage()));
        setTextColor(context.getResources().getColor(R.color.infobar_text));
        setTextSize(TEXT_SIZE_SP);
        setChecked(mOptions.alwaysTranslateLanguageState());
        setOnCheckedChangeListener(this);
    }

    @Override
    public void onCheckedChanged(CompoundButton view, boolean isChecked) {
        mOptions.toggleAlwaysTranslateLanguageState(isChecked);
        if (isChecked) {
            mListener.onPanelClosed(InfoBar.ACTION_TYPE_NONE);
        } else {
            mListener.onOptionsChanged();
        }
    }
}
