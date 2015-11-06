// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.Collection;
import java.util.HashSet;

/**
 * This activity displays a list of nearby URLs as stored in the {@link UrlManager}.
 * This activity does not and should not rely directly or indirectly on the native library.
 */
public class ListUrlsActivity extends AppCompatActivity implements AdapterView.OnItemClickListener {
    private static final String TAG = "PhysicalWeb";
    private static final long SCAN_TIMEOUT_MILLIS = 5000; // 5 seconds
    private NearbyUrlsAdapter mAdapter;
    private PwsClient mPwsClient;
    private Handler mTimerHandler;
    private Runnable mTimerCallback;
    private ListView mListView;
    private TextView mEmptyView;
    private boolean mDisplayRecorded;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.physical_web_list_urls_activity);

        mAdapter = new NearbyUrlsAdapter(this);

        mListView = (ListView) findViewById(android.R.id.list);
        mEmptyView = (TextView) findViewById(android.R.id.empty);
        mListView.setEmptyView(mEmptyView);
        mListView.setAdapter(mAdapter);
        mListView.setOnItemClickListener(this);

        mPwsClient = new PwsClient();
        int referer = getIntent().getIntExtra(UrlManager.REFERER_KEY, 0);
        if (savedInstanceState == null  // Ensure this is a newly-created activity
                && referer == UrlManager.NOTIFICATION_REFERER) {
            PhysicalWebUma.onNotificationPressed();
        }
        mDisplayRecorded = false;

        mTimerHandler = new Handler();
        mTimerCallback = new Runnable() {
            @Override
            public void run() {
                updateEmptyListMessage(false);
            }
        };
    }

    @Override
    protected void onResume() {
        super.onResume();
        mAdapter.clear();
        Collection<String> urls = UrlManager.getInstance(this).getUrls();
        final long timestamp = SystemClock.elapsedRealtime();
        mPwsClient.resolve(urls, new PwsClient.ResolveScanCallback() {
            @Override
            public void onPwsResults(Collection<PwsResult> pwsResults) {
                PhysicalWebUma.onPwsResponse(SystemClock.elapsedRealtime() - timestamp);
                // filter out duplicate site URLs
                Collection<String> siteUrls = new HashSet<>();
                for (PwsResult pwsResult : pwsResults) {
                    String siteUrl = pwsResult.siteUrl;
                    String iconUrl = pwsResult.iconUrl;

                    if (siteUrl != null && !siteUrls.contains(siteUrl)) {
                        siteUrls.add(siteUrl);
                        mAdapter.add(pwsResult);

                        if (iconUrl != null) {
                            fetchIcon(iconUrl);
                        }
                    }
                }
                // TODO(cco3): Right now we use a simple boolean to see if we've previously recorded
                //             how many URLs we display, but in the future we need to switch to
                //             something more sophisticated that recognizes when a "refresh" has
                //             taken place and the displayed URLs are significantly different.
                if (!mDisplayRecorded) {
                    mDisplayRecorded = true;
                    PhysicalWebUma.onUrlsDisplayed(mAdapter.getCount());
                }
            }
        });

        // Nearby doesn't tell us when it's finished but it usually only
        // takes a few seconds.
        updateEmptyListMessage(true);
        mTimerHandler.removeCallbacks(mTimerCallback);
        mTimerHandler.postDelayed(mTimerCallback, SCAN_TIMEOUT_MILLIS);
    }

    /**
     * Handle a click event.
     * @param adapterView The AdapterView where the click happened.
     * @param view The View that was clicked inside the AdapterView.
     * @param position The position of the clicked element in the list.
     * @param id The row id of the clicked element in the list.
     */
    @Override
    public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
        PhysicalWebUma.onUrlSelected();
        PwsResult pwsResult = mAdapter.getItem(position);
        Intent intent = createNavigateToUrlIntent(pwsResult);
        startActivity(intent);
    }

    private void fetchIcon(String iconUrl) {
        mPwsClient.fetchIcon(iconUrl, new PwsClient.FetchIconCallback() {
            @Override
            public void onIconReceived(String url, Bitmap bitmap) {
                mAdapter.setIcon(url, bitmap);
            }
        });
    }

    private void updateEmptyListMessage(boolean isScanning) {
        int messageId = R.string.physical_web_empty_list;
        if (isScanning) {
            messageId = R.string.physical_web_empty_list_scanning;
        }

        mEmptyView.setText(messageId);
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
