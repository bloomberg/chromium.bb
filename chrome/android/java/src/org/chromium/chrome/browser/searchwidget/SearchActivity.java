// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Intent;
import android.net.Uri;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.AutocompleteController;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable, SearchActivityLocationBarLayout.Delegate {
    /** Notified about events happening inside a SearchActivity. */
    public interface SearchActivityObserver {
        /** Called when {@link SearchActivity#setContentView} is done. */
        void onSetContentView();

        /** Called when {@link SearchActivity#finishNativeInitialization} is done. */
        void onFinishNativeInitialization();

        /** Called when {@link SearchActivity#finishDeferredInitialization} is done. */
        void onFinishDeferredInitialization();
    }

    private static final String TAG = "searchwidget";

    /** Setting this field causes the Activity to finish itself immediately for tests. */
    private static boolean sIsDisabledForTest;

    /** Notified about events happening for the SearchActivity. */
    private static SearchActivityObserver sObserver;

    /** Main content view. */
    private ViewGroup mContentView;

    /** Whether the user is now allowed to perform searches. */
    private boolean mIsActivityUsable;

    /** Input submitted before before the native library was loaded. */
    private String mQueuedUrl;

    /** The View that represents the search box. */
    private SearchActivityLocationBarLayout mSearchBox;

    private SnackbarManager mSnackbarManager;
    private SearchBoxDataProvider mSearchBoxDataProvider;
    private Tab mTab;

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        if (sIsDisabledForTest) return false;
        return super.isStartedUpCorrectly(intent);
    }

    @Override
    public void backKeyPressed() {
        cancelSearch();
    }

    @Override
    protected boolean shouldDelayBrowserStartup() {
        return true;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(this);
    }

    @Override
    protected void setContentView() {
        mSnackbarManager = new SnackbarManager(this, null);
        mSearchBoxDataProvider = new SearchBoxDataProvider();

        mContentView = createContentView();
        setContentView(mContentView);

        // Build the search box.
        mSearchBox = (SearchActivityLocationBarLayout) mContentView.findViewById(
                R.id.search_location_bar);
        mSearchBox.setDelegate(this);
        mSearchBox.setToolbarDataProvider(mSearchBoxDataProvider);
        mSearchBox.initializeControls(new WindowDelegate(getWindow()), getWindowAndroid());

        // Kick off everything needed for the user to type into the box.
        // TODO(dfalcantara): We should prevent the user from doing anything while we're running the
        //                    logic to determine if they need to see a search engine promo.  Given
        //                    that the logic requires native to be loaded, we'll have to make some
        //                    easy Java-only first-pass checks.
        beginQuery();
        mSearchBox.showCachedZeroSuggestResultsIfAvailable();

        // Kick off loading of the native library.
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                beginLoadingLibrary();
            }
        });

        if (sObserver != null) sObserver.onSetContentView();
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        mTab = new Tab(TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID),
                Tab.INVALID_TAB_ID, false, this, getWindowAndroid(),
                TabLaunchType.FROM_EXTERNAL_APP, null, null);
        mTab.initialize(WebContentsFactory.createWebContents(false, false), null,
                new TabDelegateFactory(), false, false);
        mTab.loadUrl(new LoadUrlParams("about:blank"));

        mSearchBoxDataProvider.onNativeLibraryReady(mTab);
        mSearchBox.onNativeLibraryReady();

        // Force the user to choose a search engine if they have to.
        final Callback<Boolean> deferredCallback = new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                finishDeferredInitialization(result);
            }
        };
        if (!LocaleManager.getInstance().showSearchEnginePromoIfNeeded(this, deferredCallback)) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    deferredCallback.onResult(true);
                }
            });
        }

        if (sObserver != null) sObserver.onFinishNativeInitialization();
    }

    private void finishDeferredInitialization(Boolean result) {
        if (result == null || !result.booleanValue()) {
            Log.e(TAG, "User failed to select a default search engine.");
            finish();
            return;
        }

        mIsActivityUsable = true;
        if (mQueuedUrl != null) loadUrl(mQueuedUrl);

        AutocompleteController.nativePrefetchZeroSuggestResults();
        CustomTabsConnection.getInstance(getApplication()).warmup(0);
        mSearchBox.onDeferredStartup(isVoiceSearchIntent());

        if (sObserver != null) sObserver.onFinishDeferredInitialization();
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        return mSearchBox;
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        beginQuery();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    private boolean isVoiceSearchIntent() {
        return IntentUtils.safeGetBooleanExtra(
                getIntent(), SearchWidgetProvider.EXTRA_START_VOICE_SEARCH, false);
    }

    private void beginQuery() {
        mSearchBox.beginQuery(isVoiceSearchIntent());
    }

    @Override
    protected void onDestroy() {
        if (mTab != null && mTab.isInitialized()) mTab.destroy();
        super.onDestroy();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void loadUrl(String url) {
        // Wait until native has loaded.
        if (!mIsActivityUsable) {
            mQueuedUrl = url;
            return;
        }

        // Don't do anything if the input was empty. This is done after the native check to prevent
        // resending a queued query after the user deleted it.
        if (TextUtils.isEmpty(url)) return;

        // Fix up the URL and send it to the full browser.
        String fixedUrl = UrlFormatter.fixupUrl(url);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(fixedUrl));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setPackage(getPackageName());
        IntentHandler.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(this, intent,
                ActivityOptionsCompat
                        .makeCustomAnimation(this, android.R.anim.fade_in, android.R.anim.fade_out)
                        .toBundle());
        finish();
    }

    private ViewGroup createContentView() {
        assert mContentView == null;

        ViewGroup contentView = (ViewGroup) LayoutInflater.from(this).inflate(
                R.layout.search_activity, null, false);
        contentView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                cancelSearch();
            }
        });
        return contentView;
    }

    private void cancelSearch() {
        finish();
        overridePendingTransition(0, R.anim.activity_close_exit);
    }

    /** See {@link #sIsDisabledForTest}. */
    @VisibleForTesting
    static void disableForTests() {
        sIsDisabledForTest = true;
    }

    /** See {@link #sObserver}. */
    @VisibleForTesting
    static void setObserverForTests(SearchActivityObserver observer) {
        sObserver = observer;
    }
}
