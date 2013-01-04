// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.HeaderViewListAdapter;
import android.widget.ListPopupWindow;
import android.widget.ListView;
import android.widget.ListView.FixedViewInfo;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.NavigationClient;
import org.chromium.content.browser.NavigationEntry;
import org.chromium.content.browser.NavigationHistory;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

/**
 * A popup that handles displaying the navigation history for a given tab.
 */
public class NavigationPopup extends ListPopupWindow implements AdapterView.OnItemClickListener {

    private static final int LIST_ITEM_HEIGHT_DP = 48;
    private static final int FAVICON_SIZE_DP = 16;
    private static final int PADDING_DP = 8;
    private static final int TEXT_SIZE_SP = 18;

    private static final int MAXIMUM_HISTORY_ITEMS = 8;

    private final Context mContext;
    private final NavigationPopupDelegate mDelegate;
    private final NavigationClient mNavigationClient;
    private final NavigationHistory mHistory;
    private final NavigationAdapter mAdapter;
    private final TextView mShowHistoryView;

    private final int mListItemHeight;
    private final int mFaviconSize;
    private final int mPadding;

    private int mNativeNavigationPopup;

    /**
     * Delegate functionality required to operate on the history.
     */
    public interface NavigationPopupDelegate {
        /** Open the complete browser history page */
        void openHistory();
    }

    /**
     * Constructs a new popup with the given history information.
     *
     * @param context The context used for building the popup.
     * @param delegate The delegate that handles history actions outside of the given
     *                 {@link ContentViewCore}.
     * @param navigationClient The owner of the history being displayed.
     * @param isForward Whether to request forward navigation entries.
     */
    public NavigationPopup(
            Context context, NavigationPopupDelegate delegate,
            NavigationClient navigationClient, boolean isForward) {
        super(context, null, android.R.attr.popupMenuStyle);
        mContext = context;
        assert delegate != null : "NavigationPopup requires non-null delegate.";
        mDelegate = delegate;
        mNavigationClient = navigationClient;
        mHistory = mNavigationClient.getDirectedNavigationHistory(
                isForward, MAXIMUM_HISTORY_ITEMS);
        mAdapter = new NavigationAdapter();

        float density = mContext.getResources().getDisplayMetrics().density;
        mListItemHeight = (int) (density * LIST_ITEM_HEIGHT_DP);
        mFaviconSize = (int) (density * FAVICON_SIZE_DP);
        mPadding = (int) (density * PADDING_DP);

        setModal(true);
        setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        setOnItemClickListener(this);

        FixedViewInfo footerInfo = new ListView(context).new FixedViewInfo();
        mShowHistoryView = createListItem();
        mShowHistoryView.setText(R.string.show_history_label);
        footerInfo.isSelectable = true;
        footerInfo.view = mShowHistoryView;
        ArrayList<FixedViewInfo> footerInfoList = new ArrayList<FixedViewInfo>(1);
        footerInfoList.add(footerInfo);
        setAdapter(new HeaderViewListAdapter(null, footerInfoList, mAdapter));
    }

    /**
     * @return The URL of the page for managing history.
     */
    public static String getHistoryUrl() {
        return nativeGetHistoryUrl();
    }

    /**
     * @return Whether a navigation popup is valid for the given page.
     */
    public boolean shouldBeShown() {
        return mHistory.getEntryCount() > 0;
    }

    @Override
    public void show() {
        if (mNativeNavigationPopup == 0) initializeNative();
        super.show();
    }

    @Override
    public void dismiss() {
        if (mNativeNavigationPopup != 0) {
            nativeDestroy(mNativeNavigationPopup);
            mNativeNavigationPopup = 0;
        }
        super.dismiss();
    }

    private void initializeNative() {
        ThreadUtils.assertOnUiThread();
        mNativeNavigationPopup = nativeInit();

        Set<String> requestedUrls = new HashSet<String>();
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (entry.getFavicon() != null) continue;
            String url = entry.getUrl();
            if (!requestedUrls.contains(url)) {
                nativeFetchFaviconForUrl(mNativeNavigationPopup, url);
                requestedUrls.add(url);
            }
        }
        nativeFetchFaviconForUrl(mNativeNavigationPopup, nativeGetHistoryUrl());
    }

    @CalledByNative
    private void onFaviconUpdated(String url, Object favicon) {
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (TextUtils.equals(url, entry.getUrl())) entry.updateFavicon((Bitmap) favicon);
        }
        if (TextUtils.equals(url, nativeGetHistoryUrl())) {
            updateBitmapForTextView(mShowHistoryView, (Bitmap) favicon);
        }
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        if (position == mHistory.getEntryCount()) {
            mDelegate.openHistory();
        } else {
            NavigationEntry entry = (NavigationEntry) parent.getItemAtPosition(position);
            mNavigationClient.goToNavigationIndex(entry.getIndex());
        }
        dismiss();
    }

    private TextView createListItem() {
        TextView view = new TextView(mContext);
        view.setSingleLine();
        view.setTextSize(TEXT_SIZE_SP);
        view.setMinimumHeight(mListItemHeight);
        view.setGravity(Gravity.CENTER_VERTICAL);
        view.setCompoundDrawablePadding(mPadding);
        view.setPadding(mPadding, 0, mPadding, 0);
        return view;
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

    private class NavigationAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return mHistory.getEntryCount();
        }

        @Override
        public Object getItem(int position) {
            return mHistory.getEntryAtIndex(position);
        }

        @Override
        public long getItemId(int position) {
            return ((NavigationEntry) getItem(position)).getIndex();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            TextView view;
            if (convertView != null && convertView instanceof TextView) {
                view = (TextView) convertView;
            } else {
                view = createListItem();
            }
            NavigationEntry entry = (NavigationEntry) getItem(position);

            String entryText = entry.getTitle();
            if (TextUtils.isEmpty(entryText)) entryText = entry.getVirtualUrl();
            if (TextUtils.isEmpty(entryText)) entryText = entry.getUrl();
            view.setText(entryText);
            updateBitmapForTextView(view, entry.getFavicon());

            return view;
        }
    }

    private static native String nativeGetHistoryUrl();

    private native int nativeInit();
    private native void nativeDestroy(int nativeNavigationPopup);
    private native void nativeFetchFaviconForUrl(int nativeNavigationPopup, String url);
}
