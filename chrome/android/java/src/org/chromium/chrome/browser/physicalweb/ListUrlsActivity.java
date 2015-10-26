// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.app.ListActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import org.chromium.chrome.R;

import java.util.Collection;

/**
 * This activity displays a list of nearby URLs as stored in the {@link UrlManager}.
 * This activity does not and should not rely directly or indirectly on the native library.
 */
public class ListUrlsActivity extends ListActivity {
    private static final String TAG = "PhysicalWeb";
    private ArrayAdapter<PwsResult> mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.physical_web_list_urls_activity);

        mAdapter = new NearbyUrlsAdapter(this);
        setListAdapter(mAdapter);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mAdapter.clear();
        Collection<String> urls = UrlManager.getInstance(this).getUrls();
        for (String url : urls) {
            PwsResult pwsResult = new PwsResult(url, url, "", "");
            mAdapter.add(pwsResult);
        }
    }

    /**
     * Handle a click event.
     * @param l The ListView.
     * @param v The View that was clicked inside the ListView.
     * @param position The position of the clicked element in the list.
     * @param id The row id of the clicked element in the list.
     */
    @Override
    public void onListItemClick(ListView l, View v, int position, long id) {
        PwsResult pwsResult = mAdapter.getItem(position);
        Intent intent = createNavigateToUrlIntent(pwsResult);
        startActivity(intent);
    }

    private static Intent createNavigateToUrlIntent(PwsResult pwsResult) {
        String url = pwsResult.siteUrl;
        if (url == null) {
            url = pwsResult.requestUrl;
        }

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setData(Uri.parse(url));
        return intent;
    }
}
