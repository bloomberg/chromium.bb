// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.ListView;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.ui.base.LocalizationUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * A popup that handles displaying the navigation history for a given tab.
 */
public class NavigationPopup extends ListPopupWindow implements AdapterView.OnItemClickListener {

    private static final int MAXIMUM_HISTORY_ITEMS = 8;
    private static final int FULL_HISTORY_ENTRY_INDEX = -1;

    private final Profile mProfile;
    private final Context mContext;
    private final NavigationController mNavigationController;
    private NavigationHistory mHistory;
    private final NavigationAdapter mAdapter;
    private final ListItemFactory mListItemFactory;

    private final int mFaviconSize;

    private Bitmap mDefaultFavicon;

    /**
     * Loads the favicons asynchronously.
     */
    private FaviconHelper mFaviconHelper;

    private boolean mInitialized;

    /**
     * Constructs a new popup with the given history information.
     *
     * @param profile The profile used for fetching favicons.
     * @param context The context used for building the popup.
     * @param navigationController The controller which takes care of page navigations.
     * @param isForward Whether to request forward navigation entries.
     */
    public NavigationPopup(Profile profile, Context context,
            NavigationController navigationController, boolean isForward) {
        super(context, null, android.R.attr.popupMenuStyle);
        mProfile = profile;
        mContext = context;
        mNavigationController = navigationController;
        mHistory = mNavigationController.getDirectedNavigationHistory(
                isForward, MAXIMUM_HISTORY_ITEMS);

        mHistory.addEntry(new NavigationEntry(FULL_HISTORY_ENTRY_INDEX, UrlConstants.HISTORY_URL,
                null, null, mContext.getResources().getString(R.string.show_full_history), null,
                null, 0));

        setBackgroundDrawable(
                ApiCompatibilityUtils.getDrawable(mContext.getResources(), R.drawable.popup_bg));

        if (ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.LONG_PRESS_BACK_NEW_DESIGN)) {
            mAdapter = new NewNavigationAdapter();
        } else {
            mAdapter = new NavigationAdapter();
        }

        setModal(true);
        setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        setOnItemClickListener(this);
        setAdapter(mAdapter);

        mFaviconSize = mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mListItemFactory = new ListItemFactory(context);
    }

    @Override
    public void show() {
        if (!mInitialized) initialize();
        super.show();
        if (mAdapter.mInReverseOrder) scrollToBottom();
    }

    @Override
    public void dismiss() {
        if (mInitialized) mFaviconHelper.destroy();
        mInitialized = false;
        super.dismiss();
    }

    private void scrollToBottom() {
        getListView().addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewDetachedFromWindow(View v) {
                if (v != null) v.removeOnAttachStateChangeListener(this);
            }

            @Override
            public void onViewAttachedToWindow(View v) {
                ((ListView) v).smoothScrollToPosition(mHistory.getEntryCount() - 1);
            }
        });
    }

    /**
     * Reverses the history order for visual purposes only. This does not pull from the beginning of
     * the navigation history, but only reverses the list that would have normally been retrieved.
     */
    public void reverseHistoryOrder() {
        mAdapter.reverseOrder();
    }

    private void initialize() {
        ThreadUtils.assertOnUiThread();
        mInitialized = true;
        mFaviconHelper = new FaviconHelper();

        Set<String> requestedUrls = new HashSet<String>();
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (entry.getFavicon() != null) continue;
            final String pageUrl = entry.getUrl();
            if (!requestedUrls.contains(pageUrl)) {
                FaviconImageCallback imageCallback =
                        (bitmap, iconUrl) -> NavigationPopup.this.onFaviconAvailable(pageUrl,
                                bitmap);
                mFaviconHelper.getLocalFaviconImageForURL(
                        mProfile, pageUrl, mFaviconSize, imageCallback);
                requestedUrls.add(pageUrl);
            }
        }
    }

    /**
     * Called when favicon data requested by {@link #initialize()} is retrieved.
     * @param pageUrl the page for which the favicon was retrieved.
     * @param favicon the favicon data.
     */
    private void onFaviconAvailable(String pageUrl, Object favicon) {
        if (favicon == null) {
            if (mDefaultFavicon == null) {
                mDefaultFavicon = BitmapFactory.decodeResource(
                        mContext.getResources(), R.drawable.default_favicon);
            }
            favicon = mDefaultFavicon;
        }
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (TextUtils.equals(pageUrl, entry.getUrl())) entry.updateFavicon((Bitmap) favicon);
        }
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        NavigationEntry entry = (NavigationEntry) parent.getItemAtPosition(position);
        if (entry.getIndex() == FULL_HISTORY_ENTRY_INDEX) {
            assert mContext instanceof ChromeActivity;
            ChromeActivity activity = (ChromeActivity) mContext;
            HistoryManagerUtils.showHistoryManager(activity, activity.getActivityTab());
        } else {
            mNavigationController.goToNavigationIndex(entry.getIndex());
        }

        dismiss();
    }

    private void updateBitmapForTextView(TextView view, Bitmap bitmap) {
        Drawable faviconDrawable = null;
        if (bitmap != null) {
            faviconDrawable = new BitmapDrawable(mContext.getResources(), bitmap);
            ((BitmapDrawable) faviconDrawable).setGravity(Gravity.FILL);
        } else {
            faviconDrawable = new ColorDrawable(Color.TRANSPARENT);
        }
        faviconDrawable.setBounds(0, 0, mFaviconSize, mFaviconSize);
        view.setCompoundDrawables(faviconDrawable, null, null, null);
    }

    private static class ListItemFactory {
        private static final int LIST_ITEM_HEIGHT_DP = 48;
        private static final int PADDING_DP = 8;
        private static final int TEXT_SIZE_SP = 18;
        private static final float FADE_LENGTH_DP = 25.0f;
        private static final float FADE_STOP = 0.75f;

        int mFadeEdgeLength;
        int mFadePadding;
        int mListItemHeight;
        int mPadding;
        boolean mIsLayoutDirectionRTL;
        Context mContext;

        public ListItemFactory(Context context) {
            mContext = context;
            computeFadeDimensions();
        }

        private void computeFadeDimensions() {
            // Fade with linear gradient starting 25dp from right margin.
            // Reaches 0% opacity at 75% length. (Simulated with extra padding)
            float density = mContext.getResources().getDisplayMetrics().density;
            float fadeLength = (FADE_LENGTH_DP * density);
            mFadeEdgeLength = (int) (fadeLength * FADE_STOP);
            mFadePadding = (int) (fadeLength * (1 - FADE_STOP));
            mListItemHeight = (int) (density * LIST_ITEM_HEIGHT_DP);
            mPadding = (int) (density * PADDING_DP);
            mIsLayoutDirectionRTL = LocalizationUtils.isLayoutRtl();
        }

        public TextView createListItem() {
            TextView view = new TextView(mContext);
            view.setFadingEdgeLength(mFadeEdgeLength);
            view.setHorizontalFadingEdgeEnabled(true);
            view.setSingleLine();
            view.setTextSize(TEXT_SIZE_SP);
            view.setMinimumHeight(mListItemHeight);
            view.setGravity(Gravity.CENTER_VERTICAL);
            view.setCompoundDrawablePadding(mPadding);
            if (!mIsLayoutDirectionRTL) {
                view.setPadding(mPadding, 0, mPadding + mFadePadding , 0);
            } else {
                view.setPadding(mPadding + mFadePadding, 0, mPadding, 0);
            }
            return view;
        }
    }

    private class NavigationAdapter extends BaseAdapter {
        boolean mInReverseOrder;

        public void reverseOrder() {
            mInReverseOrder = true;
        }

        @Override
        public int getCount() {
            return mHistory.getEntryCount();
        }

        @Override
        public Object getItem(int position) {
            position = mInReverseOrder ? getCount() - position - 1 : position;
            return mHistory.getEntryAtIndex(position);
        }

        @Override
        public long getItemId(int position) {
            return ((NavigationEntry) getItem(position)).getIndex();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            TextView view;
            if (convertView instanceof TextView) {
                view = (TextView) convertView;
            } else {
                view = mListItemFactory.createListItem();
            }
            NavigationEntry entry = (NavigationEntry) getItem(position);
            setViewText(entry, view);
            updateBitmapForTextView(view, entry.getFavicon());

            return view;
        }
    }

    private class NewNavigationAdapter extends NavigationAdapter {
        private Integer mTopPadding;

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            EntryViewHolder viewHolder;
            if (convertView == null) {
                LayoutInflater inflater = LayoutInflater.from(parent.getContext());
                convertView = inflater.inflate(R.layout.navigation_popup_item, parent, false);
                viewHolder = new EntryViewHolder();
                viewHolder.mContainer = convertView;
                viewHolder.mImageView = convertView.findViewById(R.id.favicon_img);
                viewHolder.mTextView = convertView.findViewById(R.id.entry_title);
                convertView.setTag(viewHolder);
            } else {
                viewHolder = (EntryViewHolder) convertView.getTag();
            }

            NavigationEntry entry = (NavigationEntry) getItem(position);
            setViewText(entry, viewHolder.mTextView);
            viewHolder.mImageView.setImageBitmap(entry.getFavicon());

            if (mInReverseOrder) {
                View container = viewHolder.mContainer;
                if (mTopPadding == null) {
                    mTopPadding = container.getResources().getDimensionPixelSize(
                            R.dimen.navigation_popup_top_padding);
                }
                viewHolder.mContainer.setPadding(container.getPaddingLeft(),
                        position == 0 ? mTopPadding : 0, container.getPaddingRight(),
                        container.getPaddingBottom());
            }

            return convertView;
        }
    }

    private static class EntryViewHolder {
        View mContainer;
        ImageView mImageView;
        TextView mTextView;
    }

    private static void setViewText(NavigationEntry entry, TextView view) {
        String entryText = entry.getTitle();
        if (TextUtils.isEmpty(entryText)) entryText = entry.getVirtualUrl();
        if (TextUtils.isEmpty(entryText)) entryText = entry.getUrl();

        view.setText(entryText);
    }
}
