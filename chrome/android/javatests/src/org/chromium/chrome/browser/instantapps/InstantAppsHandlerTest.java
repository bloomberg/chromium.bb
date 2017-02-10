// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.net.Uri;
import android.nfc.NfcAdapter;
import android.provider.Browser;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for {@link InstantAppsHandler}.
 */
public class InstantAppsHandlerTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private TestInstantAppsHandler mHandler;
    private Context mContext;

    private static final Uri URI = Uri.parse("http://sampleurl.com/foo");
    private static final String INSTANT_APP_URL = "http://sampleapp.com/boo";
    private static final Uri REFERRER_URI = Uri.parse("http://www.wikipedia.org/");

    public InstantAppsHandlerTest() {
        super(ChromeActivity.class);
    }

    private Intent createViewIntent() {
        return new Intent(Intent.ACTION_VIEW, URI);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mContext = getInstrumentation().getTargetContext();
        mHandler = new TestInstantAppsHandler();

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean("applink.app_link_enabled", true);
        editor.putBoolean("applink.chrome_default_browser", true);
        editor.apply();
    }

    @Override
    public void tearDown() throws Exception {
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        super.tearDown();
    }

    @SmallTest
    public void testInstantAppsDisabled_incognito() {
        Intent i = createViewIntent();
        i.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);

        assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }


    @SmallTest
    public void testInstantAppsDisabled_doNotLaunch() {
        Intent i = createViewIntent();
        i.putExtra("com.google.android.gms.instantapps.DO_NOT_LAUNCH_INSTANT_APP", true);

        assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }

    @SmallTest
    public void testInstantAppsDisabled_mainIntent() {
        Intent i = new Intent(Intent.ACTION_MAIN);
        assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }

    @SmallTest
    public void testInstantAppsDisabled_intentOriginatingFromChrome() {
        Intent i = createViewIntent();
        i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());

        assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));

        Intent signedIntent = createViewIntent();
        signedIntent.setPackage(mContext.getPackageName());
        IntentHandler.addTrustedIntentExtras(signedIntent);

        assertFalse(mHandler.handleIncomingIntent(mContext, signedIntent, false, true));
    }

    @SmallTest
    public void testInstantAppsDisabled_launchFromShortcut() {
        Intent i = createViewIntent();
        i.putExtra(ShortcutHelper.EXTRA_SOURCE, 1);
        assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }

    @SmallTest
    public void testChromeNotDefault() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean("applink.chrome_default_browser", false);
        editor.apply();

        assertFalse(mHandler.handleIncomingIntent(mContext, createViewIntent(), false, true));

        // Even if Chrome is not default, launch Instant Apps for CustomTabs since those never
        // show disambiguation dialogs.
        Intent cti = createViewIntent()
                .putExtra("android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS", true);
        assertTrue(mHandler.handleIncomingIntent(mContext, cti, true, true));
    }

    @SmallTest
    public void testInstantAppsEnabled() {
        Intent i = createViewIntent();
        assertTrue(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, false,
                true));

        // Check that identical intent wouldn't be enabled for CustomTab flow.
        assertFalse(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, true,
                true));

        // Add CustomTab specific extra and check it's now enabled.
        i.putExtra("android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS", true);
        assertTrue(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, true,
                true));
    }

    @SmallTest
    public void testNfcIntent() {
        Intent i = new Intent(NfcAdapter.ACTION_NDEF_DISCOVERED);
        i.setData(Uri.parse("http://instantapp.com/"));
        assertTrue(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, false,
                true));
    }

    @SmallTest
    public void testHandleNavigation_startAsyncCheck() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse(mHandler.handleNavigation(mContext, INSTANT_APP_URL, REFERRER_URI,
                        getActivity().getTabModelSelector().getCurrentTab().getWebContents()));
            }
        });
        assertFalse(mHandler.mLaunchInstantApp);
        assertTrue(mHandler.mStartedAsyncCall);
    }

    @SmallTest
    public void testLaunchFromBanner() {
        // Intent to supervisor
        final Intent i = new Intent(Intent.ACTION_MAIN);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                new IntentFilter(Intent.ACTION_MAIN), null, true);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mHandler.launchFromBanner(new InstantAppsBannerData(
                        "App", null, INSTANT_APP_URL, REFERRER_URI, i, "Launch",
                        getActivity().getTabModelSelector().getCurrentTab().getWebContents()));
            }
        });

        // Started instant apps intent
        assertEquals(1, monitor.getHits());

        assertEquals(REFERRER_URI, i.getParcelableExtra(Intent.EXTRA_REFERRER));
        assertTrue(i.getBooleanExtra(InstantAppsHandler.IS_REFERRER_TRUSTED_EXTRA, false));
        assertTrue(i.getBooleanExtra(InstantAppsHandler.IS_USER_CONFIRMED_LAUNCH_EXTRA, false));
        assertEquals(mContext.getPackageName(),
                i.getStringExtra(InstantAppsHandler.TRUSTED_REFERRER_PKG_EXTRA));

        // After a banner launch, test that the next launch happens automatically

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(mHandler.handleNavigation(mContext, INSTANT_APP_URL, REFERRER_URI,
                        getActivity().getTabModelSelector().getCurrentTab().getWebContents()));
            }
        });
        assertFalse(mHandler.mStartedAsyncCall);
        assertTrue(mHandler.mLaunchInstantApp);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    static class TestInstantAppsHandler extends InstantAppsHandler {
        // Keeps track of whether startCheckForInstantApps() has been called.
        public volatile boolean mStartedAsyncCall;
        // Keeps track of whether launchInstantAppForNavigation() has been called.
        public volatile boolean mLaunchInstantApp;

        @Override
        protected boolean tryLaunchingInstantApp(Context context, Intent intent,
                boolean isCustomTabsIntent, Intent fallbackIntent) {
            return true;
        }

        @Override
        protected boolean launchInstantAppForNavigation(Context context, String url, Uri referrer) {
            mLaunchInstantApp = true;
            return true;
        }

        @Override
        protected boolean startCheckForInstantApps(Context context, String url, Uri referrer,
                WebContents webContents) {
            mStartedAsyncCall = true;
            return false;
        }
    }
}
