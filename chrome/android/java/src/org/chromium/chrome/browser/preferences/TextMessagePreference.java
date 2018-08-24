// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

/**
 * A preference that displays informational text.
 */
public class TextMessagePreference extends ChromeBasePreference {
    /**
     * Constructor for inflating from XML.
     */
    public TextMessagePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectable(false);
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        TextView titleView = (TextView) view.findViewById(android.R.id.title);
        if (!TextUtils.isEmpty(getTitle())) {
            titleView.setVisibility(View.VISIBLE);
            titleView.setSingleLine(false);
            titleView.setMaxLines(Integer.MAX_VALUE);
            titleView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            titleView.setVisibility(View.GONE);
        }

        TextView summaryView = (TextView) view.findViewById(android.R.id.summary);
        // No need to manually toggle visibility for summary - it is done in super.onBindView.
        summaryView.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
