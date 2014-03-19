// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.List;

/**
 * ListAdapter to customize the view of items in the list.
 */
class AppMenuAdapter extends BaseAdapter {
    private static final int VIEW_TYPE_MENUITEM = 0;
    private static final int VIEW_TYPE_COUNT = 1;

    private final LayoutInflater mInflater;
    private final List<MenuItem> mMenuItems;
    private final int mNumMenuItems;

    public AppMenuAdapter(List<MenuItem> menuItems, LayoutInflater inflater) {
        mMenuItems = menuItems;
        mInflater = inflater;
        mNumMenuItems = menuItems.size();
    }

    @Override
    public int getCount() {
        return mNumMenuItems;
    }

    @Override
    public int getViewTypeCount() {
        return VIEW_TYPE_COUNT;
    }

    @Override
    public int getItemViewType(int position) {
        return VIEW_TYPE_MENUITEM;
    }

    @Override
    public long getItemId(int position) {
        return getItem(position).getItemId();
    }

    @Override
    public MenuItem getItem(int position) {
        if (position == ListView.INVALID_POSITION) return null;
        assert position >= 0;
        assert position < mMenuItems.size();
        return mMenuItems.get(position);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View rowView = convertView;
        // A ViewHolder keeps references to children views to avoid unneccessary calls
        // to findViewById() on each row.
        ViewHolder holder = null;

        // When convertView is not null, we can reuse it directly, there is no need
        // to reinflate it.
        if (rowView == null) {
            holder = new ViewHolder();
            rowView = mInflater.inflate(R.layout.menu_item, null);
            holder.text = (TextView) rowView.findViewById(R.id.menu_item_text);
            holder.image = (AppMenuItemIcon) rowView.findViewById(R.id.menu_item_icon);
            rowView.setTag(holder);
        } else {
            holder = (ViewHolder) convertView.getTag();
        }
        MenuItem item = getItem(position);

        // Set up the icon.
        Drawable icon = item.getIcon();
        holder.image.setImageDrawable(icon);
        holder.image.setVisibility(icon == null ? View.GONE : View.VISIBLE);
        holder.image.setChecked(item.isChecked());

        holder.text.setText(item.getTitle());
        boolean isEnabled = item.isEnabled();
        // Set the text color (using a color state list).
        holder.text.setEnabled(isEnabled);
        // This will ensure that the item is not highlighted when selected.
        rowView.setEnabled(isEnabled);
        return rowView;
    }

    static class ViewHolder {
        public TextView text;
        public AppMenuItemIcon image;
    }
}