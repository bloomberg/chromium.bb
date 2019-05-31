// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.ref.WeakReference;

/**
 * {@link RecyclerView.ViewHolder} for tab grid. Owns the tab info card
 * and the associated view hierarchy.
 */
class TabGridViewHolder extends RecyclerView.ViewHolder {
    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    public final ImageView favicon;
    public final TextView title;
    public final ImageView thumbnail;
    public final ImageView closeButton;
    public final ButtonCompat createGroupButton;
    public final View backgroundView;
    private int mTabId;

    private TabGridViewHolder(View itemView) {
        super(itemView);
        this.thumbnail = itemView.findViewById(R.id.tab_thumbnail);
        this.title = itemView.findViewById(R.id.tab_title);
        // TODO(yuezhanggg): Remove this when the strip is properly tinted. (crbug/939915)
        title.setTextColor(ContextCompat.getColor(
                itemView.getContext(), org.chromium.chrome.R.color.default_text_color_dark));
        this.favicon = itemView.findViewById(R.id.tab_favicon);
        this.closeButton = itemView.findViewById(R.id.close_button);
        this.createGroupButton = itemView.findViewById(R.id.create_group_button);
        this.backgroundView = itemView.findViewById(R.id.background_view);

        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) itemView.getResources().getDimension(R.dimen.tab_grid_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(
                    itemView.getResources(), org.chromium.chrome.R.drawable.btn_close);
            sCloseButtonBitmapWeakRef = new WeakReference<>(
                    Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true));
            bitmap.recycle();
        }
        this.closeButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
    }

    public static TabGridViewHolder create(ViewGroup parent, int itemViewType) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.tab_grid_card_item, parent, false);
        return new TabGridViewHolder(view);
    }

    public void setTabId(int tabId) {
        mTabId = tabId;
    }

    public int getTabId() {
        return mTabId;
    }

    public void resetThumbnail() {
        thumbnail.setImageResource(0);
        thumbnail.setMinimumHeight(thumbnail.getWidth());
    }
}
