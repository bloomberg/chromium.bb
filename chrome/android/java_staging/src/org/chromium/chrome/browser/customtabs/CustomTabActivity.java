// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.text.TextUtils;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.CompositorChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.document.BrandColorUtils;
import org.chromium.chrome.browser.tabmodel.SingleTabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarHelper;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends CompositorChromeActivity {
    private static CustomTabContentHandler sActiveContentHandler;

    private CustomTab mTab;
    private ToolbarHelper mToolbarHelper;
    private CustomTabAppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private AppMenuHandler mAppMenuHandler;
    private FindToolbarManager mFindToolbarManager;
    private CustomTabIntentDataProvider mIntentDataProvider;
    private long mSessionId;
    private CustomTabContentHandler mCustomTabContentHandler;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    private boolean mShouldOverridePackage;

    private boolean mRecordedStartupUma;

    /**
     * Sets the currently active {@link CustomTabContentHandler} in focus.
     * @param contentHandler {@link CustomTabContentHandler} to set.
     */
    public static void setActiveContentHandler(CustomTabContentHandler contentHandler) {
        sActiveContentHandler = contentHandler;
    }

    /**
     * Used to check whether an incoming intent can be handled by the
     * current {@link CustomTabContentHandler}.
     * @return Whether the active {@link CustomTabContentHandler} has handled the intent.
     */
    public static boolean handleInActiveContentIfNeeded(Intent intent) {
        if (sActiveContentHandler == null) return false;

        long intentSessionId = intent.getLongExtra(
                CustomTabIntentDataProvider.EXTRA_CUSTOM_TABS_SESSION_ID,
                CustomTabIntentDataProvider.INVALID_SESSION_ID);
        if (intentSessionId == CustomTabIntentDataProvider.INVALID_SESSION_ID) return false;

        if (sActiveContentHandler.getSessionId() != intentSessionId) return false;
        String url = IntentHandler.getUrlFromIntent(intent);
        if (TextUtils.isEmpty(url)) return false;
        sActiveContentHandler.loadUrl(new LoadUrlParams(url));
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();
        CustomTabsConnection.getInstance(getApplication())
                .keepAliveForSessionId(mIntentDataProvider.getSessionId(),
                        mIntentDataProvider.getKeepAliveServiceIntent());
    }

    @Override
    public void onStop() {
        super.onStop();
        CustomTabsConnection.getInstance(getApplication())
                .dontKeepAliveForSessionId(mIntentDataProvider.getSessionId());
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();
        setTabModelSelector(new SingleTabModelSelector(this, false, true) {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                    boolean incognito) {
                // A custom tab either loads a url or starts an external activity to handle the url.
                // It never opens a new tab/chrome activity.
                mTab.loadUrl(loadUrlParams);
                return mTab;
            }
        });
        mIntentDataProvider = new CustomTabIntentDataProvider(getIntent(), this);
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();
        ToolbarControlContainer controlContainer =
                ((ToolbarControlContainer) findViewById(R.id.control_container));
        mAppMenuPropertiesDelegate = new CustomTabAppMenuPropertiesDelegate(this,
                mIntentDataProvider.getMenuTitles());
        mAppMenuHandler =
                new AppMenuHandler(this, mAppMenuPropertiesDelegate, R.menu.custom_tabs_menu);
        mToolbarHelper = new ToolbarHelper(this, controlContainer,
                mAppMenuHandler, mAppMenuPropertiesDelegate,
                getCompositorViewHolder().getInvalidator());
        mToolbarHelper.setThemeColor(mIntentDataProvider.getToolbarColor());
        setStatusBarColor(mIntentDataProvider.getToolbarColor());
        if (mIntentDataProvider.shouldShowActionButton()) {
            mToolbarHelper.addCustomActionButton(mIntentDataProvider.getActionButtonIcon(),
                    new OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            mIntentDataProvider.sendButtonPendingIntentWithUrl(
                                    getApplicationContext(), mTab.getUrl());
                            RecordUserAction.record("CustomTabsCustomActionButtonClick");
                        }
                    });
        }
        mAppMenuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (!isVisible) {
                    mAppMenuPropertiesDelegate.onMenuDismissed();
                }
            }
        });
    }

    @Override
    public void finishNativeInitialization() {
        String url = IntentHandler.getUrlFromIntent(getIntent());
        mSessionId = mIntentDataProvider.getSessionId();
        mTab = new CustomTab(this, getWindowAndroid(), mSessionId, url, Tab.INVALID_TAB_ID);
        getTabModelSelector().setTab(mTab);

        ToolbarControlContainer controlContainer = (ToolbarControlContainer) findViewById(
                R.id.control_container);
        LayoutManagerDocument layoutDriver = new LayoutManagerDocument(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content), controlContainer);
        mFindToolbarManager = new FindToolbarManager(this, getTabModelSelector(),
                mToolbarHelper.getContextualMenuBar().getCustomSelectionActionModeCallback());
        controlContainer.setFindToolbarManager(mFindToolbarManager);
        mToolbarHelper.initializeControls(
                mFindToolbarManager, null, layoutDriver, null, null, null,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        CustomTabActivity.this.finish();
                    }
                });

        mTab.setFullscreenManager(getFullscreenManager());
        mTab.loadUrl(new LoadUrlParams(url));
        mCustomTabContentHandler = new CustomTabContentHandler() {
            @Override
            public void loadUrl(LoadUrlParams params) {
                mTab.loadUrl(params);
            }

            @Override
            public long getSessionId() {
                return mSessionId;
            }
        };
        super.finishNativeInitialization();
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        setActiveContentHandler(mCustomTabContentHandler);

        if (!mRecordedStartupUma) {
            mRecordedStartupUma = true;
            ExternalAppId externalId =
                    IntentHandler.determineExternalIntentSource(getPackageName(), getIntent());
            RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId",
                    externalId.ordinal(), ExternalAppId.INDEX_BOUNDARY.ordinal());
        }
    }

    @Override
    public void onStopWithNative() {
        if (mAppMenuHandler != null) mAppMenuHandler.hideAppMenu();
        super.onStopWithNative();
        setActiveContentHandler(null);
    }

    @Override
    protected void onDeferredStartup() {
        super.onDeferredStartup();
        mToolbarHelper.onDeferredStartup();
    }

    @Override
    public boolean hasDoneFirstDraw() {
        return mToolbarHelper.hasDoneFirstDraw();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mAppMenuHandler != null) mAppMenuHandler.hideAppMenu();
        super.onConfigurationChanged(newConfig);
    }

    /**
     * Calculate the proper color for status bar and update it. Only works on L and future versions.
     */
    private void setStatusBarColor(int color) {
        // If the client did not specify the toolbar color, we do not change the status bar color.
        if (color == getResources().getColor(R.color.default_primary_color)) return;

        ApiCompatibilityUtils.setStatusBarColor(getWindow(),
                BrandColorUtils.computeStatusBarColor(color));
    }

    @Override
    public SingleTabModelSelector getTabModelSelector() {
        return (SingleTabModelSelector) super.getTabModelSelector();
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage) return mIntentDataProvider.getClientPackageName();
        return super.getPackageName();
    }

    @Override
    public void finish() {
        super.finish();
        if (mIntentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(mIntentDataProvider.getAnimationEnterRes(),
                    mIntentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        }
    }

    @Override
    protected boolean handleBackPressed() {
        if (mTab == null) return false;
        if (mTab.canGoBack()) {
            mTab.goBack();
        } else {
            finish();
        }
        return true;
    }

    @Override
    public boolean shouldShowAppMenu() {
        return mTab != null && mToolbarHelper.isInitialized();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int menuIndex = mAppMenuPropertiesDelegate.getIndexOfMenuItem(item);
        if (menuIndex >= 0) {
            mIntentDataProvider.clickMenuItemWithUrl(getApplicationContext(), menuIndex,
                    getTabModelSelector().getCurrentTab().getUrl());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.show_menu) {
            if (shouldShowAppMenu()) {
                mAppMenuHandler.showAppMenu(mToolbarHelper.getMenuAnchor(), true, false);
                return true;
            }
        } else if (id == R.id.open_in_chrome_id) {
            String url = getTabModelSelector().getCurrentTab().getUrl();
            Intent chromeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            chromeIntent.setPackage(getApplicationContext().getPackageName());
            chromeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(chromeIntent);
            RecordUserAction.record("CustomTabsMenuOpenInChrome");
            return true;
        } else if (id == R.id.find_in_page_id) {
            mFindToolbarManager.showToolbar();
            RecordUserAction.record("CustomTabsMenuFindInPage");
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    /**
     * @return The {@link AppMenuPropertiesDelegate} associated with this activity. For test
     *         purposes only.
     */
    @VisibleForTesting
    CustomTabAppMenuPropertiesDelegate getAppMenuPropertiesDelegate() {
        return mAppMenuPropertiesDelegate;
    }

    @Override
    @VisibleForTesting
    public AppMenuHandler getAppMenuHandler() {
        return mAppMenuHandler;
    }

    /**
     * @return The {@link CustomTabIntentDataProvider} for this {@link CustomTabActivity}. For test
     *         purposes only.
     */
    @VisibleForTesting
    CustomTabIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }
}
