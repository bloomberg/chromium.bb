// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * StatusView is a location bar's view displaying status (icons and/or text).
 */
public class StatusView extends LinearLayout {
    private ImageView mNavigationButton;
    private ImageButton mSecurityButton;
    private TextView mVerboseStatusTextView;

    public StatusView(Context context) {
        super(context);
    }

    public StatusView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mNavigationButton = findViewById(R.id.navigation_button);
        mSecurityButton = findViewById(R.id.security_button);
        mVerboseStatusTextView = findViewById(R.id.location_bar_verbose_status);

        assert mNavigationButton != null : "Missing navigation type view.";
    }

    // TODO(ender): replace these with methods manipulating views directly.
    // Do not depend on these when creating new code!
    public ImageView getNavigationButton() {
        return mNavigationButton;
    }

    public ImageButton getSecurityButton() {
        return mSecurityButton;
    }

    public TextView getVerboseStatusTextView() {
        return mVerboseStatusTextView;
    }
}
