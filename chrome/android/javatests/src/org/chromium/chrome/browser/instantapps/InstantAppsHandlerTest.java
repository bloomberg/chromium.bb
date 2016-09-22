// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.nfc.NfcAdapter;
import android.provider.Browser;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;

/**
 * Unit tests for {@link InstantAppsHandler}.
 */
public class InstantAppsHandlerTest extends InstrumentationTestCase {

    private TestInstantAppsHandler mHandler;
    private Context mContext;

    private static final Uri URI = Uri.parse("http://sampleurl.com/foo");

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
    public void testInstantAppsDisabled_disabledByFlag() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean("applink.app_link_enabled", false);
        editor.apply();

        assertFalse(mHandler.handleIncomingIntent(mContext, createViewIntent(), false));
        assertFalse(mHandler.handleIncomingIntent(mContext, createViewIntent(), true));
    }

    @SmallTest
    public void testInstantAppsDisabled_incognito() {
        Intent i = createViewIntent();
        i.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);

        assertFalse(mHandler.handleIncomingIntent(mContext, i, false));
    }


    @SmallTest
    public void testInstantAppsDisabled_doNotLaunch() {
        Intent i = createViewIntent();
        i.putExtra("com.google.android.gms.instantapps.DO_NOT_LAUNCH_INSTANT_APP", true);

        assertFalse(mHandler.handleIncomingIntent(mContext, i, false));
    }

    @SmallTest
    public void testInstantAppsDisabled_mainIntent() {
        Intent i = new Intent(Intent.ACTION_MAIN);
        assertFalse(mHandler.handleIncomingIntent(mContext, i, false));
    }

    @SmallTest
    public void testInstantAppsDisabled_intentOriginatingFromChrome() {
        Intent i = createViewIntent();
        i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());

        assertFalse(mHandler.handleIncomingIntent(mContext, i, false));

        Intent signedIntent = createViewIntent();
        signedIntent.setPackage(mContext.getPackageName());
        IntentHandler.addTrustedIntentExtras(signedIntent, mContext);

        assertFalse(mHandler.handleIncomingIntent(mContext, signedIntent, false));
    }

    @SmallTest
    public void testChromeNotDefault() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean("applink.chrome_default_browser", false);
        editor.apply();

        assertFalse(mHandler.handleIncomingIntent(mContext, createViewIntent(), false));

        // Even if Chrome is not default, launch Instant Apps for CustomTabs since those never
        // show disambiguation dialogs.
        Intent cti = createViewIntent()
                .putExtra("android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS", true);
        assertTrue(mHandler.handleIncomingIntent(mContext, cti, true));
    }

    @SmallTest
    public void testInstantAppsEnabled() {
        Intent i = createViewIntent();
        assertTrue(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, false));

        // Check that identical intent wouldn't be enabled for CustomTab flow.
        assertFalse(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, true));

        // Add CustomTab specific extra and check it's now enabled.
        i.putExtra("android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS", true);
        assertTrue(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, true));
    }

    @SmallTest
    public void testNfcIntent() {
        Intent i = new Intent(NfcAdapter.ACTION_NDEF_DISCOVERED);
        i.setData(Uri.parse("http://instantapp.com/"));
        assertTrue(mHandler.handleIncomingIntent(getInstrumentation().getContext(), i, false));
    }

    static class TestInstantAppsHandler extends InstantAppsHandler {
        @Override
        protected boolean tryLaunchingInstantApp(Context context, Intent intent,
                boolean isCustomTabsIntent, Intent fallbackIntent) {
            return true;
        }
    }
}
