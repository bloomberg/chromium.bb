// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.interests;

import android.content.Context;
import android.util.AttributeSet;
import android.util.LruCache;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.GridView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.interests.InterestsItemView.DrawingData;
import org.chromium.chrome.browser.ntp.interests.InterestsItemView.ImageHolder;
import org.chromium.chrome.browser.ntp.interests.InterestsPage.InterestsClickListener;
import org.chromium.chrome.browser.ntp.interests.InterestsService.Interest;

import java.util.List;

/**
 * Displays a user's Interests in a two column view. A user's Interests are a list of topics (eg.
 * Movies, Artists, Sports Events) that Google Now Context data shows they are interested in.
 */
public class InterestsView extends FrameLayout {
    private InterestsClickListener mListener;
    private GridView mInterestsGrid;
    private final InterestsListAdapter mAdapter;

    public InterestsView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new InterestsListAdapter(context);
    }

    /**
     * This must be called before {@link InterestsView setInterests}.
     * @param listener
     */
    public void setListener(InterestsClickListener listener) {
        mListener = listener;
    }

    /**
     * Sets the Interests to display. Must not be called before {@link InterestsView setListener}.
     * @param interests
     */
    public void setInterests(List<Interest> interests) {
        assert mListener != null;
        mAdapter.addAll(interests);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mInterestsGrid = (GridView) findViewById(R.id.interests_list_view);
        mInterestsGrid.setAdapter(mAdapter);
    }

    private class InterestsListAdapter extends ArrayAdapter<Interest> {
        private final LruCache<String, ImageHolder> mImageCache;
        private final DrawingData mDrawingData;

        public InterestsListAdapter(Context context) {
            super(context, 0);
            mImageCache = new LruCache<>(30);
            mDrawingData = new InterestsItemView.DrawingData(getContext());
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            assert mListener != null;

            Interest interest = getItem(position);

            InterestsItemView view;
            if (convertView instanceof InterestsItemView) {
                view = (InterestsItemView) convertView;
                view.reset(interest);
            } else {
                view = new InterestsItemView(getContext(), interest,
                        mListener, mImageCache, mDrawingData);
            }
            return view;
        }
    }
}
