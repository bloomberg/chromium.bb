// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.omnibox;

import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.EditText;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow.OnDismissListener;


import org.chromium.chrome.browser.omnibox.AutocompleteController;
import org.chromium.chrome.browser.omnibox.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellToolbar;
import org.chromium.chrome.shell.R;

import java.util.List;

/**
 * Displays suggestions for the text that is entered to the ChromeShell URL field.
 */
public class SuggestionPopup implements OnSuggestionsReceivedListener, TextWatcher {
    private static final long SUGGESTION_START_DELAY_MS = 30;

    private final Context mContext;
    private final EditText mUrlField;
    private final ChromeShellToolbar mToolbar;
    private final AutocompleteController mAutocomplete;

    private boolean mHasStartedNewOmniboxEditSession;
    private Runnable mRequestSuggestions;
    private ListPopupWindow mSuggestionsPopup;
    private SuggestionArrayAdapter mSuggestionArrayAdapter;
    private int mSuggestionsPopupItemsCount;

    /**
     * Initializes a suggestion popup that will track urlField value and display suggestions based
     * on that value.
     */
    public SuggestionPopup(Context context, EditText urlField,
            ChromeShellToolbar toolbar) {
        mContext = context;
        mUrlField = urlField;
        mToolbar = toolbar;
        mAutocomplete = new AutocompleteController(this);
        OnLayoutChangeListener listener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (mSuggestionsPopup == null || !mSuggestionsPopup.isShowing()) return;
                mSuggestionsPopup.setWidth(mUrlField.getWidth());
                mSuggestionsPopup.setHeight(getSuggestionPopupHeight());
                mSuggestionsPopup.show();
            }
        };
        mUrlField.addOnLayoutChangeListener(listener);
    }

    private void navigateToSuggestion(int position) {
        mToolbar.getCurrentTab().loadUrlWithSanitization(
                mSuggestionArrayAdapter.getItem(position).getUrl());
        mUrlField.clearFocus();
        mToolbar.setKeyboardVisibilityForUrl(false);
        mToolbar.getCurrentTab().getView().requestFocus();
        dismissPopup();
    }

    public void dismissPopup() {
        if (mSuggestionsPopup != null) {
            mSuggestionsPopup.dismiss();
            mSuggestionsPopup = null;
        }
    }

    /**
     * Stops the autocomplete controller and closes the suggestion popup.
     */
    public void hideSuggestions() {
        stopAutocomplete(true);
        dismissPopup();
    }

    /**
     * Signals the autocomplete controller to stop generating suggestions and
     * cancels the queued task to start the autocomplete controller, if any.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    private void stopAutocomplete(boolean clear) {
        if (mAutocomplete != null) mAutocomplete.stop(clear);
        if (mRequestSuggestions != null) mRequestSuggestions = null;
    }

    private int getSuggestionPopupHeight() {
        Rect appRect = new Rect();
        ((ChromeShellActivity) mContext).getWindow().getDecorView().
                getWindowVisibleDisplayFrame(appRect);
        int dropDownItemHeight = mContext.getResources().
                getDimensionPixelSize(R.dimen.dropdown_item_height);
        // Applying margin height equal to |dropDownItemHeight| if constrained by app rect.
        int popupHeight = appRect.height() - dropDownItemHeight;
        if (mSuggestionsPopup != null) {
            int height = mSuggestionsPopupItemsCount * dropDownItemHeight;
            if (height < popupHeight)
                popupHeight = height;
        }
        return popupHeight;
    }

    // OnSuggestionsReceivedListener implementation
    @Override
    public void onSuggestionsReceived(List<OmniboxSuggestion> suggestions,
            String inlineAutocompleteText) {
        if (!mUrlField.isFocused() || suggestions.isEmpty())
            return;
        mSuggestionsPopupItemsCount = suggestions.size();
        if (mSuggestionsPopup == null) {
            mSuggestionsPopup = new ListPopupWindow(
                    mContext, null, android.R.attr.autoCompleteTextViewStyle);
            mSuggestionsPopup.setOnDismissListener(new OnDismissListener() {
                @Override
                public void onDismiss() {
                    mHasStartedNewOmniboxEditSession = false;
                    mSuggestionArrayAdapter = null;
                }
            });
        }
        mSuggestionsPopup.setInputMethodMode(ListPopupWindow.INPUT_METHOD_NEEDED);
        mSuggestionsPopup.setWidth(mUrlField.getWidth());
        mSuggestionArrayAdapter =
                new SuggestionArrayAdapter(mContext, R.layout.dropdown_item, suggestions,
                        mUrlField);
        mSuggestionsPopup.setHeight(getSuggestionPopupHeight());
        mSuggestionsPopup.setAdapter(mSuggestionArrayAdapter);
        mSuggestionsPopup.setAnchorView(mUrlField);
        mSuggestionsPopup.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                navigateToSuggestion(position);
            }
        });
        mSuggestionsPopup.show();
    }

    // TextWatcher implementation

    @Override
    public void afterTextChanged(final Editable editableText) {
        if (!mUrlField.hasFocus()) return;
        if (!mHasStartedNewOmniboxEditSession) {
            mAutocomplete.resetSession();
            mHasStartedNewOmniboxEditSession = true;
        }

        stopAutocomplete(false);
        if (TextUtils.isEmpty(editableText)) {
            dismissPopup();
        } else {
            assert mRequestSuggestions == null : "Multiple omnibox requests in flight.";
            mRequestSuggestions = new Runnable() {
                @Override
                public void run() {
                    mRequestSuggestions = null;
                    mAutocomplete.start(
                            mToolbar.getCurrentTab().getProfile(),
                            mToolbar.getCurrentTab().getUrl(),
                            editableText.toString(), false);
                }
            };
            new Handler().postDelayed(mRequestSuggestions, SUGGESTION_START_DELAY_MS);
        }
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        mRequestSuggestions = null;
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
    }
}
