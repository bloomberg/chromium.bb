// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Visual representation of a snackbar. On phone it has weight the same as entire activity, on
 * tablet it has a fixed width anchored at start-bottom corner of current window.
 */
public class SnackbarPopupWindow extends PopupWindow {
    private final TemplatePreservingTextView mMessageView;
    private final TextView mActionButtonView;
    private final int mAnimationDuration;

    /**
     * Creates an instance of the {@link SnackbarPopupWindow}.
     * @param parent Parent View the popup window anchors to
     * @param listener An {@link OnClickListener} that will be called when the undo button is
     *                 clicked.
     * @param template Format String to display on popup (e.g. "Closed %s").
     * @param description Text showing at start of snackbar.
     * @param actionText Text of action button at the end of snackbar.
     */
    public SnackbarPopupWindow(View parent, OnClickListener listener, String template,
            String description, String actionText) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.snack_bar, null);
        setContentView(view);
        mMessageView = (TemplatePreservingTextView) view.findViewById(R.id.undobar_message);
        mActionButtonView = (TextView) view.findViewById(R.id.undobar_button);
        setTextViews(template, description, actionText, false);
        mAnimationDuration = view.getResources().getInteger(
                android.R.integer.config_mediumAnimTime);
        mActionButtonView.setOnClickListener(listener);

        // Set width and height of popup window
        boolean isTablet = DeviceFormFactor.isTablet(parent.getContext());
        setWidth(isTablet
                ? parent.getResources().getDimensionPixelSize(R.dimen.undo_bar_tablet_width)
                : parent.getWidth());

        setHeight(parent.getResources().getDimensionPixelSize(R.dimen.undo_bar_height));
        setAnimationStyle(R.style.SnackbarAnimation);
    }

    @Override
    public void dismiss() {
        // Disable action button during animation.
        mActionButtonView.setEnabled(false);
        super.dismiss();
    }

    /**
     * Sets the text of TextViews in snackbar. If this popup bar is already visible it
     * will animate the new text alpha, otherwise it will only set the text.
     * @param template Template that description textview takes to format string.
     * @param description The content text of that show at start of snackbar.
     * @param actionText Text for action button to show.
     * @param animate Whether or not to animate the text in or set it.
     */
    public void setTextViews(String template, String description, String actionText,
            boolean animate) {
        mMessageView.setTemplate(template);
        setViewText(mMessageView, description, animate);
        setViewText(mActionButtonView, actionText, animate);
    }

    private void setViewText(TextView view, String text, boolean animate) {
        if (view.getText().toString().equals(text)) return;
        view.animate().cancel();
        if (animate) {
            view.setAlpha(0.0f);
            view.setText(text);
            view.animate().alpha(1.f).setDuration(mAnimationDuration).setListener(null);
        } else {
            view.setText(text);
        }
    }

    /**
     * Sends an accessibility event to mMessageView announcing that this window was added so that
     * the mMessageView content description is read aloud if accessibility is enabled.
     */
    public void announceforAccessibility() {
        mMessageView.announceForAccessibility(mMessageView.getContentDescription());
    }
}