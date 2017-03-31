// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.List;

/**
 * Takes a list of {@link ContextMenuItem} and puts them in an adapter meant to be used within a
 * list view.
 */
class TabularContextMenuListAdapter extends BaseAdapter {
    private final List<ContextMenuItem> mMenuItems;
    private final Context mContext;

    /**
     * Adapter for the tabular context menu UI
     * @param menuItems The list of items to display in the view.
     * @param context Used to inflate the layout.
     */
    TabularContextMenuListAdapter(List<ContextMenuItem> menuItems, Context context) {
        mMenuItems = menuItems;
        mContext = context;
    }

    @Override
    public int getCount() {
        return mMenuItems.size();
    }

    @Override
    public Object getItem(int position) {
        return mMenuItems.get(position);
    }

    @Override
    public long getItemId(int position) {
        return mMenuItems.get(position).menuId;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        ContextMenuItem menuItem = mMenuItems.get(position);
        ViewHolderItem viewHolder;

        if (convertView == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            convertView = inflater.inflate(R.layout.tabular_context_menu_row, null);

            viewHolder = new ViewHolderItem();
            viewHolder.mIcon = (ImageView) convertView.findViewById(R.id.context_menu_icon);
            viewHolder.mText = (TextView) convertView.findViewById(R.id.context_text);

            convertView.setTag(viewHolder);
        } else {
            viewHolder = (ViewHolderItem) convertView.getTag();
        }

        viewHolder.mText.setText(menuItem.getString(mContext));
        Drawable icon = menuItem.getDrawableAndDescription(mContext);
        viewHolder.mIcon.setImageDrawable(icon);
        viewHolder.mIcon.setVisibility(icon != null ? View.VISIBLE : View.INVISIBLE);
        return convertView;
    }

    private static class ViewHolderItem {
        ImageView mIcon;
        TextView mText;
    }
}
