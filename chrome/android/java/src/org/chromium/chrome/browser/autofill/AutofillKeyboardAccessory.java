// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * The Accessory sitting above the keyboard and below the content area. It is used for autofill
 * suggestions and manual entry points assisting the user in filling forms.
 */
public class AutofillKeyboardAccessory
        extends LinearLayout implements WindowAndroid.KeyboardVisibilityListener {
    private final WindowAndroid mWindowAndroid;

    private AutofillKeyboardSuggestions mAutofillSuggestions;

    // Boolean to track if the keyboard accessory has just popped up or has already been showing.
    private boolean mFirstAppearance;

    /**
     * Creates an AutofillKeyboardAccessory with specified parameters.
     * @param windowAndroid The owning WindowAndroid.
     */
    public AutofillKeyboardAccessory(WindowAndroid windowAndroid) {
        super(windowAndroid.getActivity().get());
        assert windowAndroid.getActivity().get() != null;
        mWindowAndroid = windowAndroid;
        mWindowAndroid.addKeyboardVisibilityListener(this);
    }

    // TODO(crbug/722897): Check to handle RTL.
    public void show() {
        removeAllViews();
        if (mAutofillSuggestions != null) {
            HorizontalScrollView scrollView = new HorizontalScrollView(getContext());
            scrollView.scrollTo(scrollView.getRight(), 0);
            ApiCompatibilityUtils.setLayoutDirection(scrollView,
                    isLayoutRtl() ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
            scrollView.addView(mAutofillSuggestions);
            addView(scrollView);
        }

        final ViewGroup container = mWindowAndroid.getKeyboardAccessoryView();

        if (getParent() == null) {
            container.addView(this);
            container.setVisibility(View.VISIBLE);
            container.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        }
        if (mFirstAppearance) {
            announceForAccessibility(container.getContentDescription());
        }
    }

    /**
     * Shows the given suggestions.
     * @param suggestions Autofill suggestion data.
     */
    public void setSuggestions(AutofillKeyboardSuggestions suggestions) {
        mAutofillSuggestions = suggestions;
    }

    /**
     * Called to hide the suggestion view.
     */
    public void dismiss() {
        UiUtils.hideKeyboard(this); // TODO(fhorschig): Double-check for autofill sheet.
        if (mAutofillSuggestions != null) mAutofillSuggestions.dismiss();
        ViewGroup container = mWindowAndroid.getKeyboardAccessoryView();
        container.removeView(this);
        container.setVisibility(View.GONE);
        mWindowAndroid.removeKeyboardVisibilityListener(this);
        container.getParent().requestLayout();
        // Next time the keyboard accessory appears, do accessibility work.
        mFirstAppearance = true;
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        if (!isShowing) { // TODO(fhorschig): ... and no bottom sheet.
            dismiss();
        }
    }
}
