// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.List;

// TODO(sinansahin): Convert ContextMenuItem to a PropertyModel. This will include changes to
// setHeaderTitle(), setHeaderUrl(), and getHeaderImage()
class RevampedContextMenuListAdapter extends BaseAdapter {
    private final List<Pair<Integer, ContextMenuItem>> mMenuItems;

    private String mHeaderTitle;
    private String mHeaderUrl;
    private ImageView mHeaderImage;

    public RevampedContextMenuListAdapter(List<Pair<Integer, ContextMenuItem>> menuItems) {
        mMenuItems = menuItems;
    }

    @Override
    public Pair<Integer, ContextMenuItem> getItem(int position) {
        return mMenuItems.get(position);
    }

    @Override
    public int getCount() {
        return mMenuItems.size();
    }

    @Override
    public long getItemId(int position) {
        if (mMenuItems.get(position).first
                == RevampedContextMenuController.ListItemType.CONTEXT_MENU_ITEM) {
            return mMenuItems.get(position).second.getMenuId();
        }
        return 0;
    }

    @Override
    public int getViewTypeCount() {
        return RevampedContextMenuController.ListItemType.TYPE_COUNT;
    }

    @Override
    public int getItemViewType(int position) {
        return mMenuItems.get(position).first;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            int layout;

            if (position == 0) {
                layout = R.layout.revamped_context_menu_header;
                convertView = LayoutInflater.from(parent.getContext()).inflate(layout, null);

                mHeaderImage = convertView.findViewById(R.id.menu_header_image);

                TextView titleText = convertView.findViewById(R.id.menu_header_title);
                titleText.setText(mHeaderTitle);

                TextView urlText = convertView.findViewById(R.id.menu_header_url);
                urlText.setText(mHeaderUrl);

                if (TextUtils.isEmpty(mHeaderTitle)) {
                    titleText.setVisibility(View.GONE);
                    urlText.setMaxLines(2);
                }

                if (TextUtils.isEmpty(mHeaderUrl)) {
                    urlText.setVisibility(View.GONE);
                    titleText.setMaxLines(2);
                } else {
                    // Expand or shrink the URL on click
                    urlText.setOnClickListener(view -> {
                        if (urlText.getMaxLines() == Integer.MAX_VALUE) {
                            urlText.setMaxLines(TextUtils.isEmpty(mHeaderTitle) ? 2 : 1);
                            urlText.setEllipsize(TextUtils.TruncateAt.END);
                        } else {
                            urlText.setMaxLines(Integer.MAX_VALUE);
                            urlText.setEllipsize(null);
                        }
                    });
                }
            } else if (getItem(position).first
                    == RevampedContextMenuController.ListItemType.DIVIDER) {
                // TODO(sinansahin): divider_preference can be renamed to horizontal_divider
                layout = R.layout.divider_preference;
                convertView = LayoutInflater.from(parent.getContext()).inflate(layout, null);
            } else {
                layout = R.layout.revamped_context_menu_row;
                convertView = LayoutInflater.from(parent.getContext()).inflate(layout, null);

                TextView rowText = (TextView) convertView;
                rowText.setText(getItem(position).second.getTitle(parent.getContext()));
            }
        }

        return convertView;
    }

    public void setHeaderTitle(String headerTitle) {
        mHeaderTitle = headerTitle;
    }

    public void setHeaderUrl(String headerUrl) {
        mHeaderUrl = headerUrl;
    }

    public ImageView getHeaderImage() {
        return mHeaderImage;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return false;
    }

    @Override
    public boolean isEnabled(int position) {
        return getItem(position).first
                == RevampedContextMenuController.ListItemType.CONTEXT_MENU_ITEM;
    }
}
