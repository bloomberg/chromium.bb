// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Intent;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Handler;
import android.support.customtabs.CustomTabsIntent;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.omnibox.AutocompleteController;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Prototype that queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable, SearchActivityLocationBarLayout.Delegate,
                   View.OnLayoutChangeListener {
    private static final String TAG = "searchwidget";

    /** Padding gleaned from the background Drawable of the search box. */
    private final Rect mSearchBoxPadding = new Rect();

    /** Medium margin/padding used for the search box. */
    private int mSpacingMedium;

    /** Large margin/padding used for the search box. */
    private int mSpacingLarge;

    /** Main content view. */
    private ViewGroup mContentView;

    /** Whether the native library has been loaded. */
    private boolean mIsNativeReady;

    /** Input submitted before before the native library was loaded. */
    private String mQueuedUrl;

    /** The View that represents the search box. */
    private SearchActivityLocationBarLayout mSearchBox;

    private UrlBar mUrlBar;

    private SearchBoxDataProvider mSearchBoxDataProvider;

    private SnackbarManager mSnackbarManager;
    private ActivityWindowAndroid mWindowAndroid;
    private Tab mTab;

    @Override
    public void backKeyPressed() {
        finish();
    }

    @Override
    public void onStop() {
        finish();
        super.onStop();
    }

    @Override
    public void finish() {
        super.finish();
        overridePendingTransition(0, android.R.anim.fade_out);
    }

    @Override
    protected boolean shouldDelayBrowserStartup() {
        return true;
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (super.onActivityResultWithNative(requestCode, resultCode, intent)) return true;
        boolean result = mWindowAndroid.onActivityResult(requestCode, resultCode, intent);

        // The voice query should have completed.  If the voice recognition isn't confident about
        // what it heard, it puts the query into the omnibox.  We need to focus it at that point.
        if (mUrlBar != null) focusTextBox(false);

        return result;
    }

    @Override
    protected void setContentView() {
        initializeDimensions();

        mWindowAndroid = new ActivityWindowAndroid(this);
        mSnackbarManager = new SnackbarManager(this);
        mSearchBoxDataProvider = new SearchBoxDataProvider();

        mContentView = createContentView(mSearchBox);
        mContentView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Finish the Activity if the user clicks on the scrim.
                finish();
            }
        });

        // Build the search box.
        mSearchBox = (SearchActivityLocationBarLayout) mContentView.findViewById(
                R.id.search_location_bar);
        mSearchBox.setDelegate(this);
        mSearchBox.setPadding(mSpacingLarge, mSpacingMedium, mSpacingLarge, mSpacingMedium);
        mSearchBox.initializeControls(new WindowDelegate(getWindow()), mWindowAndroid);
        mSearchBox.setUrlBarFocusable(true);
        mSearchBox.setToolbarDataProvider(mSearchBoxDataProvider);

        setContentView(mContentView);
        mUrlBar = (UrlBar) mSearchBox.findViewById(R.id.url_bar);
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mIsNativeReady = true;

        mTab = new Tab(TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID),
                Tab.INVALID_TAB_ID, false, this, mWindowAndroid, TabLaunchType.FROM_EXTERNAL_APP,
                null, null);
        mTab.initialize(WebContentsFactory.createWebContents(false, false), null,
                new TabDelegateFactory(), false, false);
        mTab.loadUrl(new LoadUrlParams("about:blank"));
        mSearchBoxDataProvider.onNativeLibraryReady(mTab);
        mSearchBox.onNativeLibraryReady();
        mSearchBox.setAutocompleteProfile(Profile.getLastUsedProfile().getOriginalProfile());
        mSearchBox.setShowCachedZeroSuggestResults(true);

        if (mQueuedUrl != null) loadUrl(mQueuedUrl);

        new Handler().post(new Runnable() {
            @Override
            public void run() {
                onDeferredStartup();
            }
        });
    }

    @Override
    public void onDeferredStartup() {
        super.onDeferredStartup();

        AutocompleteController.nativePrefetchZeroSuggestResults();
        CustomTabsConnection.getInstance(getApplication()).warmup(0);

        if (!isVoiceSearchIntent() && mUrlBar.isFocused()) mSearchBox.onUrlFocusChange(true);
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
        if (isVoiceSearchIntent()) {
            mSearchBox.startVoiceRecognition();
        } else {
            focusTextBox(true);
        }
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

    private void focusTextBox(boolean clearQuery) {
        if (mIsNativeReady) mSearchBox.onUrlFocusChange(true);

        if (clearQuery) {
            mUrlBar.setIgnoreTextChangesForAutocomplete(true);
            mUrlBar.setUrl("", null);
            mUrlBar.setIgnoreTextChangesForAutocomplete(false);
        }
        mUrlBar.setCursorVisible(true);
        mUrlBar.setSelection(0, mUrlBar.getText().length());
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                UiUtils.showKeyboard(mUrlBar);
            }
        });
    }

    @Override
    public void loadUrl(String url) {
        // Wait until native has loaded.
        if (!mIsNativeReady) {
            mQueuedUrl = url;
            return;
        }

        // Don't do anything if the input was empty. This is done after the native check to prevent
        // resending a queued query after the user deleted it.
        if (TextUtils.isEmpty(url)) return;

        // Fix up the URL and send it to a tab.
        String fixedUrl = UrlFormatter.fixupUrl(url);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(fixedUrl));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        boolean useHerbTab = ContextUtils.getAppSharedPreferences().getBoolean(
                SearchWidgetProvider.PREF_USE_HERB_TAB, false);
        if (useHerbTab) {
            intent = ChromeLauncherActivity.createCustomTabActivityIntent(this, intent, true);
            intent.putExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE,
                    CustomTabsIntent.SHOW_PAGE_TITLE);
        } else {
            intent.setPackage(getPackageName());
            IntentHandler.addTrustedIntentExtras(intent);
        }

        // TODO(dfalcantara): Make IntentUtils take in an ActivityOptions bundle once public.
        startActivity(intent,
                ActivityOptionsCompat
                        .makeCustomAnimation(this, android.R.anim.fade_in, android.R.anim.fade_out)
                        .toBundle());
        finish();
    }

    private ViewGroup createContentView(final View searchBox) {
        assert mContentView == null;

        ViewGroup contentView = (ViewGroup) LayoutInflater.from(this).inflate(
                R.layout.search_activity, null, false);
        contentView.addOnLayoutChangeListener(this);
        return contentView;
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        mContentView.removeOnLayoutChangeListener(this);
        beginLoadingLibrary();
    }

    private void initializeDimensions() {
        // Cache the padding of the Drawable that is used as the background for the search box.
        Drawable searchBackground =
                ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.card_single);
        searchBackground.getPadding(mSearchBoxPadding);

        // TODO(dfalcantara): Add values to the XML files instead of reusing random ones.
        mSpacingMedium =
                getResources().getDimensionPixelSize(R.dimen.location_bar_incognito_badge_padding);
        mSpacingLarge =
                getResources().getDimensionPixelSize(R.dimen.contextual_search_peek_promo_padding);
    }

    private void beginLoadingLibrary() {
        beginQuery();
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mSearchBox.showCachedZeroSuggestResultsIfAvailable();
            }
        });
        ChromeBrowserInitializer.getInstance(getApplicationContext())
                .handlePreNativeStartup(SearchActivity.this);
    }
}
