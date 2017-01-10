// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.selection.SelectionToolbar;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/**
 * The SelectionToolbar for the browsing history UI.
 */
public class HistoryManagerToolbar extends SelectionToolbar<HistoryItem>
        implements OnEditorActionListener {
    private HistoryManager mManager;
    private boolean mIsSearching;

    private LinearLayout mSearchView;
    private EditText mSearchEditText;
    private TintedImageButton mDeleteTextButton;

    public HistoryManagerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.history_manager_menu);

        updateMenuItemVisibility();
        // TODO(twellington): Add content descriptions to the number roll view.
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSearchView = (LinearLayout) findViewById(R.id.history_search);

        mSearchEditText = (EditText) findViewById(R.id.history_search_text);
        mSearchEditText.setOnEditorActionListener(this);
        mSearchEditText.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                mDeleteTextButton.setVisibility(
                        TextUtils.isEmpty(s) ? View.INVISIBLE : View.VISIBLE);
                if (mIsSearching) mManager.performSearch(s.toString());
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void afterTextChanged(Editable s) {}
        });

        mDeleteTextButton = (TintedImageButton) findViewById(R.id.delete_button);
        mDeleteTextButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mSearchEditText.setText("");
            }
        });
    }

    /**
     * @param manager The {@link HistoryManager} associated with this toolbar.
     */
    public void setManager(HistoryManager manager) {
        mManager = manager;
    }

    /**
     * Shows the search edit text box and related views.
     */
    public void showSearchView() {
        mIsSearching = true;
        mSelectionDelegate.clearSelection();

        getMenu().setGroupVisible(mNormalGroupResId, false);
        getMenu().setGroupVisible(mSelectedGroupResId, false);
        setNavigationButton(NAVIGATION_BUTTON_BACK);
        mSearchView.setVisibility(View.VISIBLE);

        mSearchEditText.requestFocus();
        UiUtils.showKeyboard(mSearchEditText);
        setTitle(null);
    }

    /**
     * Hides the search edit text box and related views.
     */
    public void hideSearchView() {
        mIsSearching = false;
        mSearchEditText.setText("");
        UiUtils.hideKeyboard(mSearchEditText);
        mSearchView.setVisibility(View.GONE);

        mManager.onEndSearch();
    }

    @Override
    public void onSelectionStateChange(List<HistoryItem> selectedItems) {
        boolean wasSelectionEnabled = mIsSelectionEnabled;
        super.onSelectionStateChange(selectedItems);

        if (!wasSelectionEnabled && mIsSelectionEnabled) {
            mManager.recordUserActionWithOptionalSearch("SelectionEstablished");
        }

        if (!mIsSearching) return;

        if (mIsSelectionEnabled) {
            mSearchView.setVisibility(View.GONE);
            UiUtils.hideKeyboard(mSearchEditText);
        } else {
            mSearchView.setVisibility(View.VISIBLE);
            getMenu().setGroupVisible(mNormalGroupResId, false);
            setNavigationButton(NAVIGATION_BUTTON_BACK);
        }
    }

    @Override
    protected void onDataChanged(int numItems) {
        getMenu().findItem(R.id.search_menu_id).setVisible(!mIsSearching && numItems != 0);
    }

    @Override
    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
        if (actionId == EditorInfo.IME_ACTION_SEARCH) {
            UiUtils.hideKeyboard(v);
        }
        return false;
    }

    @Override
    protected void onNavigationBack() {
        hideSearchView();

        // Call #onSelectionStateChange() to reset toolbar buttons and title.
        super.onSelectionStateChange(mSelectionDelegate.getSelectedItems());
    }

    /**
     * Should be called when the user's sign in state changes.
     */
    public void onSignInStateChange() {
        updateMenuItemVisibility();
    }

    private void updateMenuItemVisibility() {
        if (DeviceFormFactor.isTablet(getContext())) {
            getMenu().removeItem(R.id.close_menu_id);
        }

        // Once the selection mode delete or incognito menu options are removed, they will not
        // be added back until the user refreshes the history UI. This could happen if the user is
        // signed in to an account that cannot remove browsing history or has incognito disabled and
        // signs out.
        if (!PrefServiceBridge.getInstance().canDeleteBrowsingHistory()) {
            getMenu().removeItem(R.id.selection_mode_delete_menu_id);
        }
        if (!PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
            getMenu().removeItem(R.id.selection_mode_open_in_incognito);
        }
    }

    @VisibleForTesting
    MenuItem getItemById(int menuItemId) {
        Menu menu = getMenu();
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == menuItemId) return item;
        }
        return null;
    }

    @VisibleForTesting
    Menu getMenuForTest() {
        return getMenu();
    }
}
