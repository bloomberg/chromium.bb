// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.PorterDuff;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;

import java.util.Collection;

/**
 * This activity displays a list of nearby URLs as stored in the {@link UrlManager}.
 * This activity does not and should not rely directly or indirectly on the native library.
 */
public class ListUrlsActivity extends AppCompatActivity implements AdapterView.OnItemClickListener,
        SwipeRefreshWidget.OnRefreshListener, UrlManager.Listener {
    public static final String REFERER_KEY = "referer";
    public static final int NOTIFICATION_REFERER = 1;
    public static final int OPTIN_REFERER = 2;
    public static final int PREFERENCE_REFERER = 3;
    private static final String TAG = "PhysicalWeb";

    private Context mContext;
    private NearbyUrlsAdapter mAdapter;
    private PwsClient mPwsClient;
    private ListView mListView;
    private TextView mEmptyListText;
    private ImageView mScanningImageView;
    private SwipeRefreshWidget mSwipeRefreshWidget;
    private boolean mIsInitialDisplayRecorded;
    private boolean mIsRefreshing;
    private boolean mIsRefreshUserInitiated;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mContext = this;
        setContentView(R.layout.physical_web_list_urls_activity);

        mAdapter = new NearbyUrlsAdapter(this);

        View emptyView = findViewById(R.id.physical_web_empty);
        mListView = (ListView) findViewById(R.id.physical_web_urls_list);
        mListView.setEmptyView(emptyView);
        mListView.setAdapter(mAdapter);
        mListView.setOnItemClickListener(this);

        mEmptyListText = (TextView) findViewById(R.id.physical_web_empty_list_text);

        mScanningImageView = (ImageView) findViewById(R.id.physical_web_logo);

        mSwipeRefreshWidget =
                (SwipeRefreshWidget) findViewById(R.id.physical_web_swipe_refresh_widget);
        mSwipeRefreshWidget.setOnRefreshListener(this);

        mPwsClient = new PwsClientImpl();
        int referer = getIntent().getIntExtra(REFERER_KEY, 0);
        if (savedInstanceState == null  // Ensure this is a newly-created activity.
                && referer == NOTIFICATION_REFERER) {
            PhysicalWebUma.onNotificationPressed(this);
        }
        mIsInitialDisplayRecorded = false;
        mIsRefreshing = false;
        mIsRefreshUserInitiated = false;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        Drawable tintedRefresh = ContextCompat.getDrawable(this, R.drawable.btn_toolbar_reload);
        int tintColor = ContextCompat.getColor(this, R.color.light_normal_color);
        tintedRefresh.setColorFilter(tintColor, PorterDuff.Mode.SRC_IN);

        MenuItem refreshItem = menu.add(R.string.physical_web_refresh);
        refreshItem.setIcon(tintedRefresh);
        refreshItem.setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
        refreshItem.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                startRefresh(true, false);
                return true;
            }
        });

        MenuItem closeItem = menu.add(R.string.close);
        closeItem.setIcon(R.drawable.btn_close);
        closeItem.setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
        closeItem.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                finish();
                return true;
            }
        });

        return true;
    }

    @Override
    protected void onResume() {
        super.onResume();
        startRefresh(false, false);
    }

    @Override
    public void onRefresh() {
        startRefresh(true, true);
    }

    @Override
    protected void onStop() {
        UrlManager.getInstance(this).removeObserver(this);
        super.onStop();
    }

    private void resolve(Collection<String> urls) {
        final long timestamp = SystemClock.elapsedRealtime();
        mPwsClient.resolve(urls, new PwsClient.ResolveScanCallback() {
            @Override
            public void onPwsResults(Collection<PwsResult> pwsResults) {
                long duration = SystemClock.elapsedRealtime() - timestamp;
                PhysicalWebUma.onForegroundPwsResolution(ListUrlsActivity.this, duration);

                // filter out duplicate site URLs.
                for (PwsResult pwsResult : pwsResults) {
                    String siteUrl = pwsResult.siteUrl;
                    String iconUrl = pwsResult.iconUrl;

                    if (siteUrl != null && !mAdapter.hasSiteUrl(siteUrl)) {
                        mAdapter.add(pwsResult);

                        if (iconUrl != null && !mAdapter.hasIcon(iconUrl)) {
                            fetchIcon(iconUrl);
                        }
                    }
                }
                finishRefresh();
            }
        });
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
        PhysicalWebUma.onUrlSelected(this);
        PwsResult pwsResult = mAdapter.getItem(position);
        Intent intent = createNavigateToUrlIntent(pwsResult);
        mContext.startActivity(intent);
    }

    /**
     * Called when new nearby URLs are found.
     * @param urls The set of newly-found nearby URLs.
     */
    @Override
    public void onDisplayableUrlsAdded(Collection<String> urls) {
        resolve(urls);
    }

    private void startRefresh(boolean isUserInitiated, boolean isSwipeInitiated) {
        if (mIsRefreshing) {
            return;
        }

        mIsRefreshing = true;
        mIsRefreshUserInitiated = isUserInitiated;

        // Clear the list adapter to trigger the empty list display.
        mAdapter.clear();

        UrlManager urlManager = UrlManager.getInstance(this);
        urlManager.addObserver(this);

        Collection<String> urls = urlManager.getUrls(true);
        if (urls.isEmpty()) {
            finishRefresh();
        } else {
            // Show the swipe-to-refresh busy indicator for refreshes initiated by a swipe.
            if (isSwipeInitiated) {
                mSwipeRefreshWidget.setRefreshing(true);
            }

            // Update the empty list view to show a scanning animation.
            mEmptyListText.setText(R.string.physical_web_empty_list_scanning);

            mScanningImageView.setImageResource(R.drawable.physical_web_scanning_animation);
            mScanningImageView.setColorFilter(null);

            AnimationDrawable animationDrawable =
                    (AnimationDrawable) mScanningImageView.getDrawable();
            animationDrawable.start();

            resolve(urls);
        }

        // Clear stored URLs and resubscribe to Nearby.
        PhysicalWeb.startPhysicalWeb((ChromeApplication) getApplicationContext());
    }

    private void finishRefresh() {
        UrlManager.getInstance(this).removeObserver(this);

        // Hide the busy indicator.
        mSwipeRefreshWidget.setRefreshing(false);

        // Stop the scanning animation, show a "nothing found" message.
        mEmptyListText.setText(R.string.physical_web_empty_list);

        int tintColor = ContextCompat.getColor(this, R.color.physical_web_logo_gray_tint);
        mScanningImageView.setImageResource(R.drawable.physical_web_logo);
        mScanningImageView.setColorFilter(tintColor, PorterDuff.Mode.SRC_IN);

        // Record refresh-related UMA.
        if (!mIsInitialDisplayRecorded) {
            mIsInitialDisplayRecorded = true;
            PhysicalWebUma.onUrlsDisplayed(this, mAdapter.getCount());
        } else if (mIsRefreshUserInitiated) {
            PhysicalWebUma.onUrlsRefreshed(this, mAdapter.getCount());
        }

        mIsRefreshing = false;
    }

    private void fetchIcon(String iconUrl) {
        mPwsClient.fetchIcon(iconUrl, new PwsClient.FetchIconCallback() {
            @Override
            public void onIconReceived(String url, Bitmap bitmap) {
                mAdapter.setIcon(url, bitmap);
            }
        });
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

    @VisibleForTesting
    void overridePwsClientForTesting(PwsClient pwsClient) {
        mPwsClient = pwsClient;
    }

    @VisibleForTesting
    void overrideContextForTesting(Context context) {
        mContext = context;
    }
}
