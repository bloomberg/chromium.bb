// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.os.StrictMode;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.Space;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.R;

import java.util.List;

/**
 * Takes a list of {@link ContextMenuItem} and puts them in an adapter meant to be used within a
 * list view.
 */
class TabularContextMenuListAdapter extends BaseAdapter {
    private final List<ContextMenuItem> mMenuItems;
    private final Activity mActivity;
    private final Callback<Boolean> mOnDirectShare;

    /**
     * Adapter for the tabular context menu UI
     * @param menuItems The list of items to display in the view.
     * @param activity Used to inflate the layout.
     * @param onDirectShare Callback to handle direct share.
     */
    TabularContextMenuListAdapter(
            List<ContextMenuItem> menuItems, Activity activity, Callback<Boolean> onDirectShare) {
        mMenuItems = menuItems;
        mActivity = activity;
        mOnDirectShare = onDirectShare;
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
        return mMenuItems.get(position).getMenuId();
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        final ContextMenuItem menuItem = mMenuItems.get(position);
        ViewHolderItem viewHolder;

        if (convertView == null) {
            LayoutInflater inflater = LayoutInflater.from(mActivity);
            convertView = inflater.inflate(R.layout.tabular_context_menu_row, null);

            viewHolder = new ViewHolderItem();
            viewHolder.mIcon = (ImageView) convertView.findViewById(R.id.context_menu_icon);
            viewHolder.mText = (TextView) convertView.findViewById(R.id.context_menu_text);
            if (viewHolder.mText == null) {
                throw new IllegalStateException("Context text not found in new view inflation");
            }
            viewHolder.mShareIcon =
                    (ImageView) convertView.findViewById(R.id.context_menu_share_icon);
            viewHolder.mRightPadding =
                    (Space) convertView.findViewById(R.id.context_menu_right_padding);

            convertView.setTag(viewHolder);
        } else {
            viewHolder = (ViewHolderItem) convertView.getTag();
            if (viewHolder.mText == null) {
                throw new IllegalStateException("Context text not found in view resuse");
            }
        }

        final String titleText = menuItem.getTitle(mActivity);
        viewHolder.mText.setText(titleText);
        viewHolder.mIcon.setVisibility(View.GONE);

        if (menuItem instanceof ShareContextMenuItem) {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                final Pair<Drawable, CharSequence> shareInfo =
                        ((ShareContextMenuItem) menuItem).getShareInfo();
                if (shareInfo.first != null) {
                    viewHolder.mShareIcon.setImageDrawable(shareInfo.first);
                    viewHolder.mShareIcon.setVisibility(View.VISIBLE);
                    viewHolder.mShareIcon.setContentDescription(mActivity.getString(
                            R.string.accessibility_menu_share_via, shareInfo.second));
                    viewHolder.mShareIcon.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View view) {
                            mOnDirectShare.onResult(
                                    ((ShareContextMenuItem) menuItem).isShareLink());
                        }
                    });
                    viewHolder.mRightPadding.setVisibility(View.GONE);
                }
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        } else {
            viewHolder.mShareIcon.setVisibility(View.GONE);
            viewHolder.mRightPadding.setVisibility(View.VISIBLE);
        }

        return convertView;
    }

    private static class ViewHolderItem {
        ImageView mIcon;
        TextView mText;
        ImageView mShareIcon;
        Space mRightPadding;
    }
}
