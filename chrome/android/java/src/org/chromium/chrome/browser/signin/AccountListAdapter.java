// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.ProfileDataCache;

/**
* Adapter for AccountListView. It associates an array of account names on the device and
* provides account views of these accounts.
*/
public class AccountListAdapter extends ArrayAdapter<String> {
    private final LayoutInflater mInflater;
    private final ProfileDataCache mProfileData;
    private int mSelectedAccountPosition = 0;

    public AccountListAdapter(Context context, ProfileDataCache profileData) {
        super(context, 0);
        mInflater = LayoutInflater.from(context);
        mProfileData = profileData;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view = convertView;
        if (view == null) {
            view = mInflater.inflate(R.layout.account_signin_account_view, parent, false);
        }

        // Sets account profile image, name and selection status.
        String accountName = getItem(position);
        ImageView accountImage = (ImageView) view.findViewById(R.id.account_image);
        // The view at the last position is the "Add account" view.
        if (position == getCount() - 1) {
            accountImage.setImageResource(R.drawable.add_circle_blue);
        } else {
            accountImage.setImageBitmap(mProfileData.getImage(accountName));
        }
        ((TextView) view.findViewById(R.id.account_name)).setText(accountName);
        if (position == mSelectedAccountPosition) {
            view.findViewById(R.id.account_selection_mark).setVisibility(View.VISIBLE);
        } else {
            view.findViewById(R.id.account_selection_mark).setVisibility(View.GONE);
        }

        return view;
    }

    /**
     * Sets selected account position.
     * @param position Position in the collection of the associated items.
     */
    public void setSelectedAccountPosition(int position) {
        mSelectedAccountPosition = position;
    }

    /**
     * Gets selected account position.
     */
    public int getSelectedAccountPosition() {
        return mSelectedAccountPosition;
    }
}
