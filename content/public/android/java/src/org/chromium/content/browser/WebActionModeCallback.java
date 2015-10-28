// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.TargetApi;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import org.chromium.content.R;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/**
 * An ActionMode.Callback for in-page web content selection. This class handles both the editable
 * and non-editable cases.
 */
public class WebActionModeCallback implements ActionMode.Callback {
    /**
     * An interface to retrieve information about the current selection, and also to perform
     * actions based on the selection or when the action bar is dismissed.
     */
    public interface ActionHandler {
        /**
         * Perform a select all action.
         */
        void selectAll();

        /**
         * Perform a copy (to clipboard) action.
         */
        void copy();

        /**
         * Perform a cut (to clipboard) action.
         */
        void cut();

        /**
         * Perform a paste action.
         */
        void paste();

        /**
         * Perform a share action.
         */
        void share();

        /**
         * Perform a processText action (translating the text, for example).
         */
        void processText(Intent intent);

        /**
         * Perform a search action.
         */
        void search();

        /**
         * @return true iff the current selection is editable (e.g. text within an input field).
         */
        boolean isSelectionEditable();

        /**
         * Called when the onDestroyActionMode of the SelectActionmodeCallback is called.
         */
        void onDestroyActionMode();

        /**
         * Called when the onGetContentRect of the SelectActionModeCallback is called.
         * @param outRect The Rect to be populated with the content position.
         */
        void onGetContentRect(Rect outRect);

        /**
         * @return true if the current selection is of password type.
         */
        boolean isSelectionPassword();

        /**
         * @return true if the current selection is an insertion point.
         */
        boolean isInsertion();

        /**
         * @return true if the current selection is for incognito content.
         *         Note: This should remain constant for the callback's lifetime.
         */
        boolean isIncognito();

        /**
         * @param actionModeItem the flag for the action mode item in question. The valid flags are
         *        {@link #MENU_ITEM_SHARE}, {@link #MENU_ITEM_WEB_SEARCH}, and
         *        {@link #MENU_ITEM_PROCESS_TEXT}.
         * @return true if the menu item action is allowed. Otherwise, the menu item
         *         should be removed from the menu.
         */
        boolean isSelectActionModeAllowed(int actionModeItem);
    }

    // TODO(hush): Use these constants from android.webkit.WebSettings, when they are made
    // available. crbug.com/546762.
    public static final int MENU_ITEM_SHARE = 1 << 0;
    public static final int MENU_ITEM_WEB_SEARCH = 1 << 1;
    public static final int MENU_ITEM_PROCESS_TEXT = 1 << 2;

    protected final ActionHandler mActionHandler;
    private final Context mContext;
    private boolean mEditable;
    private boolean mIsPasswordType;
    private boolean mIsInsertion;
    private boolean mIsDestroyed;

    public WebActionModeCallback(Context context, ActionHandler actionHandler) {
        mContext = context;
        mActionHandler = actionHandler;
    }

    protected Context getContext() {
        return mContext;
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        mode.setTitle(DeviceFormFactor.isTablet(getContext())
                        ? getContext().getString(R.string.actionbar_textselection_title)
                        : null);
        mode.setSubtitle(null);
        mEditable = mActionHandler.isSelectionEditable();
        mIsPasswordType = mActionHandler.isSelectionPassword();
        mIsInsertion = mActionHandler.isInsertion();
        createActionMenu(mode, menu);
        return true;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        boolean isEditableNow = mActionHandler.isSelectionEditable();
        boolean isPasswordNow = mActionHandler.isSelectionPassword();
        boolean isInsertionNow = mActionHandler.isInsertion();
        if (mEditable != isEditableNow || mIsPasswordType != isPasswordNow
                || mIsInsertion != isInsertionNow) {
            mEditable = isEditableNow;
            mIsPasswordType = isPasswordNow;
            mIsInsertion = isInsertionNow;
            menu.clear();
            createActionMenu(mode, menu);
            return true;
        }
        return false;
    }

    private void createActionMenu(ActionMode mode, Menu menu) {
        try {
            mode.getMenuInflater().inflate(R.menu.select_action_menu, menu);
        } catch (Resources.NotFoundException e) {
            // TODO(tobiasjs) by the time we get here we have already
            // caused a resource loading failure to be logged. WebView
            // resource access needs to be improved so that this
            // logspam can be avoided.
            new MenuInflater(getContext()).inflate(R.menu.select_action_menu, menu);
        }

        if (mIsInsertion) {
            menu.removeItem(R.id.select_action_menu_select_all);
            menu.removeItem(R.id.select_action_menu_cut);
            menu.removeItem(R.id.select_action_menu_copy);
            menu.removeItem(R.id.select_action_menu_share);
            menu.removeItem(R.id.select_action_menu_web_search);
            return;
        }

        if (!mEditable || !canPaste()) {
            menu.removeItem(R.id.select_action_menu_paste);
        }

        if (!mEditable) {
            menu.removeItem(R.id.select_action_menu_cut);
        }

        if (mEditable || !mActionHandler.isSelectActionModeAllowed(MENU_ITEM_SHARE)) {
            menu.removeItem(R.id.select_action_menu_share);
        }

        if (mEditable || mActionHandler.isIncognito()
                || !mActionHandler.isSelectActionModeAllowed(MENU_ITEM_WEB_SEARCH)) {
            menu.removeItem(R.id.select_action_menu_web_search);
        }

        if (mIsPasswordType) {
            menu.removeItem(R.id.select_action_menu_copy);
            menu.removeItem(R.id.select_action_menu_cut);
            return;
        }

        initializeTextProcessingMenu(menu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        if (mIsDestroyed) return true;

        int id = item.getItemId();
        int groupId = item.getGroupId();

        if (id == R.id.select_action_menu_select_all) {
            mActionHandler.selectAll();
        } else if (id == R.id.select_action_menu_cut) {
            mActionHandler.cut();
            mode.finish();
        } else if (id == R.id.select_action_menu_copy) {
            mActionHandler.copy();
            mode.finish();
        } else if (id == R.id.select_action_menu_paste) {
            mActionHandler.paste();
            mode.finish();
        } else if (id == R.id.select_action_menu_share) {
            mActionHandler.share();
            mode.finish();
        } else if (id == R.id.select_action_menu_web_search) {
            mActionHandler.search();
            mode.finish();
        } else if (groupId == R.id.select_action_menu_text_processing_menus) {
            mActionHandler.processText(item.getIntent());
            // The ActionMode is not dismissed to match the behavior with
            // TextView in Android M.
        } else {
            return false;
        }
        return true;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {
        mIsDestroyed = true;
        mActionHandler.onDestroyActionMode();
    }

    /**
     * Called when an ActionMode needs to be positioned on screen, potentially occluding view
     * content. Note this may be called on a per-frame basis.
     *
     * @param mode The ActionMode that requires positioning.
     * @param view The View that originated the ActionMode, in whose coordinates the Rect should
     *             be provided.
     * @param outRect The Rect to be populated with the content position.
     */
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        if (mIsDestroyed) return;
        mActionHandler.onGetContentRect(outRect);
    }

    private boolean canPaste() {
        ClipboardManager clipMgr = (ClipboardManager)
                getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        return clipMgr.hasPrimaryClip();
    }

    /**
     * Intialize the menu items for processing text, if there is any.
     */
    private void initializeTextProcessingMenu(Menu menu) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M
                || !mActionHandler.isSelectActionModeAllowed(MENU_ITEM_PROCESS_TEXT)) {
            return;
        }

        PackageManager packageManager = getContext().getPackageManager();
        List<ResolveInfo> supportedActivities =
                packageManager.queryIntentActivities(createProcessTextIntent(), 0);
        for (int i = 0; i < supportedActivities.size(); i++) {
            ResolveInfo resolveInfo = supportedActivities.get(i);
            CharSequence label = resolveInfo.loadLabel(getContext().getPackageManager());
            menu.add(R.id.select_action_menu_text_processing_menus, Menu.NONE, i, label)
                    .setIntent(createProcessTextIntentForResolveInfo(resolveInfo))
                    .setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    private Intent createProcessTextIntent() {
        return new Intent().setAction(Intent.ACTION_PROCESS_TEXT).setType("text/plain");
    }

    @TargetApi(Build.VERSION_CODES.M)
    private Intent createProcessTextIntentForResolveInfo(ResolveInfo info) {
        boolean isReadOnly = !mActionHandler.isSelectionEditable();
        return createProcessTextIntent()
                .putExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, isReadOnly)
                .setClassName(info.activityInfo.packageName, info.activityInfo.name);
    }
}
