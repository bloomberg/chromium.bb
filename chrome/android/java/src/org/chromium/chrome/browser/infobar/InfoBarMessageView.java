// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.support.annotation.CallSuper;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * Handles the additional message view responsibilities needed for InfoBars.
 *   - Makes the full text view clickable if there is just a single link.
 */
public class InfoBarMessageView extends TextViewWithClickableSpans implements OnClickListener {
    public InfoBarMessageView(Context context) {
        super(context);
    }

    public InfoBarMessageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @CallSuper
    @Override
    public void setText(CharSequence text, BufferType type) {
        super.setText(text, type);
        ClickableSpan[] spans = getClickableSpans();
        setOnClickListener(spans != null && spans.length == 1 ? this : null);
    }

    @Override
    public void onClick(View v) {
        ClickableSpan[] spans = getClickableSpans();
        if (spans == null || spans.length != 1) {
            assert false : "Click listener should not be registered with more than 1 span";
            return;
        }
        spans[0].onClick(this);
    }
}
