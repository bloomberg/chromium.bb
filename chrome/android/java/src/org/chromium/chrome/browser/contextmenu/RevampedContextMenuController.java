// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.support.v7.app.AlertDialog;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ContextMenuDialog;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * A customized context menu that displays the options in a list with the first item being
 * the header and represents the the options in groups.
 */
public class RevampedContextMenuController
        implements ContextMenuUi, AdapterView.OnItemClickListener {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.HEADER, ListItemType.CONTEXT_MENU_ITEM,
            ListItemType.TYPE_COUNT})
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;

        // Keep this up-to-date
        int TYPE_COUNT = 3;
    }

    private ContextMenuDialog mContextMenuDialog;
    private RevampedContextMenuListAdapter mListAdapter;
    private Callback<Integer> mItemClickCallback;
    private float mTopContentOffsetPx;

    /**
     * Constructor that also sets the content offset.
     *
     * @param topContentOffsetPx content offset from the top.
     */
    public RevampedContextMenuController(float topContentOffsetPx) {
        mTopContentOffsetPx = topContentOffsetPx;
    }

    @Override
    public void displayMenu(final Activity activity, ContextMenuParams params,
            List<Pair<Integer, List<ContextMenuItem>>> items, Callback<Integer> onItemClicked,
            final Runnable onMenuShown, final Runnable onMenuClosed) {
        mItemClickCallback = onItemClicked;
        float density = Resources.getSystem().getDisplayMetrics().density;
        final float touchPointXPx = params.getTriggeringTouchXDp() * density;
        final float touchPointYPx = params.getTriggeringTouchYDp() * density;

        final View view =
                LayoutInflater.from(activity).inflate(R.layout.revamped_context_menu, null);
        mContextMenuDialog = createContextMenuDialog(activity, view, touchPointXPx, touchPointYPx);
        mContextMenuDialog.setOnShowListener(dialogInterface -> { onMenuShown.run(); });
        mContextMenuDialog.setOnDismissListener(dialogInterface -> { onMenuClosed.run(); });

        // Integer here indicates if the item is the header, a divider, or a row (selectable option)
        List<Pair<Integer, ContextMenuItem>> itemList = new ArrayList<>();
        itemList.add(new Pair<>(ListItemType.HEADER, null));

        for (Pair<Integer, List<ContextMenuItem>> group : items) {
            // Add a divider
            itemList.add(new Pair<>(ListItemType.DIVIDER, null));

            for (ContextMenuItem item : group.second) {
                itemList.add(new Pair<>(ListItemType.CONTEXT_MENU_ITEM, item));
            }
        }

        mListAdapter = new RevampedContextMenuListAdapter(itemList);
        mListAdapter.setHeaderTitle(params.getTitleText());
        mListAdapter.setHeaderUrl(params.getLinkUrl());
        RevampedContextMenuListView listView = view.findViewById(R.id.context_menu_list_view);
        listView.setAdapter(mListAdapter);
        listView.setOnItemClickListener(this);

        mContextMenuDialog.show();
    }

    /**
     * Returns the fully complete dialog based off the params and the itemGroups.
     *
     * @param activity Used to inflate the dialog.
     * @param view The inflated view that contains the list view.
     * @param touchPointYPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointXPx The y-coordinate of the touch that triggered the context menu.
     * @return Returns a final dialog that does not have a background can be displayed using
     *         {@link AlertDialog#show()}.
     */
    private ContextMenuDialog createContextMenuDialog(
            Activity activity, View view, float touchPointXPx, float touchPointYPx) {
        View listView = view.findViewById(R.id.context_menu_list_view);
        final ContextMenuDialog dialog =
                new ContextMenuDialog(activity, R.style.Theme_Chromium_DialogWhenLarge,
                        touchPointXPx, touchPointYPx, mTopContentOffsetPx, listView);
        dialog.setContentView(view);

        return dialog;
    }

    /**
     * This is called when the thumbnail is fetched and ready to display.
     * @param thumbnail The bitmap received that will be displayed as the header image.
     */
    public void onImageThumbnailRetrieved(Bitmap thumbnail) {
        if (thumbnail != null) {
            mListAdapter.getHeaderImage().setImageBitmap(thumbnail);
        } else {
            // TODO(sinansahin): Handle the failed image loads differently.
            mListAdapter.getHeaderImage().setImageResource(R.drawable.sad_tab);
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mContextMenuDialog.dismiss();
        mItemClickCallback.onResult((int) id);
    }
}
