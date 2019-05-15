// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.annotation.ColorInt;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.ui.widget.RoundedCornerImageView;

import java.util.List;

// TODO(sinansahin): Convert ContextMenuItem to a PropertyModel. This will include changes to
// setHeaderTitle(), setHeaderUrl(), and getHeaderImage()
class RevampedContextMenuListAdapter extends BaseAdapter {
    private final List<Pair<Integer, ContextMenuItem>> mMenuItems;

    private String mHeaderTitle;
    private CharSequence mHeaderUrl;
    private RoundedCornerImageView mHeaderImage;
    private Bitmap mHeaderBitmap;

    private boolean mIsHeaderImageThumbnail;

    private Context mContext;

    public RevampedContextMenuListAdapter(
            Context context, List<Pair<Integer, ContextMenuItem>> menuItems) {
        mContext = context;
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
        return RevampedContextMenuController.ListItemType.NUM_ENTRIES;
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

                // We should've cached the bitmap if the header wasn't ready when we received a
                // thumbnail, favicon, or monogram. We can set the image to the cached bitmap here.
                if (mHeaderBitmap != null) {
                    setHeaderImage(mHeaderBitmap);
                }

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
                layout = R.layout.context_menu_divider;
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

    public void setHeaderUrl(CharSequence headerUrl) {
        mHeaderUrl = headerUrl;
    }

    /**
     * This is called when the thumbnail is fetched and ready to display.
     * @param thumbnail The bitmap received that will be displayed as the header image.
     */
    void onImageThumbnailRetrieved(Bitmap thumbnail) {
        mIsHeaderImageThumbnail = true;
        if (thumbnail != null) {
            setHeaderImage(RevampedContextMenuController.getImageWithCheckerBackground(
                    mContext.getResources(), thumbnail));
        }
        // TODO(sinansahin): Handle the case where the retrieval of the thumbnail fails.
    }

    void onFaviconAvailable(@Nullable Bitmap icon, @ColorInt int fallbackColor, String iconUrl) {
        // If we didn't get a favicon, generate a monogram instead
        if (icon == null) {
            RoundedIconGenerator iconGenerator = createRoundedIconGenerator(fallbackColor);
            icon = iconGenerator.generateIconForUrl(iconUrl);
        }

        mIsHeaderImageThumbnail = false;
        final int size = mContext.getResources().getDimensionPixelSize(
                R.dimen.revamped_context_menu_header_monogram_size);

        if (icon.getWidth() > size) {
            icon = Bitmap.createScaledBitmap(icon, size, size, true);
        }

        setHeaderImage(icon);
    }

    private void setHeaderImage(Bitmap bitmap) {
        // If the HeaderImage hasn't been inflated by the time we receive the bitmap,
        // cache the bitmap received to be set in #getView later.
        if (mHeaderImage == null) {
            mHeaderBitmap = bitmap;
            return;
        }

        // Clear the loading background color now that we have the real image.
        mHeaderImage.setRoundedFillColor(android.R.color.transparent);

        if (mIsHeaderImageThumbnail) {
            mHeaderImage.setImageBitmap(bitmap);
            return;
        }

        ((View) mHeaderImage.getParent())
                .findViewById(R.id.circle_background)
                .setVisibility(View.VISIBLE);
        mHeaderImage.setImageBitmap(bitmap);
    }

    private RoundedIconGenerator createRoundedIconGenerator(@ColorInt int iconColor) {
        final Resources resources = mContext.getResources();
        final int iconSize =
                resources.getDimensionPixelSize(R.dimen.revamped_context_menu_header_monogram_size);
        final int cornerRadius = iconSize / 2;
        final int textSize = resources.getDimensionPixelSize(
                R.dimen.revamped_context_menu_header_monogram_text_size);

        return new RoundedIconGenerator(iconSize, iconSize, cornerRadius, iconColor, textSize);
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
