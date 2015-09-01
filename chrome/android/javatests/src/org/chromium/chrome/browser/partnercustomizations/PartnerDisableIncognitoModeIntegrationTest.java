// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.PopupMenu;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests for the partner disabling incognito mode feature.
 */
public class PartnerDisableIncognitoModeIntegrationTest extends
        BasePartnerBrowserCustomizationIntegrationTest {

    private static final String TEST_URLS[] = {
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html"),
            TestHttpServerClient.getUrl("chrome/test/data/android/ok.txt"),
            TestHttpServerClient.getUrl("chrome/test/data/android/test.html")
    };

    @Override
    public void startMainActivity() throws InterruptedException {
        // Each test will launch main activity, so purposefully omit here.
    }

    private void setParentalControlsEnabled(boolean enabled) {
        Uri uri = PartnerBrowserCustomizations.buildQueryUri(
                PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.INCOGNITO_MODE_DISABLED_KEY, enabled);
        Context context = getInstrumentation().getTargetContext();
        context.getContentResolver().call(uri, "setIncognitoModeDisabled", null, bundle);
    }

    private void assertIncognitoMenuItemEnabled(boolean enabled) throws ExecutionException {
        Menu menu = ThreadUtils.runOnUiThreadBlocking(new Callable<Menu>() {
            @Override
            public Menu call() throws Exception {
                // PopupMenu is a convenient way of building a temp menu.
                PopupMenu tempMenu = new PopupMenu(
                        getActivity(), getActivity().findViewById(R.id.menu_anchor_stub));
                tempMenu.inflate(R.menu.main_menu);
                Menu menu = tempMenu.getMenu();

                getActivity().prepareMenu(menu);
                return menu;
            }
        });
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.new_incognito_tab_menu_id && item.isVisible()) {
                assertEquals("Menu item enabled state is not correct.", enabled, item.isEnabled());
            }
        }
    }

    private boolean waitForParentalControlsEnabledState(final boolean parentalControlsEnabled)
            throws InterruptedException {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // areParentalControlsEnabled is updated on a background thread, so we
                // also wait on the isIncognitoModeEnabled to ensure the updates on the
                // UI thread have also triggered.
                boolean retVal = parentalControlsEnabled
                        == PartnerBrowserCustomizations.isIncognitoDisabled();
                retVal &= parentalControlsEnabled
                        != PrefServiceBridge.getInstance().isIncognitoModeEnabled();
                return retVal;
            }
        });
    }

    private void toggleActivityForegroundState() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onPause();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onStop();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onStart();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onResume();
            }
        });
    }

    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testIncognitoEnabledIfNoParentalControls() throws InterruptedException {
        setParentalControlsEnabled(false);
        startMainActivityOnBlankPage();
        assertTrue(waitForParentalControlsEnabledState(false));
        newIncognitoTabFromMenu();
    }

    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testIncognitoMenuItemEnabledBasedOnParentalControls()
            throws InterruptedException, ExecutionException {
        setParentalControlsEnabled(true);
        startMainActivityOnBlankPage();
        assertTrue(waitForParentalControlsEnabledState(true));
        assertIncognitoMenuItemEnabled(false);

        setParentalControlsEnabled(false);
        toggleActivityForegroundState();
        assertTrue(waitForParentalControlsEnabledState(false));
        assertIncognitoMenuItemEnabled(true);
    }

    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testEnabledParentalControlsClosesIncognitoTabs() throws InterruptedException {
        setParentalControlsEnabled(false);
        startMainActivityOnBlankPage();
        assertTrue(waitForParentalControlsEnabledState(false));

        loadUrlInNewTab(TEST_URLS[0], true);
        loadUrlInNewTab(TEST_URLS[1], true);
        loadUrlInNewTab(TEST_URLS[2], true);
        loadUrlInNewTab(TEST_URLS[0], false);

        setParentalControlsEnabled(true);
        toggleActivityForegroundState();
        assertTrue(waitForParentalControlsEnabledState(true));

        assertTrue("Incognito tabs did not close as expected",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return incognitoTabsCount() == 0;
                    }
                }));
    }
}
