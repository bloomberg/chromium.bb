// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.toolbar;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.widget.EditText;

import org.chromium.base.ObserverList;
import org.chromium.ui.UiUtils;

/**
 * A {@link View} that allows users to enter text and URLs.
 */
public class UrlBar extends EditText implements OnKeyListener {
    /**
     * An observer that gets notified of relevant URL bar text changes.
     */
    public interface UrlBarObserver {
        /**
         * Will be called when the user presses enter after typing something into this
         * {@link EditText}.
         * @param text The new text the user entered.
         */
        void onNewTextEntered(String text);
    }

    private final ObserverList<UrlBarObserver> mObservers = new ObserverList<UrlBarObserver>();

    /**
     * Builds a new {@link UrlBar}.
     * @param context A {@link Context} instance.
     * @param attrs   An {@link AttributeSet} instance.
     */
    public UrlBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOnKeyListener(this);
    }

    /**
     * @return The current text of this {@link EditText}.  Guaranteed to be not {@code null}.
     */
    public String getQueryText() {
        return getEditableText() == null ? "" : getEditableText().toString();
    }

    /**
     * @param observer An {@link UrlBarObserver} that will be notified of future text entries.
     */
    public void addUrlBarObserver(UrlBarObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer An {@link UrlBarObserver} that will no longer be notified of future text
     *                 entries.
     */
    public void removeUrlBarObserver(UrlBarObserver observer) {
        mObservers.removeObserver(observer);
    }

    // OnKeyListener interface.
    @Override
    public boolean onKey(View view, int keyCode, KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP
                && (event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                || event.getKeyCode() == KeyEvent.KEYCODE_NUMPAD_ENTER)) {
            UiUtils.hideKeyboard(view);
            clearFocus();
            for (UrlBarObserver observer : mObservers) {
                observer.onNewTextEntered(getQueryText());
            }
        }
        return false;
    }
}
