// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule.LONG_TIMEOUT_MS;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CUSTOM_TABS_UI_TYPE_MEDIA_VIEWER;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CUSTOM_TABS_UI_TYPE_READER_MODE;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsClient;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsServiceConnection;
import android.support.customtabs.CustomTabsSession;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.TabsOpenedFromExternalAppTest;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.browserservices.BrowserSessionContentUtils;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.TestBrowsingHistoryObserver;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for app menu, context menu, and toolbar of a {@link CustomTabActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class CustomTabActivityTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final int TIMEOUT_PAGE_LOAD_SECONDS = 10;
    private static final int MAX_MENU_CUSTOM_ITEMS = 5;
    private static final int NUM_CHROME_MENU_ITEMS = 5;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String GEOLOCATION_PAGE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String SELECT_POPUP_PAGE = "/chrome/test/data/android/select.html";
    private static final String FRAGMENT_TEST_PAGE = "/chrome/test/data/android/fragment.html";
    private static final String TEST_MENU_TITLE = "testMenuTitle";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";
    private static final String WEBLITE_PREFIX = "http://googleweblight.com/?lite_url=";
    private static final String JS_MESSAGE = "from_js";
    private static final String TITLE_FROM_POSTMESSAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        var received = '';"
            + "        onmessage = function (e) {"
            + "            var myport = e.ports[0];"
            + "            myport.onmessage = function (f) {"
            + "                received += f.data;"
            + "                document.title = received;"
            + "            }"
            + "        }"
            + "   </script>"
            + "</body></html>";
    private static final String MESSAGE_FROM_PAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        onmessage = function (e) {"
            + "            if (e.ports != null && e.ports.length > 0) {"
            + "               e.ports[0].postMessage(\"" + JS_MESSAGE + "\");"
            + "            }"
            + "        }"
            + "   </script>"
            + "</body></html>";
    private static final String ONLOAD_TITLE_CHANGE = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        window.onload = function () {"
            + "            document.title = \"nytimes.com\";"
            + "        }"
            + "   </script>"
            + "</body></html>";
    private static final String DELAYED_TITLE_CHANGE = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        window.onload = function () {"
            + "           setTimeout(function (){ document.title = \"nytimes.com\"}, 200);"
            + "        }"
            + "   </script>"
            + "</body></html>";

    private static int sIdToIncrement = 1;

    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(true);
            }
        });

        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        mWebServer = TestWebServer.start();

        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.setForcePrerender(true);
    }

    @After
    public void tearDown() throws Exception {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.setForcePrerender(false);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(false);
            }
        });

        mTestServer.stopAndDestroyServer();

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (getActivity() == null) return;
                AppMenuHandler handler = getActivity().getAppMenuHandler();
                if (handler != null) handler.hideAppMenu();
            }
        });
        mWebServer.shutdown();
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * @see CustomTabsTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);
    }

    /**
     * Add a bundle specifying a custom menu entry.
     * @param intent The intent to modify.
     * @return The pending intent associated with the menu entry.
     */
    private PendingIntent addMenuEntriesToIntent(Intent intent, int numEntries) {
        PendingIntent pi = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        ArrayList<Bundle> menuItems = new ArrayList<>();
        for (int i = 0; i < numEntries; i++) {
            Bundle bundle = new Bundle();
            bundle.putString(CustomTabsIntent.KEY_MENU_ITEM_TITLE, TEST_MENU_TITLE);
            bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
            menuItems.add(bundle);
        }
        intent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_MENU_ITEMS, menuItems);
        return pi;
    }

    private void addToolbarColorToIntent(Intent intent, int color) {
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, color);
    }

    /**
     * Adds an action button to the custom tab toolbar.
     * @return The {@link PendingIntent} that will be triggered when the action button is clicked.
     */
    private PendingIntent addActionButtonToIntent(Intent intent, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        PendingIntent pi = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);

        intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, bundle);
        return pi;
    }

    private Bundle makeBottomBarBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        PendingIntent pi = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);

        bundle.putInt(CustomTabsIntent.KEY_ID, sIdToIncrement++);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        return bundle;
    }

    private void openAppMenuAndAssertMenuShown() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().onMenuOrKeyboardAction(R.id.show_menu, false);
            }
        });

        CriteriaHelper.pollUiThread(new Criteria("App menu was not shown") {
            @Override
            public boolean isSatisfied() {
                return getActivity().getAppMenuHandler().isAppMenuShowing();
            }
        });
    }

    /**
     * @return The number of visible and enabled items in the given menu.
     */
    private int getActualMenuSize(Menu menu) {
        int actualMenuSize = 0;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible() && item.isEnabled()) actualMenuSize++;
        }
        return actualMenuSize;
    }

    /**
     * @return The number of visible items in the given menu.
     */
    private int getVisibleMenuSize(Menu menu) {
        int visibleMenuSize = 0;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible()) visibleMenuSize++;
        }
        return visibleMenuSize;
    }

    private Bitmap createTestBitmap(int widthDp, int heightDp) {
        Resources testRes = InstrumentationRegistry.getTargetContext().getResources();
        float density = testRes.getDisplayMetrics().density;
        return Bitmap.createBitmap((int) (widthDp * density),
                (int) (heightDp * density), Bitmap.Config.ARGB_8888);
    }

    /**
     * Test the entries in the context menu shown when long clicking an image.
     * @SmallTest
     * @RetryOnFailure
     * BUG=crbug.com/655970
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForImage() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "logo");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_search_by_image).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking a link.
     * @SmallTest
     * @RetryOnFailure
     * BUG=crbug.com/655970
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForLink() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "aboutLink");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_save_link_as).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking an mailto url.
     * @SmallTest
     * @RetryOnFailure
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForMailto() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "email");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking an tel url.
     * @SmallTest
     * @RetryOnFailure
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForTel() throws InterruptedException, TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "tel");
        Assert.assertEquals(expectedMenuSize, menu.size());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        Assert.assertTrue(menu.findItem(R.id.contextmenu_call).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_send_message).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        Assert.assertTrue(menu.findItem(R.id.contextmenu_copy).isVisible());

        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        Assert.assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the app menu.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testAppMenu() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 1;
        addMenuEntriesToIntent(intent, numMenuEntries);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu =
                mCustomTabActivityTestRule.getActivity().getAppMenuHandler().getAppMenu().getMenu();
        final int expectedMenuSize = numMenuEntries + NUM_CHROME_MENU_ITEMS;

        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
        Assert.assertNotNull(menu.findItem(R.id.forward_menu_id));
        Assert.assertNotNull(menu.findItem(R.id.bookmark_this_page_id));
        Assert.assertNotNull(menu.findItem(R.id.offline_page_id));
        Assert.assertNotNull(menu.findItem(R.id.info_menu_id));
        Assert.assertNotNull(menu.findItem(R.id.reload_menu_id));
        Assert.assertNotNull(menu.findItem(R.id.open_in_browser_id));
        Assert.assertFalse(menu.findItem(R.id.share_row_menu_id).isVisible());
        Assert.assertFalse(menu.findItem(R.id.share_row_menu_id).isEnabled());
        Assert.assertNotNull(menu.findItem(R.id.find_in_page_id));
        Assert.assertNotNull(menu.findItem(R.id.add_to_homescreen_id));
        Assert.assertNotNull(menu.findItem(R.id.request_desktop_site_row_menu_id));
    }

    /**
     * Test the entries in app menu for media viewer.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testAppMenuForMediaViewer() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_UI_TYPE, CUSTOM_TABS_UI_TYPE_MEDIA_VIEWER);
        IntentHandler.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu =
                mCustomTabActivityTestRule.getActivity().getAppMenuHandler().getAppMenu().getMenu();
        final int expectedMenuSize = 0;

        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
    }

    /**
     * Test the entries in app menu for Reader Mode.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testAppMenuForReaderMode() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CUSTOM_TABS_UI_TYPE_READER_MODE);
        IntentHandler.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu =
                mCustomTabActivityTestRule.getActivity().getAppMenuHandler().getAppMenu().getMenu();
        final int expectedMenuSize = 2;

        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
        Assert.assertTrue(menu.findItem(R.id.find_in_page_id).isVisible());
        Assert.assertTrue(menu.findItem(R.id.reader_mode_prefs_id).isVisible());
    }

    /**
     * Tests if the default share item can be shown in the app menu.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testShareMenuItem() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu =
                mCustomTabActivityTestRule.getActivity().getAppMenuHandler().getAppMenu().getMenu();
        Assert.assertTrue(menu.findItem(R.id.share_menu_id).isVisible());
        Assert.assertTrue(menu.findItem(R.id.share_menu_id).isEnabled());
    }


    /**
     * Test that only up to 5 entries are added to the custom menu.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testMaxMenuItems() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 7;
        Assert.assertTrue(MAX_MENU_CUSTOM_ITEMS < numMenuEntries);
        addMenuEntriesToIntent(intent, numMenuEntries);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu =
                mCustomTabActivityTestRule.getActivity().getAppMenuHandler().getAppMenu().getMenu();
        final int expectedMenuSize = MAX_MENU_CUSTOM_ITEMS + NUM_CHROME_MENU_ITEMS;
        Assert.assertNotNull("App menu is not initialized: ", menu);
        Assert.assertEquals(expectedMenuSize, getActualMenuSize(menu));
        Assert.assertEquals(expectedMenuSize, getVisibleMenuSize(menu));
    }

    /**
     * Test whether the custom menu is correctly shown and clicking it sends the right
     * {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testCustomMenuEntry() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = addMenuEntriesToIntent(intent, 1);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        getActivity().getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        openAppMenuAndAssertMenuShown();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MenuItem item = getActivity().getAppMenuPropertiesDelegate().getMenuItemForTitle(
                        TEST_MENU_TITLE);
                Assert.assertNotNull(item);
                Assert.assertTrue(getActivity().onOptionsItemSelected(item));
            }
        });

        CriteriaHelper.pollInstrumentationThread(new Criteria("Pending Intent was not sent.") {
            @Override
            public boolean isSatisfied() {
                return onFinished.isSent();
            }
        });
    }

    /**
     * Test whether clicking "Open in Chrome" takes us to a chrome normal tab, loading the same url.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testOpenInBrowser() throws InterruptedException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme(Uri.parse(mTestServer.getURL("/")).getScheme());
        final ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, false);
        openAppMenuAndAssertMenuShown();
        final String menuItemTitle = getActivity().getString(R.string.menu_open_in_product_default);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MenuItem item = getActivity().getAppMenuHandler()
                        .getAppMenu().getMenu().findItem(R.id.open_in_browser_id);
                Assert.assertNotNull(item);
                Assert.assertEquals(menuItemTitle, item.getTitle().toString());
                getActivity().onMenuOrKeyboardAction(R.id.open_in_browser_id, false);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return InstrumentationRegistry.getInstrumentation().checkMonitorHit(monitor, 1);
            }
        });
    }

    /**
     * Test whether a custom tab can be reparented to a new activity.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testTabReparentingBasic() throws InterruptedException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        reparentAndVerifyTab();
    }

    /**
     * Test whether a custom tab can be reparented to a new activity while showing an infobar.
     *
     * TODO(timloh): Use a different InfoBar type once we only use modals for permission prompts.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.MODAL_PERMISSION_PROMPTS)
    @RetryOnFailure
    public void testTabReparentingInfoBar() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        mTestServer.getURL(GEOLOCATION_PAGE)));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return currentTab != null
                        && currentTab.getInfoBarContainer() != null
                        && currentTab.getInfoBarContainer().getInfoBarsForTesting().size() == 1;
            }
        });
        final ChromeActivity newActivity = reparentAndVerifyTab();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab currentTab = newActivity.getActivityTab();
                return currentTab != null
                        && currentTab.getInfoBarContainer() != null
                        && currentTab.getInfoBarContainer().getInfoBarsForTesting().size() == 1;
            }
        });
    }

    /**
     * Test whether a custom tab can be reparented to a new activity while showing a select popup.
     */
    // @SmallTest
    // @RetryOnFailure
    @Test
    @DisabledTest // Disabled due to flakiness on browser_side_navigation apk - see crbug.com/707766
    public void testTabReparentingSelectPopup() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        mTestServer.getURL(SELECT_POPUP_PAGE)));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return currentTab != null
                        && currentTab.getContentViewCore() != null;
            }
        });
        try {
            DOMUtils.clickNode(
                    mCustomTabActivityTestRule.getActivity().getActivityTab().getContentViewCore(),
                    "select");
        } catch (TimeoutException e) {
            Assert.fail();
        }
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCustomTabActivityTestRule.getActivity()
                               .getActivityTab()
                               .getContentViewCore()
                               .getSelectPopupForTest()
                        != null;
            }
        });
        final ChromeActivity newActivity = reparentAndVerifyTab();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab currentTab = newActivity.getActivityTab();
                return currentTab != null
                        && currentTab.getContentViewCore() != null
                        && currentTab.getContentViewCore().getSelectPopupForTest() == null;
            }
        });
    }
    /**
     * Test whether the color of the toolbar is correctly customized. For L or later releases,
     * status bar color is also tested.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testToolbarColor() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        final int expectedColor = Color.RED;
        addToolbarColorToIntent(intent, expectedColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        Assert.assertEquals(expectedColor, toolbar.getBackground().getColor());
        Assert.assertFalse(toolbar.shouldEmphasizeHttpsScheme());
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            Assert.assertEquals(ColorUtils.getDarkenedColorForStatusBar(expectedColor),
                    mCustomTabActivityTestRule.getActivity().getWindow().getStatusBarColor());
        }
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testActionButton() throws InterruptedException {
        Bitmap expectedIcon = createTestBitmap(96, 48);
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = addActionButtonToIntent(intent, expectedIcon, "Good test");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        getActivity().getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest();

        Assert.assertNotNull(actionButton);
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertTrue("Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        Assert.assertTrue("Action button does not have the correct bitmap.",
                expectedIcon.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                actionButton.performClick();
            }
        });

        CriteriaHelper.pollInstrumentationThread(new Criteria("Pending Intent was not sent.") {
            @Override
            public boolean isSatisfied() {
                return onFinished.isSent();
            }
        });
    }

    /**
     * Test the case that the action button should not be shown, given a bitmap with unacceptable
     * height/width ratio.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testActionButtonBadRatio() throws InterruptedException {
        Bitmap expectedIcon = createTestBitmap(60, 20);
        Intent intent = createMinimalCustomTabIntent();
        addActionButtonToIntent(intent, expectedIcon, "Good test");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest();

        Assert.assertNotNull(actionButton);
        Assert.assertTrue(
                "Action button should not be shown", View.VISIBLE != actionButton.getVisibility());

        CustomTabIntentDataProvider dataProvider = getActivity().getIntentDataProvider();
        Assert.assertNull(dataProvider.getCustomButtonOnToolbar());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testBottomBar() throws InterruptedException {
        final int numItems = 3;
        final Bitmap expectedIcon = createTestBitmap(48, 24);
        final int barColor = Color.GREEN;

        Intent intent = createMinimalCustomTabIntent();
        ArrayList<Bundle> bundles = new ArrayList<>();
        for (int i = 1; i <= numItems; i++) {
            Bundle bundle = makeBottomBarBundle(i, expectedIcon, Integer.toString(i));
            bundles.add(bundle);
        }
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, bundles);
        intent.putExtra(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR, barColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        ViewGroup bottomBar = (ViewGroup) mCustomTabActivityTestRule.getActivity().findViewById(
                R.id.custom_tab_bottom_bar_wrapper);
        Assert.assertNotNull(bottomBar);
        Assert.assertTrue("Bottom Bar wrapper is not visible.",
                bottomBar.getVisibility() == View.VISIBLE && bottomBar.getHeight() > 0
                        && bottomBar.getWidth() > 0);
        Assert.assertEquals("Bottom Bar showing incorrect number of buttons.", numItems,
                bottomBar.getChildCount());
        Assert.assertEquals("Bottom bar not showing correct color", barColor,
                ((ColorDrawable) bottomBar.getBackground()).getColor());
        for (int i = 0; i < numItems; i++) {
            ImageButton button = (ImageButton) bottomBar.getChildAt(i);
            Assert.assertTrue("Bottom Bar button does not have the correct bitmap.",
                    expectedIcon.sameAs(((BitmapDrawable) button.getDrawable()).getBitmap()));
            Assert.assertTrue("Bottom Bar button is not visible.",
                    button.getVisibility() == View.VISIBLE && button.getHeight() > 0
                            && button.getWidth() > 0);
            Assert.assertEquals("Bottom Bar button does not have correct content description",
                    Integer.toString(i + 1), button.getContentDescription());
        }
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLaunchWithSession() throws InterruptedException {
        CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession();
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLoadNewUrlWithSession() throws InterruptedException {
        final Context context = InstrumentationRegistry.getTargetContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        warmUpAndLaunchUrlWithSession(intent);
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        Assert.assertFalse("CustomTabContentHandler handled intent with wrong session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return BrowserSessionContentUtils.handleInActiveContentIfNeeded(
                                CustomTabsTestUtils.createMinimalCustomTabIntent(
                                        context, mTestPage2));
                    }
                }));
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(mTestPage, new Callable<String>() {
            @Override
            public String call() {
                return getActivity().getActivityTab().getUrl();
            }
        }));
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        intent.setData(Uri.parse(mTestPage2));
                        return BrowserSessionContentUtils.handleInActiveContentIfNeeded(intent);
                    }
                }));
        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        try {
            pageLoadFinishedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(mTestPage2, new Callable<String>() {
                    @Override
                    public String call() {
                        return getActivity().getActivityTab().getUrl();
                    }
                }));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testCreateNewTab() throws InterruptedException, TimeoutException {
        final String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/customtabs/test_window_open.html");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        final TabModelSelector tabSelector =
                mCustomTabActivityTestRule.getActivity().getTabModelSelector();

        final CallbackHelper openTabHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tabSelector.getModel(false).addObserver(new EmptyTabModelObserver() {
                    @Override
                    public void didAddTab(Tab tab, TabLaunchType type) {
                        openTabHelper.notifyCalled();
                    }
                });
            }
        });
        DOMUtils.clickNode(
                mCustomTabActivityTestRule.getActivity().getActivityTab().getContentViewCore(),
                "new_window");

        openTabHelper.waitForCallback(0, 1);
        Assert.assertEquals(
                "A new tab should have been created.", 2, tabSelector.getModel(false).getCount());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testReferrerAddedAutomatically() throws InterruptedException {
        final Context context = InstrumentationRegistry.getInstrumentation()
                                        .getTargetContext()
                                        .getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        String packageName = context.getPackageName();
        final String referrer =
                IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        Assert.assertEquals(referrer, connection.getReferrerForSession(session).getUrl());

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                Assert.assertEquals(referrer, params.getReferrer().getUrl());
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return BrowserSessionContentUtils.handleInActiveContentIfNeeded(intent);
                    }
                }));
        try {
            pageLoadFinishedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testVerifiedReferrer() throws InterruptedException {
        final Context context = InstrumentationRegistry.getInstrumentation()
                                        .getTargetContext()
                                        .getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        String referrer = "https://example.com";
        intent.putExtra(Intent.EXTRA_REFERRER_NAME, referrer);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                OriginVerifier.addVerifiedOriginForPackage(
                        "app1", Uri.parse(referrer), CustomTabsService.RELATION_USE_AS_ORIGIN);
            }
        });

        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        Assert.assertEquals(getActivity().getIntentDataProvider().getSession(), session);

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                Assert.assertEquals(referrer, params.getReferrer().getUrl());
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return BrowserSessionContentUtils.handleInActiveContentIfNeeded(intent);
                    }
                }));
        try {
            pageLoadFinishedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }
    }

    /**
     * Tests that the navigation callbacks are sent.
     */
    @Test
    @SmallTest
    public void testCallbacksAreSent() {
        final Semaphore navigationStartSemaphore = new Semaphore(0);
        final Semaphore navigationFinishedSemaphore = new Semaphore(0);
        CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                Assert.assertNotEquals(CustomTabsCallback.NAVIGATION_FAILED, navigationEvent);
                Assert.assertNotEquals(CustomTabsCallback.NAVIGATION_ABORTED, navigationEvent);
                if (navigationEvent == CustomTabsCallback.NAVIGATION_STARTED) {
                    navigationStartSemaphore.release();
                } else if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) {
                    navigationFinishedSemaphore.release();
                }
            }
        });
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
            Assert.assertTrue(navigationStartSemaphore.tryAcquire(
                    TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
            Assert.assertTrue(navigationFinishedSemaphore.tryAcquire(
                    TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            Assert.fail();
        }
    }

    /**
     * Tests that Time To First Contentful Paint and Load Event Start timings are sent.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPageLoadMetricIsSent() {
        final AtomicReference<Long> firstContentfulPaintMs = new AtomicReference<>(-1L);
        final AtomicReference<Long> activityStartTimeMs = new AtomicReference<>(-1L);
        final AtomicReference<Long> loadEventStartMs = new AtomicReference<>(-1L);

        CustomTabsCallback cb = new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                Assert.assertEquals(CustomTabsConnection.PAGE_LOAD_METRICS_CALLBACK, callbackName);

                long navigationStart = args.getLong(PageLoadMetrics.NAVIGATION_START, -1);
                if (navigationStart == -1) {
                    // Return if the callback just had network quality information, which
                    // isn't tested.
                    return;
                }
                long current = SystemClock.uptimeMillis();
                Assert.assertTrue(navigationStart <= current);
                Assert.assertTrue(navigationStart >= activityStartTimeMs.get());

                long firstContentfulPaint =
                        args.getLong(PageLoadMetrics.FIRST_CONTENTFUL_PAINT, -1);
                if (firstContentfulPaint > 0) {
                    Assert.assertTrue(firstContentfulPaint <= (current - navigationStart));
                    firstContentfulPaintMs.set(firstContentfulPaint);
                }

                long loadEventStart = args.getLong(PageLoadMetrics.LOAD_EVENT_START, -1);
                if (loadEventStart > 0) {
                    Assert.assertTrue(loadEventStart <= (current - navigationStart));
                    loadEventStartMs.set(loadEventStart);
                }
            }
        };

        CustomTabsSession session = bindWithCallback(cb);
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            activityStartTimeMs.set(SystemClock.uptimeMillis());
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return firstContentfulPaintMs.get() > 0;
                }
            });
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return loadEventStartMs.get() > 0;
                }
            });
        } catch (InterruptedException e) {
            Assert.fail();
        }
    }

    private static void assertSuffixedHistogramTotalCount(long expected, String histogramPrefix) {
        for (String suffix : new String[] {".ZoomedIn", ".ZoomedOut"}) {
            Assert.assertEquals(expected,
                    RecordHistogram.getHistogramTotalCountForTesting(histogramPrefix + suffix));
        }
    }

    /**
     * Tests that one navigation in a custom tab records the histograms reflecting time from
     * intent to first navigation start/commit.
     */
    @Test
    @SmallTest
    public void testNavigationHistogramsRecorded() {
        String startHistogramPrefix = "CustomTabs.IntentToFirstNavigationStartTime";
        String commitHistogramPrefix = "CustomTabs.IntentToFirstCommitNavigationTime3";
        assertSuffixedHistogramTotalCount(0, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(0, commitHistogramPrefix);

        final Semaphore semaphore = new Semaphore(0);
        CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) semaphore.release();
            }
        });
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
            Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            Assert.fail();
        }

        assertSuffixedHistogramTotalCount(1, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(1, commitHistogramPrefix);
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set onload.
     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitle() throws InterruptedException {
        final String url = mWebServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(
                url, CustomTabsConnection.SpeculationParams.NO_SPECULATION, "nytimes.com");
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set during prerendering.

     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitlePrerendered() throws InterruptedException {
        final String url = mWebServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(
                url, CustomTabsConnection.SpeculationParams.PRERENDER, "nytimes.com");
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set delayed after load.

     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithDelayedTitle() throws InterruptedException {
        final String url = mWebServer.setResponse("/test.html", DELAYED_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(
                url, CustomTabsConnection.SpeculationParams.NO_SPECULATION, "nytimes.com");
    }

    private void hideDomainAndEnsureTitleIsSet(
            final String url, int speculation, final String expectedTitle) {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        intent.putExtra(
                CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        connection.mClientManager.setHideDomainForSession(token, true);

        if (speculation != CustomTabsConnection.SpeculationParams.NO_SPECULATION) {
            connection.setSpeculationModeForSession(token, speculation);
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
            try {
                ensureCompletedSpeculationForUrl(connection, url, speculation);
            } catch (Exception e1) {
                Assert.fail();
            }
        }

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                CustomTabToolbar toolbar =
                        (CustomTabToolbar) mCustomTabActivityTestRule.getActivity().findViewById(
                                R.id.toolbar);
                TextView titleBar = (TextView) toolbar.findViewById(R.id.title_bar);
                return titleBar != null && titleBar.isShown()
                        && (titleBar.getText()).toString().equals(expectedTitle);
            }
        });
    }

    /**
     * Tests that basic postMessage functionality works through sending a single postMessage
     * request.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageBasic() throws InterruptedException {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null));
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null) == CustomTabsService.RESULT_SUCCESS);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mCustomTabActivityTestRule.getActivity().getActivityTab().loadUrl(
                        new LoadUrlParams(mTestPage2));
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return currentTab.isLoadingAndRenderingDone();
            }
        });
        Assert.assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests that postMessage channel is not functioning after web contents get destroyed and also
     * not breaking things.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageWebContentsDestroyed() throws InterruptedException {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null));
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null) == CustomTabsService.RESULT_SUCCESS);

        final CallbackHelper renderProcessCallback = new CallbackHelper();
        new WebContentsObserver(
                mCustomTabActivityTestRule.getActivity().getActivityTab().getWebContents()) {
            @Override
            public void renderProcessGone(boolean wasOomProtected) {
                renderProcessCallback.notifyCalled();
            }
        };
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mCustomTabActivityTestRule.getActivity()
                        .getActivityTab()
                        .getWebContents()
                        .simulateRendererKilledForTesting(false);
            }
        });
        try {
            renderProcessCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }
        Assert.assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests whether validatePostMessageOrigin is necessary for making successful postMessage
     * requests.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageRequiresValidation() throws InterruptedException {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests the sent postMessage requests not only return success, but is also received by page.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedInPage() throws InterruptedException {
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null));
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });
        Assert.assertTrue(connection.postMessage(token, "New title", null)
                == CustomTabsService.RESULT_SUCCESS);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return "New title".equals(currentTab.getTitle());
            }
        });
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedFromPage() throws InterruptedException {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onMessageChannelReady(Bundle extras) {
                messageChannelHelper.notifyCalled();
            }

            @Override
            public void onPostMessage(String message, Bundle extras) {
                onPostMessageHelper.notifyCalled();
            }
        });
        session.requestPostMessageChannel(null);
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Assert.assertTrue(session.postMessage("Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }

        try {
            messageChannelHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }

        try {
            onPostMessageHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side even though
     * the request is sent after the page is created.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedFromPageWithLateRequest() throws InterruptedException {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onMessageChannelReady(Bundle extras) {
                messageChannelHelper.notifyCalled();
            }

            @Override
            public void onPostMessage(String message, Bundle extras) {
                onPostMessageHelper.notifyCalled();
            }
        });

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });

        session.requestPostMessageChannel(null);

        try {
            messageChannelHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }

        try {
            onPostMessageHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }

        Assert.assertTrue(session.postMessage("Message", null) == CustomTabsService.RESULT_SUCCESS);
    }

    private static final int BEFORE_MAY_LAUNCH_URL = 0;
    private static final int BEFORE_INTENT = 1;
    private static final int AFTER_INTENT = 2;

    /**
     * Tests a postMessage request chain can start while prerendering and continue afterwards.
     * Request sent before prerendering starts.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughPrerenderWithRequestBeforeMayLaunchUrl()
            throws InterruptedException {
        sendPostMessageDuringPrerenderTransition(BEFORE_MAY_LAUNCH_URL);
    }

    /**
     * Tests a postMessage request chain can start while prerendering and continue afterwards.
     * Request sent after prerendering starts and before intent launched.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughPrerenderWithRequestBeforeIntent()
            throws InterruptedException {
        sendPostMessageDuringPrerenderTransition(BEFORE_INTENT);
    }

    /**
     * Tests a postMessage request chain can start while prerendering and continue afterwards.
     * Request sent after intent received.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughPrerenderWithRequestAfterIntent()
            throws InterruptedException {
        sendPostMessageDuringPrerenderTransition(AFTER_INTENT);
    }

    private void sendPostMessageDuringPrerenderTransition(int requestTime)
            throws InterruptedException {
        sendPostMessageDuringSpeculationTransition(
                requestTime, CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent before the hidden tab start.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testPostMessageThroughHiddenTabWithRequestBeforeMayLaunchUrl()
            throws InterruptedException {
        sendPostMessageDuringHiddenTabTransition(BEFORE_MAY_LAUNCH_URL);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent after the hidden tab start and before intent launched.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testPostMessageThroughHiddenTabWithRequestBeforeIntent()
            throws InterruptedException {
        sendPostMessageDuringHiddenTabTransition(BEFORE_INTENT);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent after intent received.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testPostMessageThroughHiddenTabWithRequestAfterIntent()
            throws InterruptedException {
        sendPostMessageDuringHiddenTabTransition(AFTER_INTENT);
    }

    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    private void sendPostMessageDuringHiddenTabTransition(int requestTime)
            throws InterruptedException {
        sendPostMessageDuringSpeculationTransition(
                requestTime, CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
    }

    private void sendPostMessageDuringSpeculationTransition(int requestTime, int speculationMode)
            throws InterruptedException {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();

        final CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onMessageChannelReady(Bundle extras) {
                messageChannelHelper.notifyCalled();
            }
        });

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);

        boolean channelRequested = false;
        String titleString = "";
        String currentMessage = "";

        if (requestTime == BEFORE_MAY_LAUNCH_URL) {
            channelRequested = session.requestPostMessageChannel(null);
            Assert.assertTrue(channelRequested);
        }

        connection.setSpeculationModeForSession(token, speculationMode);
        session.mayLaunchUrl(Uri.parse(url), null, null);
        try {
            ensureCompletedSpeculationForUrl(connection, url, speculationMode);
        } catch (Exception e) {
            Assert.fail();
        }

        if (requestTime == BEFORE_INTENT) {
            channelRequested = session.requestPostMessageChannel(null);
            Assert.assertTrue(channelRequested);
        }

        if (channelRequested) {
            try {
                messageChannelHelper.waitForCallback(0);
            } catch (TimeoutException e) {
                Assert.fail();
            }
            currentMessage = "Prerendering ";
            // Initial title update during prerender.
            Assert.assertTrue(
                    session.postMessage(currentMessage, null) == CustomTabsService.RESULT_SUCCESS);
            titleString = currentMessage;
        }

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });

        if (requestTime == AFTER_INTENT) {
            channelRequested = session.requestPostMessageChannel(null);
            Assert.assertTrue(channelRequested);
            try {
                messageChannelHelper.waitForCallback(0);
            } catch (TimeoutException e) {
                Assert.fail();
            }
        }

        currentMessage = "and loading ";
        // Update title again and verify both updates went through with the channel still intact.
        Assert.assertTrue(
                session.postMessage(currentMessage, null) == CustomTabsService.RESULT_SUCCESS);
        titleString += currentMessage;

        // Request a new channel, verify it was created.
        session.requestPostMessageChannel(null);
        try {
            messageChannelHelper.waitForCallback(1);
        } catch (TimeoutException e) {
            Assert.fail();
        }

        currentMessage = "and refreshing";
        // Update title again and verify both updates went through with the channel still intact.
        Assert.assertTrue(
                session.postMessage(currentMessage, null) == CustomTabsService.RESULT_SUCCESS);
        titleString += currentMessage;

        final String title = titleString;
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return title.equals(currentTab.getTitle());
            }
        });
    }

    /**
     * Tests that when we use a pre-created renderer, the page loaded is the
     * only one in the navigation history.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testPrecreatedRenderer() throws Exception {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Bundle extras = new Bundle();
        // Forcing no prerendering implies falling back to simply creating a spare WebContents.
        extras.putInt(
                CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.NO_PRERENDERING);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), extras, null));
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });

        Assert.assertFalse(mCustomTabActivityTestRule.getActivity().getActivityTab().canGoBack());
        Assert.assertFalse(
                mCustomTabActivityTestRule.getActivity().getActivityTab().canGoForward());

        List<HistoryItem> history = getHistory();
        Assert.assertEquals(1, history.size());
        Assert.assertEquals(mTestPage, history.get(0).getUrl());
    }

    /** Tests that calling warmup() is optional without prerendering. */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testMayLaunchUrlWithoutWarmupNoSpeculation() {
        mayLaunchUrlWithoutWarmup(CustomTabsConnection.SpeculationParams.NO_SPECULATION);
    }

    /** Tests that calling mayLaunchUrl() without warmup() succeeds. */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testMayLaunchUrlWithoutWarmupPrerender() {
        mayLaunchUrlWithoutWarmup(CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /** Tests that calling mayLaunchUrl() without warmup() succeeds. */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testMayLaunchUrlWithoutWarmupHiddenTab() {
        mayLaunchUrlWithoutWarmup(CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
    }

    /**
     * Tests that launching a regular Chrome tab after warmup() gives the right layout.
     *
     * About the restrictions and switches: No FRE and no document mode to get a
     * ChromeTabbedActivity, and no tablets to have the tab switcher button.
     *
     * Non-regression test for crbug.com/547121.
     * @SmallTest
     * Disabled for flake: https://crbug.com/692025.
     */
    @Test
    @DisabledTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRegularChrome() {
        CustomTabsTestUtils.warmUpAndWait();
        Intent intent = new Intent(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        ChromeTabbedActivity.class.getName(), null, false);
        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertNotNull("Main activity did not start", activity);
        ChromeTabbedActivity tabbedActivity =
                (ChromeTabbedActivity) monitor.waitForActivityWithTimeout(
                        mCustomTabActivityTestRule.getActivityStartTimeoutMs());
        Assert.assertNotNull("ChromeTabbedActivity did not start", tabbedActivity);
        Assert.assertNotNull("Should have a tab switcher button.",
                tabbedActivity.findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests that launching a Custom Tab after warmup() gives the right layout.
     *
     * Non-regression test for crbug.com/547121.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRightToolbarLayout() {
        CustomTabsTestUtils.warmUpAndWait();
        mCustomTabActivityTestRule.startActivityCompletely(createMinimalCustomTabIntent());
        Assert.assertNull("Should not have a tab switcher button.",
                getActivity().findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * The expected behavior is that the prerender shouldn't be dropped, and that the fragment is
     * updated.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingAndChangingFragmentIgnoreFragments() throws Exception {
        prerenderAndChangeFragment(true, true);
    }

    /** Same as above, but the prerender matching should not ignore the fragment. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingAndChangingFragmentDontIgnoreFragments() throws Exception {
        prerenderAndChangeFragment(false, true);
    }

    /** Same as above, prerender matching ignores the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingAndChangingFragmentDontWait() throws Exception {
        prerenderAndChangeFragment(true, false);
    }

    /** Same as above, prerender matching doesn't ignore the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingAndChangingFragmentDontWaitDrop() throws Exception {
        prerenderAndChangeFragment(false, false);
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * There are two parameters changing the bahavior:
     * @param ignoreFragments Whether the prerender should be kept.
     * @param wait Whether to wait for the prerender to load.
     *
     * The prerender state is assessed through monitoring the properties of the test page.
     */
    private void prerenderAndChangeFragment(boolean ignoreFragments, boolean wait)
            throws Exception {
        speculateAndChangeFragment(
                ignoreFragments, wait, CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * The expected behavior is that the hidden tab shouldn't be dropped, and that the fragment is
     * updated.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHiddenTabAndChangingFragmentIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(true, true);
    }

    /** Same as above, but the hidden tab matching should not ignore the fragment. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHiddenTabAndChangingFragmentDontIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(false, true);
    }

    /** Same as above, hidden tab matching ignores the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHiddenTabAndChangingFragmentDontWait() throws Exception {
        startHiddenTabAndChangeFragment(true, false);
    }

    /** Same as above, hidden tab matching doesn't ignore the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHiddenTabAndChangingFragmentDontWaitDrop() throws Exception {
        startHiddenTabAndChangeFragment(false, false);
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * There are two parameters changing the bahavior:
     * @param ignoreFragments Whether the hidden tab should be kept.
     * @param wait Whether to wait for the hidden tab to load.
     */
    private void startHiddenTabAndChangeFragment(boolean ignoreFragments, boolean wait)
            throws Exception {
        speculateAndChangeFragment(
                ignoreFragments, wait, CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
    }

    private void speculateAndChangeFragment(
            boolean ignoreFragments, boolean wait, int speculationMode) throws Exception {
        String testUrl = mTestServer.getURL(FRAGMENT_TEST_PAGE);
        String initialFragment = "#test";
        String initialUrl = testUrl + initialFragment;
        String fragment = "#yeah";
        String urlWithFragment = testUrl + fragment;

        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, urlWithFragment);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        connection.setIgnoreUrlFragmentsForSession(token, ignoreFragments);
        connection.setSpeculationModeForSession(token, speculationMode);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(initialUrl), null, null));

        if (wait) ensureCompletedSpeculationForUrl(connection, initialUrl, speculationMode);

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
        final Tab tab = getActivity().getActivityTab();

        if (wait) {
            ElementContentCriteria initialVisibilityCriteria = new ElementContentCriteria(tab,
                    "visibility",
                    ignoreFragments ? getExpectedVisibilityForSpeculationMode(speculationMode)
                                    : "visible");
            ElementContentCriteria initialFragmentCriteria = new ElementContentCriteria(
                    tab, "initial-fragment", ignoreFragments ? initialFragment : fragment);
            ElementContentCriteria fragmentCriteria =
                    new ElementContentCriteria(tab, "fragment", fragment);
            // The tab hasn't been reloaded.
            CriteriaHelper.pollInstrumentationThread(initialVisibilityCriteria, 2000, 200);
            // No reload (initial fragment is correct).
            CriteriaHelper.pollInstrumentationThread(initialFragmentCriteria, 2000, 200);
            if (ignoreFragments) {
                CriteriaHelper.pollInstrumentationThread(fragmentCriteria, 2000, 200);
            }
        } else {
            CriteriaHelper.pollInstrumentationThread(new ElementContentCriteria(
                    tab, "initial-fragment", fragment), 2000, 200);
        }

        Assert.assertFalse(tab.canGoForward());
        Assert.assertFalse(tab.canGoBack());

        // TODO(ahemery):
        // Fragment misses will trigger two history entries
        // - url#speculated and url#actual are both inserted
        // This should ideally not be the case.
    }

    /**
     * Test whether the url shown on prerender gets updated from about:blank when it
     * completes in the background.
     * Non-regression test for crbug.com/554236.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingCorrectUrl() throws Exception {
        testSpeculateCorrectUrl(CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /**
     * Test whether the url shown on hidden tab gets updated from about:blank when it
     * completes in the background.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHiddenTabCorrectUrl() throws Exception {
        testSpeculateCorrectUrl(CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
    }

    /**
     * Test that a hidden tab speculation is executed as a prerender if the |CCT_BACKGROUND_TAB|
     * feature is disabled.
     **/
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "disable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB})
    public void testHiddenTabDisabled() throws Exception {
        testSpeculateCorrectUrl(CustomTabsConnection.SpeculationParams.HIDDEN_TAB,
                CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /**
     * Test that hidden tab speculation is not performed if 3rd party cookies are blocked.
     **/
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB})
    public void testHiddenTabThirdPartyCookiesBlocked() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        connection.setSpeculationModeForSession(
                token, CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
        connection.warmup(0);

        // Needs the browser process to be initialized.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge prefs = PrefServiceBridge.getInstance();
                boolean old_block_pref = prefs.isBlockThirdPartyCookiesEnabled();
                prefs.setBlockThirdPartyCookiesEnabled(false);
                Assert.assertTrue(connection.maySpeculate(token));
                prefs.setBlockThirdPartyCookiesEnabled(true);
                Assert.assertFalse(connection.maySpeculate(token));
                prefs.setBlockThirdPartyCookiesEnabled(old_block_pref);
            }
        });
    }

    private void testSpeculateCorrectUrl(int speculationMode) throws Exception {
        testSpeculateCorrectUrl(speculationMode, speculationMode);
    }

    private void testSpeculateCorrectUrl(int requestedSpeculationMode, int usedSpeculationMode)
            throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        connection.setSpeculationModeForSession(token, requestedSpeculationMode);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        ensureCompletedSpeculationForUrl(connection, mTestPage, usedSpeculationMode);

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            Assert.fail();
        }
        Assert.assertEquals(Uri.parse(mTestPage).getHost() + ":" + Uri.parse(mTestPage).getPort(),
                ((EditText) getActivity().findViewById(R.id.url_bar)).getText().toString());
    }

    /**
     * Test whether invalid urls are avoided for prerendering.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingInvalidUrl() throws Exception {
        testSpeculateInvalidUrl(CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /**
     * Test whether invalid urls are avoided for hidden tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testHiddenTabInvalidUrl() throws Exception {
        testSpeculateInvalidUrl(CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
    }

    private void testSpeculateInvalidUrl(int speculationMode) throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        connection.setSpeculationModeForSession(token, speculationMode);
        Assert.assertFalse(
                connection.mayLaunchUrl(token, Uri.parse("chrome://version"), null, null));
    }

    /**
     * Tests that the activity knows there is already a child process when warmup() has been called.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionWithWarmup() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            Assert.fail();
        }
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse("Warmup() should have allocated a child connection",
                        getActivity().shouldAllocateChildConnection());
            }
        });
    }

    /**
     * Tests that the activity knows there is no child process.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionNoWarmup() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsConnection.getInstance();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2));
        } catch (InterruptedException e) {
            Assert.fail();
        }
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertTrue(
                        "No spare renderer available, should allocate a child connection.",
                        getActivity().shouldAllocateChildConnection());
            }
        });
    }

    /**
     * Tests that the activity knows there is already a child process when prerendering.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionWithPrerender() throws Exception {
        testAllocateChildConnectionWithSpeculation(
                CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    private void testAllocateChildConnectionWithSpeculation(int speculationMode) throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        connection.setSpeculationModeForSession(token, speculationMode);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            Assert.fail();
        }
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse("Prerendering should have allocated a child connection",
                        getActivity().shouldAllocateChildConnection());
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRecreateSpareRendererOnTabClose() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsTestUtils.warmUpAndWait();

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            Assert.fail();
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
                final CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
                activity.finishAndClose(false);
            }
        });
        CriteriaHelper.pollUiThread(new Criteria("No new spare renderer") {
            @Override
            public boolean isSatisfied() {
                return WarmupManager.getInstance().hasSpareWebContents();
            }
        }, 2000, 200);
    }

    /**
     * Tests that prerendering accepts a referrer, and that this is not lost when launching the
     * Custom Tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingWithReferrer() throws Exception {
        testSpeculatingWithReferrer(CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is not lost when launching the
     * Custom Tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHiddenTabWithReferrer() throws Exception {
        testSpeculatingWithReferrer(CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
    }

    private void testSpeculatingWithReferrer(int speculationMode) throws Exception {
        String referrer = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), speculationMode, referrer, referrer);

        Tab tab = getActivity().getActivityTab();
        // The tab hasn't been reloaded.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility",
                        getExpectedVisibilityForSpeculationMode(speculationMode)),
                2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrer), 2000, 200);
    }

    /**
     * Tests that prerendering accepts a referrer, and that this is dropped when the tab
     * is launched with a mismatched referrer.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingWithMismatchedReferrers() throws Exception {
        testSpeculatingWithMismatchedReferrers(CustomTabsConnection.SpeculationParams.PRERENDER);
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is dropped when the tab
     * is launched with a mismatched referrer.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHiddenTabWithMismatchedReferrers() throws Exception {
        testSpeculatingWithMismatchedReferrers(CustomTabsConnection.SpeculationParams.HIDDEN_TAB);
    }

    private void testSpeculatingWithMismatchedReferrers(int speculationMode) throws Exception {
        String prerenderReferrer = "android-app://com.foo.me/";
        String launchReferrer = "android-app://com.foo.me.i.changed.my.mind/";
        maybeSpeculateAndLaunchWithReferrers(mTestServer.getURL(FRAGMENT_TEST_PAGE),
                speculationMode, prerenderReferrer, launchReferrer);

        Tab tab = getActivity().getActivityTab();
        // Prerender has been dropped.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "visible"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, launchReferrer), 2000, 200);
    }

    /** Tests that a client can set a referrer, without speculating. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testClientCanSetReferrer() throws Exception {
        String referrerUrl = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(mTestPage,
                CustomTabsConnection.SpeculationParams.NO_SPECULATION, null, referrerUrl);

        Tab tab = getActivity().getActivityTab();
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrerUrl), 2000, 200);
    }

    @Test
    @MediumTest
    public void testLaunchIncognitoURL() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final CustomTabActivity cctActivity = mCustomTabActivityTestRule.getActivity();
        final CallbackHelper mCctHiddenCallback = new CallbackHelper();
        final CallbackHelper mTabbedModeShownCallback = new CallbackHelper();
        final AtomicReference<ChromeTabbedActivity> tabbedActivity = new AtomicReference<>();

        ActivityStateListener listener = new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (activity == cctActivity
                        && (newState == ActivityState.STOPPED
                                   || newState == ActivityState.DESTROYED)) {
                    mCctHiddenCallback.notifyCalled();
                }

                if (activity instanceof ChromeTabbedActivity && newState == ActivityState.RESUMED) {
                    mTabbedModeShownCallback.notifyCalled();
                    tabbedActivity.set((ChromeTabbedActivity) activity);
                }
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(listener);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                cctActivity.getActivityTab().getTabWebContentsDelegateAndroid().openNewTab(
                        "about:blank", null, null, WindowOpenDisposition.OFF_THE_RECORD, false);
            }
        });

        mCctHiddenCallback.waitForCallback("CCT not hidden.", 0);
        mTabbedModeShownCallback.waitForCallback("Tabbed mode not shown.", 0);

        CriteriaHelper.pollUiThread(Criteria.equals(true, new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return tabbedActivity.get().areTabModelsInitialized();
            }
        }));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab tab = tabbedActivity.get().getActivityTab();
                if (tab == null) {
                    updateFailureReason("Tab is null");
                    return false;
                }
                if (!tab.isIncognito()) {
                    updateFailureReason("Incognito tab not selected");
                    return false;
                }
                if (!TextUtils.equals(tab.getUrl(), "about:blank")) {
                    updateFailureReason("Wrong URL loaded in incognito tab");
                    return false;
                }
                return true;
            }
        });

        ApplicationStatus.unregisterActivityStateListener(listener);
    }

    /**
     * Tests that a Weblite URL from an external app uses the lite_url param when Data Reduction
     * Proxy previews are being used.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchWebLiteURL() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(mTestPage, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy previews are not being used.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth",
            "disable-features=DataReductionProxyDecidesTransform",
            "data-reduction-proxy-lo-fi=always-on"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoPreviews() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy is not using Lo-Fi.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth",
            "disable-features=DataReductionProxyDecidesTransform"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoLoFi() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy is not being used.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoDataReductionProxy() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when the param
     * is an https URL.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchHttpsWebLiteURL() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage.replaceFirst("http", "https");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a URL from an external app does not use the lite_url param when the prefix is not
     * the WebLite url.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchNonWebLiteURL() throws Exception {
        final String testUrl = mTestPage2 + "/?lite_url=" + mTestPage;
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(testUrl, tab.getUrl());
    }

    /** Maybe prerenders a URL with a referrer, then launch it with another one. */
    private void maybeSpeculateAndLaunchWithReferrers(String url, int speculationMode,
            String speculationReferrer, String launchReferrer) throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsConnection connection = null;
        CustomTabsSessionToken token = null;
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        if (speculationMode != CustomTabsConnection.SpeculationParams.NO_SPECULATION) {
            connection = CustomTabsTestUtils.warmUpAndWait();
            token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            connection.newSession(token);
            connection.setSpeculationModeForSession(token, speculationMode);
            Bundle extras = null;
            if (speculationReferrer != null) {
                extras = new Bundle();
                extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(speculationReferrer));
            }
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), extras, null));
            ensureCompletedSpeculationForUrl(connection, url, speculationMode);
        }

        if (launchReferrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(launchReferrer));
        }
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail();
        }
    }

    /**
     * Test intended to verify the way we test history is correct.
     * Start an activity and then navigate to a different url.
     * We test NavigationController behavior through canGoBack/Forward as well
     * as browser history through an HistoryProvider.
     */
    @Test
    @SmallTest
    public void testHistoryNoSpeculation() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.loadUrl(new LoadUrlParams(mTestPage2));
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        Assert.assertTrue(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory();
        Assert.assertEquals(2, history.size());
        Assert.assertEquals(mTestPage2, history.get(0).getUrl());
        Assert.assertEquals(mTestPage, history.get(1).getUrl());
    }

    /**
     * The following test that history only has a single final page after speculation,
     * whether it was a hit or a miss.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterPrerenderHit() throws Exception {
        verifyHistoryAfterSpeculation(CustomTabsConnection.SpeculationParams.PRERENDER, true);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterPrerenderMiss() throws Exception {
        verifyHistoryAfterSpeculation(CustomTabsConnection.SpeculationParams.PRERENDER, false);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHistoryAfterHiddenTabHit() throws Exception {
        verifyHistoryAfterSpeculation(CustomTabsConnection.SpeculationParams.HIDDEN_TAB, true);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.CCT_BACKGROUND_TAB)
    public void testHistoryAfterHiddenTabMiss() throws Exception {
        verifyHistoryAfterSpeculation(CustomTabsConnection.SpeculationParams.HIDDEN_TAB, false);
    }

    private void verifyHistoryAfterSpeculation(int speculationMode, boolean speculationWasAHit)
            throws Exception {
        String speculationUrl = mTestPage;
        String navigationUrl = speculationWasAHit ? mTestPage : mTestPage2;
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, navigationUrl);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        connection.setSpeculationModeForSession(token, speculationMode);

        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(speculationUrl), null, null));
        ensureCompletedSpeculationForUrl(connection, speculationUrl, speculationMode);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = getActivity().getActivityTab();
        Assert.assertFalse(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory();
        Assert.assertEquals(1, history.size());
        Assert.assertEquals(navigationUrl, history.get(0).getUrl());
    }

    private void mayLaunchUrlWithoutWarmup(int speculationMode) {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        Bundle extras = null;
        if (speculationMode == CustomTabsConnection.SpeculationParams.NO_SPECULATION) {
            extras = new Bundle();
            extras.putInt(
                    CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.NO_PRERENDERING);
        }
        connection.setSpeculationModeForSession(token, speculationMode);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), extras, null));
        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            Assert.fail();
        }
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(mTestPage, tab.getUrl());
    }

    private ChromeActivity reparentAndVerifyTab() throws InterruptedException {
        ActivityResult result = null;
        final ActivityMonitor monitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                ChromeTabbedActivity.class.getName(), result, false);
        final Tab tabToBeReparented = getActivity().getActivityTab();
        final CallbackHelper tabHiddenHelper = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab) {
                tabHiddenHelper.notifyCalled();
            }
        };
        tabToBeReparented.addObserver(observer);
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().openCurrentUrlInBrowser(true);
                Assert.assertNull(getActivity().getActivityTab());
            }
        });
        // Use the extended CriteriaHelper timeout to make sure we get an activity
        final Activity lastActivity =
                monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull(
                "Monitor did not get an activity before hitting the timeout", lastActivity);
        Assert.assertTrue("Expected lastActivity to be a ChromeActivity, was "
                        + lastActivity.getClass().getName(),
                lastActivity instanceof ChromeActivity);
        final ChromeActivity newActivity = (ChromeActivity) lastActivity;
        CriteriaHelper.pollUiThread((new Criteria() {
            @Override
            public boolean isSatisfied() {
                return newActivity.getActivityTab() != null
                        && newActivity.getActivityTab().equals(tabToBeReparented);
            }
        }));
        Assert.assertEquals(newActivity.getWindowAndroid(), tabToBeReparented.getWindowAndroid());
        Assert.assertEquals(newActivity.getWindowAndroid(),
                tabToBeReparented.getContentViewCore().getWindowAndroid());
        Assert.assertFalse(
                tabToBeReparented.getDelegateFactory() instanceof CustomTabDelegateFactory);
        Assert.assertEquals("The tab should never be hidden during the reparenting process", 0,
                tabHiddenHelper.getCallCount());
        Assert.assertFalse(tabToBeReparented.isCurrentlyACustomTab());
        tabToBeReparented.removeObserver(observer);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(tabToBeReparented);
        while (observers.hasNext()) {
            Assert.assertFalse(observers.next() instanceof CustomTabObserver);
        }
        return newActivity;
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession(Intent intentWithSession)
            throws InterruptedException {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intentWithSession);
        connection.newSession(token);
        intentWithSession.setData(Uri.parse(mTestPage));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intentWithSession);
        return token;
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession() throws InterruptedException {
        return warmUpAndLaunchUrlWithSession(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage));
    }

    private static void ensureCompletedSpeculationForUrl(final CustomTabsConnection connection,
            final String url, int speculationMode) throws Exception {
        if (speculationMode == CustomTabsConnection.SpeculationParams.PRERENDER) {
            CriteriaHelper.pollUiThread(new Criteria("No Prerender") {
                @Override
                public boolean isSatisfied() {
                    return connection.mSpeculation != null
                            && connection.mSpeculation.webContents != null
                            && ExternalPrerenderHandler.hasPrerenderedAndFinishedLoadingUrl(
                                       Profile.getLastUsedProfile(), url,
                                       connection.mSpeculation.webContents);
                }
            }, LONG_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
        if (speculationMode == CustomTabsConnection.SpeculationParams.HIDDEN_TAB) {
            CriteriaHelper.pollUiThread(new Criteria("Tab was not created") {
                @Override
                public boolean isSatisfied() {
                    return connection.mSpeculation != null && connection.mSpeculation.tab != null;
                }
            }, LONG_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            ChromeTabUtils.waitForTabPageLoaded(connection.mSpeculation.tab, url);
        }
    }

    private CustomTabsSession bindWithCallback(final CustomTabsCallback callback) {
        final AtomicReference<CustomTabsSession> sessionReference = new AtomicReference<>(null);
        CustomTabsClient.bindCustomTabsService(InstrumentationRegistry.getContext(),
                InstrumentationRegistry.getTargetContext().getPackageName(),
                new CustomTabsServiceConnection() {
                    @Override
                    public void onServiceDisconnected(ComponentName name) {}

                    @Override
                    public void onCustomTabsServiceConnected(
                            ComponentName name, CustomTabsClient client) {
                        sessionReference.set(client.newSession(callback));
                    }
                });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return sessionReference.get() != null;
            }
        });
        return sessionReference.get();
    }

    /**
      * A helper class to monitor sending status of a {@link PendingIntent}.
      */
    private class OnFinishedForTest implements PendingIntent.OnFinished {

        private final PendingIntent mPi;
        private final AtomicBoolean mIsSent = new AtomicBoolean();
        private String mUri;

        /**
         * Create an instance of {@link OnFinishedForTest}, testing the given {@link PendingIntent}.
         */
        public OnFinishedForTest(PendingIntent pi) {
            mPi = pi;
        }

        /**
         * @return Whether {@link PendingIntent#send()} has been successfully triggered and the sent
         *         intent carries the correct Uri as data.
         */
        public boolean isSent() {
            return mIsSent.get() && mTestPage.equals(mUri);
        }

        @Override
        public void onSendFinished(PendingIntent pendingIntent, Intent intent, int resultCode,
                String resultData, Bundle resultExtras) {
            if (pendingIntent.equals(mPi)) {
                mUri = intent.getDataString();
                mIsSent.set(true);
            }
        }
    }

    private static class ElementContentCriteria extends Criteria {
        private final Tab mTab;
        private final String mJsFunction;
        private final String mExpected;

        public ElementContentCriteria(Tab tab, String elementId, String expected) {
            super("Page element is not as expected.");
            mTab = tab;
            mExpected = "\"" + expected + "\"";
            mJsFunction = "(function () { return document.getElementById(\"" + elementId
                    + "\").innerHTML; })()";
        }

        @Override
        public boolean isSatisfied() {
            String value = null;
            try {
                String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mTab.getWebContents(), mJsFunction);
                if (jsonText.equalsIgnoreCase("null")) jsonText = "";
                value = jsonText;
            } catch (InterruptedException | TimeoutException e) {
                e.printStackTrace();
                return false;
            }
            return TextUtils.equals(mExpected, value);
        }
    }

    private static String getExpectedVisibilityForSpeculationMode(int speculationMode) {
        switch (speculationMode) {
            case CustomTabsConnection.SpeculationParams.PRERENDER:
                return "prerender";
            case CustomTabsConnection.SpeculationParams.HIDDEN_TAB:
                return "hidden";
            default:
                return "visible";
        }
    }

    private static List<HistoryItem> getHistory() throws Exception {
        final TestBrowsingHistoryObserver historyObserver = new TestBrowsingHistoryObserver();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                BrowsingHistoryBridge historyService = new BrowsingHistoryBridge(false);
                historyService.setObserver(historyObserver);
                String historyQueryFilter = "";
                historyService.queryHistory(historyQueryFilter);
            }
        });
        historyObserver.getQueryCallback().waitForCallback(0);
        return historyObserver.getHistoryQueryResults();
    }
}
