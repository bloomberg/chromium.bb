// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Intent;
import android.os.Build;
import android.text.TextUtils;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.content.R;
import org.chromium.content_public.browser.ActionModeCallbackHelper;

/**
 * A class that handles selection action mode for Android WebView.
 */
public class AwActionModeCallback implements ActionMode.Callback {
    private final AwContents mAwContents;
    private final ActionModeCallbackHelper mHelper;
    private int mAllowedMenuItems;

    public AwActionModeCallback(AwContents awContents, ActionModeCallbackHelper helper) {
        mAwContents = awContents;
        mHelper = helper;
        mHelper.setAllowedMenuItems(0);  // No item is allowed by default for WebView.
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        int allowedItems = (getAllowedMenu(ActionModeCallbackHelper.MENU_ITEM_SHARE)
                | getAllowedMenu(ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH)
                | getAllowedMenu(ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT));
        if (allowedItems != mAllowedMenuItems) {
            mHelper.setAllowedMenuItems(allowedItems);
            mAllowedMenuItems = allowedItems;
        }
        mHelper.onCreateActionMode(mode, menu);
        return true;
    }

    private int getAllowedMenu(int menuItem) {
        return mAwContents.isSelectActionModeAllowed(menuItem) ? menuItem : 0;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return mHelper.onPrepareActionMode(mode, menu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        if (!mHelper.isActionModeValid()) return true;

        int groupId = item.getGroupId();

        if (groupId == R.id.select_action_menu_text_processing_menus) {
            processText(item.getIntent());
            // The ActionMode is not dismissed to match the behavior with
            // TextView in Android M.
        } else {
            return mHelper.onActionItemClicked(mode, item);
        }
        return true;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {
        mHelper.onDestroyActionMode();
    }

    private void processText(Intent intent) {
        RecordUserAction.record("MobileActionMode.ProcessTextIntent");
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;

        String query = mHelper.sanitizeQuery(mHelper.getSelectedText(),
                ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        intent.putExtra(Intent.EXTRA_PROCESS_TEXT, query);
        try {
            mAwContents.startProcessTextIntent(intent);
        } catch (android.content.ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }
}
