// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Activity;
import android.app.Application;
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
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageButton;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.TabsOpenedFromExternalAppTest;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
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
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.BrowserStartupController.StartupCallback;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for app menu, context menu, and toolbar of a {@link CustomTabActivity}.
 */
public class CustomTabActivityTest extends CustomTabActivityTestBase {
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

    private static int sIdToIncrement = 1;

    private CustomTabActivity mActivity;
    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;
    private TestWebServer mWebServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(true);
            }
        });

        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        mWebServer = TestWebServer.start();

        CustomTabsConnection connection =
                CustomTabsConnection.getInstance((Application) appContext);
        connection.setForcePrerender(true);
    }

    @Override
    protected void tearDown() throws Exception {
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection =
                CustomTabsConnection.getInstance((Application) appContext);
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
                if (mActivity == null) return;
                AppMenuHandler handler = mActivity.getAppMenuHandler();
                if (handler != null) handler.hideAppMenu();
            }
        });
        mWebServer.shutdown();
        super.tearDown();
    }

    @Override
    protected void startActivityCompletely(Intent intent) {
        super.startActivityCompletely(intent);
        mActivity = getActivity();
    }

    /**
     * @see CustomTabsTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), mTestPage);
    }

    /**
     * Add a bundle specifying a custom menu entry.
     * @param intent The intent to modify.
     * @return The pending intent associated with the menu entry.
     */
    private PendingIntent addMenuEntriesToIntent(Intent intent, int numEntries) {
        PendingIntent pi = PendingIntent.getBroadcast(getInstrumentation().getTargetContext(), 0,
                new Intent(), 0);
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
        PendingIntent pi = PendingIntent.getBroadcast(getInstrumentation().getTargetContext(), 0,
                new Intent(), 0);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);

        intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, bundle);
        return pi;
    }

    private Bundle makeBottomBarBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        PendingIntent pi = PendingIntent.getBroadcast(getInstrumentation().getTargetContext(), 0,
                new Intent(), 0);

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
                mActivity.onMenuOrKeyboardAction(R.id.show_menu, false);
            }
        });

        CriteriaHelper.pollUiThread(new Criteria("App menu was not shown") {
            @Override
            public boolean isSatisfied() {
                return mActivity.getAppMenuHandler().isAppMenuShowing();
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

    private Bitmap createTestBitmap(int widthDp, int heightDp) {
        Resources testRes = getInstrumentation().getTargetContext().getResources();
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
    @DisabledTest
    public void testContextMenuEntriesForImage() throws InterruptedException, TimeoutException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(getActivity().getActivityTab(), "logo");
        assertEquals(expectedMenuSize, menu.size());

        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        assertNotNull(menu.findItem(R.id.contextmenu_call));
        assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        assertNotNull(menu.findItem(R.id.contextmenu_copy));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        assertTrue(menu.findItem(R.id.contextmenu_save_image).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_share_image).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_open_image).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_search_by_image).isVisible());

        assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_send_message).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_copy).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking a link.
     * @SmallTest
     * @RetryOnFailure
     * BUG=crbug.com/655970
     */
    @DisabledTest
    public void testContextMenuEntriesForLink() throws InterruptedException, TimeoutException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(getActivity().getActivityTab(), "aboutLink");
        assertEquals(expectedMenuSize, menu.size());

        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        assertNotNull(menu.findItem(R.id.contextmenu_call));
        assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        assertNotNull(menu.findItem(R.id.contextmenu_copy));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        assertTrue(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_save_link_as).isVisible());

        assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_send_message).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_copy).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking an mailto url.
     * @SmallTest
     * @RetryOnFailure
     */
    @DisabledTest
    public void testContextMenuEntriesForMailto() throws InterruptedException, TimeoutException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(getActivity().getActivityTab(), "email");
        assertEquals(expectedMenuSize, menu.size());

        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        assertNotNull(menu.findItem(R.id.contextmenu_call));
        assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        assertNotNull(menu.findItem(R.id.contextmenu_copy));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        assertTrue(menu.findItem(R.id.contextmenu_send_message).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_copy).isVisible());

        assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_call).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the context menu shown when long clicking an tel url.
     * @SmallTest
     * @RetryOnFailure
     */
    @DisabledTest
    public void testContextMenuEntriesForTel() throws InterruptedException, TimeoutException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        Menu menu = ContextMenuUtils.openContextMenu(getActivity().getActivityTab(), "tel");
        assertEquals(expectedMenuSize, menu.size());

        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        assertNotNull(menu.findItem(R.id.contextmenu_call));
        assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        assertNotNull(menu.findItem(R.id.contextmenu_copy));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        assertNotNull(menu.findItem(R.id.contextmenu_save_video));

        assertTrue(menu.findItem(R.id.contextmenu_call).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_send_message).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_add_to_contacts).isVisible());
        assertTrue(menu.findItem(R.id.contextmenu_copy).isVisible());

        assertFalse(menu.findItem(R.id.contextmenu_copy_link_address).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_share_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_open_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_search_by_image).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_copy_link_text).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_link_as).isVisible());
        assertFalse(menu.findItem(R.id.contextmenu_save_video).isVisible());
    }

    /**
     * Test the entries in the app menu.
     */
    @SmallTest
    @RetryOnFailure
    public void testAppMenu() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 1;
        addMenuEntriesToIntent(intent, numMenuEntries);
        startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = getActivity().getAppMenuHandler().getAppMenu().getMenu();
        final int expectedMenuSize = numMenuEntries + NUM_CHROME_MENU_ITEMS;
        final int actualMenuSize = getActualMenuSize(menu);

        assertNotNull("App menu is not initialized: ", menu);
        assertEquals(expectedMenuSize, actualMenuSize);
        assertNotNull(menu.findItem(R.id.forward_menu_id));
        assertNotNull(menu.findItem(R.id.bookmark_this_page_id));
        assertNotNull(menu.findItem(R.id.offline_page_id));
        assertNotNull(menu.findItem(R.id.info_menu_id));
        assertNotNull(menu.findItem(R.id.reload_menu_id));
        assertNotNull(menu.findItem(R.id.open_in_browser_id));
        assertFalse(menu.findItem(R.id.share_row_menu_id).isVisible());
        assertFalse(menu.findItem(R.id.share_row_menu_id).isEnabled());
        assertNotNull(menu.findItem(R.id.find_in_page_id));
        assertNotNull(menu.findItem(R.id.add_to_homescreen_id));
        assertNotNull(menu.findItem(R.id.request_desktop_site_id));
    }

    /**
     * Test the entries in app menu for media viewer.
     */
    @SmallTest
    @RetryOnFailure
    public void testAppMenuForMediaViewer() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_MEDIA_VIEWER, true);
        IntentHandler.addTrustedIntentExtras(intent);
        startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = getActivity().getAppMenuHandler().getAppMenu().getMenu();
        final int expectedMenuSize = 0;
        final int actualMenuSize = getActualMenuSize(menu);

        assertNotNull("App menu is not initialized: ", menu);
        assertEquals(expectedMenuSize, actualMenuSize);
        assertFalse(menu.findItem(R.id.find_in_page_id).isVisible());
        assertFalse(menu.findItem(R.id.add_to_homescreen_id).isVisible());
        assertFalse(menu.findItem(R.id.request_desktop_site_id).isVisible());
        assertFalse(menu.findItem(R.id.open_in_browser_id).isVisible());
    }

    /**
     * Tests if the default share item can be shown in the app menu.
     */
    @SmallTest
    @RetryOnFailure
    public void testShareMenuItem() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
        startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = getActivity().getAppMenuHandler().getAppMenu().getMenu();
        assertTrue(menu.findItem(R.id.share_menu_id).isVisible());
        assertTrue(menu.findItem(R.id.share_menu_id).isEnabled());
    }


    /**
     * Test that only up to 5 entries are added to the custom menu.
     */
    @SmallTest
    @RetryOnFailure
    public void testMaxMenuItems() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 7;
        addMenuEntriesToIntent(intent, numMenuEntries);
        startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        Menu menu = getActivity().getAppMenuHandler().getAppMenu().getMenu();
        final int expectedMenuSize = MAX_MENU_CUSTOM_ITEMS + NUM_CHROME_MENU_ITEMS;
        final int actualMenuSize = getActualMenuSize(menu);
        assertNotNull("App menu is not initialized: ", menu);
        assertEquals(expectedMenuSize, actualMenuSize);
    }

    /**
     * Test whether the custom menu is correctly shown and clicking it sends the right
     * {@link PendingIntent}.
     */
    @SmallTest
    @RetryOnFailure
    public void testCustomMenuEntry() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = addMenuEntriesToIntent(intent, 1);
        startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        mActivity.getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        openAppMenuAndAssertMenuShown();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MenuItem item = mActivity.getAppMenuPropertiesDelegate().getMenuItemForTitle(
                        TEST_MENU_TITLE);
                assertNotNull(item);
                assertTrue(mActivity.onOptionsItemSelected(item));
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
    @SmallTest
    @RetryOnFailure
    public void testOpenInBrowser() throws InterruptedException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme(Uri.parse(mTestServer.getURL("/")).getScheme());
        final ActivityMonitor monitor = getInstrumentation().addMonitor(filter, null, false);
        openAppMenuAndAssertMenuShown();
        final String menuItemTitle = mActivity.getString(R.string.menu_open_in_product_default);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MenuItem item = mActivity.getAppMenuHandler()
                        .getAppMenu().getMenu().findItem(R.id.open_in_browser_id);
                assertNotNull(item);
                assertEquals(menuItemTitle, item.getTitle().toString());
                mActivity.onMenuOrKeyboardAction(R.id.open_in_browser_id, false);
            }
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInstrumentation().checkMonitorHit(monitor, 1);
            }
        });
    }

    /**
     * Test whether a custom tab can be reparented to a new activity.
     */
    @SmallTest
    @RetryOnFailure
    public void testTabReparentingBasic() throws InterruptedException {
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        reparentAndVerifyTab();
    }

    /**
     * Test whether a custom tab can be reparented to a new activity while showing an infobar.
     */
    @SmallTest
    @RetryOnFailure
    public void testTabReparentingInfoBar() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(),
                        mTestServer.getURL(GEOLOCATION_PAGE)));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab currentTab = getActivity().getActivityTab();
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
    @SmallTest
    @RetryOnFailure
    public void testTabReparentingSelectPopup() throws InterruptedException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(),
                        mTestServer.getURL(SELECT_POPUP_PAGE)));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab currentTab = getActivity().getActivityTab();
                return currentTab != null
                        && currentTab.getContentViewCore() != null;
            }
        });
        try {
            DOMUtils.clickNode(getActivity().getActivityTab().getContentViewCore(), "select");
        } catch (TimeoutException e) {
            fail();
        }
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab().getContentViewCore().getSelectPopupForTest()
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
    @SmallTest
    @RetryOnFailure
    public void testToolbarColor() throws InterruptedException {
        Intent intent = createMinimalCustomTabIntent();
        final int expectedColor = Color.RED;
        addToolbarColorToIntent(intent, expectedColor);
        startCustomTabActivityWithIntent(intent);

        View toolbarView = getActivity().findViewById(R.id.toolbar);
        assertTrue("A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        assertEquals(expectedColor, toolbar.getBackground().getColor());
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            assertEquals(ColorUtils.getDarkenedColorForStatusBar(expectedColor),
                    getActivity().getWindow().getStatusBarColor());
        }
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @SmallTest
    @RetryOnFailure
    public void testActionButton() throws InterruptedException {
        Bitmap expectedIcon = createTestBitmap(96, 48);
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = addActionButtonToIntent(intent, expectedIcon, "Good test");
        startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        mActivity.getIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        View toolbarView = getActivity().findViewById(R.id.toolbar);
        assertTrue("A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest();

        assertNotNull(actionButton);
        assertNotNull(actionButton.getDrawable());
        assertTrue("Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        assertTrue("Action button does not have the correct bitmap.",
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
    @SmallTest
    @RetryOnFailure
    public void testActionButtonBadRatio() throws InterruptedException {
        Bitmap expectedIcon = createTestBitmap(60, 20);
        Intent intent = createMinimalCustomTabIntent();
        addActionButtonToIntent(intent, expectedIcon, "Good test");
        startCustomTabActivityWithIntent(intent);

        View toolbarView = getActivity().findViewById(R.id.toolbar);
        assertTrue("A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest();

        assertNotNull(actionButton);
        assertTrue("Action button should not be shown",
                View.VISIBLE != actionButton.getVisibility());

        CustomTabIntentDataProvider dataProvider = mActivity.getIntentDataProvider();
        assertNull(dataProvider.getCustomButtonOnToolbar());
    }

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
        startCustomTabActivityWithIntent(intent);

        ViewGroup bottomBar = (ViewGroup) getActivity()
                .findViewById(R.id.custom_tab_bottom_bar_wrapper);
        assertNotNull(bottomBar);
        assertTrue("Bottom Bar wrapper is not visible.", bottomBar.getVisibility() == View.VISIBLE
                && bottomBar.getHeight() > 0 && bottomBar.getWidth() > 0);
        assertEquals("Bottom Bar showing incorrect number of buttons.",
                numItems, bottomBar.getChildCount());
        assertEquals("Bottom bar not showing correct color", barColor,
                ((ColorDrawable) bottomBar.getBackground()).getColor());
        for (int i = 0; i < numItems; i++) {
            ImageButton button = (ImageButton) bottomBar.getChildAt(i);
            assertTrue("Bottom Bar button does not have the correct bitmap.",
                    expectedIcon.sameAs(((BitmapDrawable) button.getDrawable()).getBitmap()));
            assertTrue("Bottom Bar button is not visible.", button.getVisibility() == View.VISIBLE
                    && button.getHeight() > 0 && button.getWidth() > 0);
            assertEquals("Bottom Bar button does not have correct content description",
                    Integer.toString(i + 1), button.getContentDescription());
        }
    }

    @SmallTest
    @RetryOnFailure
    public void testLaunchWithSession() throws InterruptedException {
        CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession();
        assertEquals(mActivity.getIntentDataProvider().getSession(), session);
    }

    @SmallTest
    @RetryOnFailure
    public void testLoadNewUrlWithSession() throws InterruptedException {
        final Context context = getInstrumentation().getTargetContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        warmUpAndLaunchUrlWithSession(intent);
        assertEquals(mActivity.getIntentDataProvider().getSession(), session);
        assertFalse("CustomTabContentHandler handled intent with wrong session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return CustomTabActivity.handleInActiveContentIfNeeded(
                                CustomTabsTestUtils.createMinimalCustomTabIntent(
                                        context, mTestPage2));
                    }
                }));
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(mTestPage, new Callable<String>() {
            @Override
            public String call() {
                return mActivity.getActivityTab().getUrl();
            }
        }));
        assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        intent.setData(Uri.parse(mTestPage2));
                        return CustomTabActivity.handleInActiveContentIfNeeded(intent);
                    }
                }));
        final Tab tab = mActivity.getActivityTab();
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
            fail();
        }
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(mTestPage2, new Callable<String>() {
                    @Override
                    public String call() {
                        return mActivity.getActivityTab().getUrl();
                    }
                }));
    }

    @SmallTest
    @RetryOnFailure
    public void testCreateNewTab() throws InterruptedException, TimeoutException {
        final String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/customtabs/test_window_open.html");
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), testUrl));
        final TabModelSelector tabSelector = getActivity().getTabModelSelector();

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
        DOMUtils.clickNode(getActivity().getActivityTab().getContentViewCore(), "new_window");

        openTabHelper.waitForCallback(0, 1);
        assertEquals("A new tab should have been created.", 2,
                tabSelector.getModel(false).getCount());
    }

    @SmallTest
    @RetryOnFailure
    public void testReferrerAddedAutomatically() throws InterruptedException {
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        assertEquals(mActivity.getIntentDataProvider().getSession(), session);
        CustomTabsConnection connection = CustomTabsConnection.getInstance((Application) context);
        String packageName = context.getPackageName();
        final String referrer =
                IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        assertEquals(referrer, connection.getReferrerForSession(session).getUrl());

        final Tab tab = mActivity.getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                assertEquals(referrer, params.getReferrer().getUrl());
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        assertTrue("CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return CustomTabActivity.handleInActiveContentIfNeeded(intent);
                    }
                }));
        try {
            pageLoadFinishedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }
    }

    /**
     * Tests that the navigation callbacks are sent.
     */
    @SmallTest
    @RetryOnFailure
    public void testCallbacksAreSent() {
        final ArrayList<Integer> navigationEvents = new ArrayList<>();
        CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                navigationEvents.add(navigationEvent);
            }
        });
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                getInstrumentation().getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            startCustomTabActivityWithIntent(intent);
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return navigationEvents.contains(CustomTabsCallback.NAVIGATION_STARTED);
                }
            });
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return navigationEvents.contains(CustomTabsCallback.NAVIGATION_FINISHED);
                }
            });
        } catch (InterruptedException e) {
            fail();
        }
        assertFalse(navigationEvents.contains(CustomTabsCallback.NAVIGATION_FAILED));
        assertFalse(navigationEvents.contains(CustomTabsCallback.NAVIGATION_ABORTED));
    }

    /**
     * Tests that Time To First Contentful Paint and Load Event Start timings are sent.
     */
    @SmallTest
    @RetryOnFailure
    public void testPageLoadMetricIsSent() {
        final AtomicReference<Long> firstContentfulPaintMs = new AtomicReference<>(-1L);
        final AtomicReference<Long> activityStartTimeMs = new AtomicReference<>(-1L);
        final AtomicReference<Long> loadEventStartMs = new AtomicReference<>(-1L);

        CustomTabsCallback cb = new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                assertEquals(CustomTabsConnection.PAGE_LOAD_METRICS_CALLBACK, callbackName);

                long navigationStart = args.getLong(PageLoadMetrics.NAVIGATION_START, -1);
                long current = SystemClock.uptimeMillis();
                assertTrue(navigationStart <= current);
                assertTrue(navigationStart >= activityStartTimeMs.get());

                long firstContentfulPaint =
                        args.getLong(PageLoadMetrics.FIRST_CONTENTFUL_PAINT, -1);
                if (firstContentfulPaint > 0) {
                    assertTrue(firstContentfulPaint <= (current - navigationStart));
                    firstContentfulPaintMs.set(firstContentfulPaint);
                }

                long loadEventStart = args.getLong(PageLoadMetrics.LOAD_EVENT_START, -1);
                if (loadEventStart > 0) {
                    assertTrue(loadEventStart <= (current - navigationStart));
                    loadEventStartMs.set(loadEventStart);
                }
            }
        };

        CustomTabsSession session = bindWithCallback(cb);
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                getInstrumentation().getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            activityStartTimeMs.set(SystemClock.uptimeMillis());
            startCustomTabActivityWithIntent(intent);
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
            fail();
        }
    }

    /**
     * Tests that basic postMessage functionality works through sending a single postMessage
     * request.
     */
    @SmallTest
    @RetryOnFailure
    public void testPostMessageBasic() throws InterruptedException {
        final CustomTabsConnection connection = warmUpAndWait();
        Context context = getInstrumentation().getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        assertTrue(connection.newSession(token));
        assertTrue(connection.requestPostMessageChannel(token, null));
        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_SUCCESS);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getActivityTab().loadUrl(new LoadUrlParams(mTestPage2));
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return currentTab.isLoadingAndRenderingDone();
            }
        });
        assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests that postMessage channel is not functioning after web contents get destroyed and also
     * not breaking things.
     */
    @SmallTest
    @RetryOnFailure
    public void testPostMessageWebContentsDestroyed() throws InterruptedException {
        final CustomTabsConnection connection = warmUpAndWait();
        Context context = getInstrumentation().getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        assertTrue(connection.newSession(token));
        assertTrue(connection.requestPostMessageChannel(token, null));
        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_SUCCESS);

        final CallbackHelper renderProcessCallback = new CallbackHelper();
        new WebContentsObserver(getActivity().getActivityTab().getWebContents()) {
            @Override
            public void renderProcessGone(boolean wasOomProtected) {
                renderProcessCallback.notifyCalled();
            }
        };
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getActivityTab()
                        .getWebContents().simulateRendererKilledForTesting(false);
            }
        });
        try {
            renderProcessCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }
        assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests whether validatePostMessageOrigin is necessary for making successful postMessage
     * requests.
     */
    @SmallTest
    @RetryOnFailure
    public void testPostMessageRequiresValidation() throws InterruptedException {
        final CustomTabsConnection connection = warmUpAndWait();
        Context context = getInstrumentation().getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        assertTrue(connection.newSession(token));
        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        assertTrue(connection.postMessage(token, "Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests the sent postMessage requests not only return success, but is also received by page.
     */
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedInPage() throws InterruptedException {
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        final CustomTabsConnection connection = warmUpAndWait();
        Context context = getInstrumentation().getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        assertTrue(connection.newSession(token));
        assertTrue(connection.requestPostMessageChannel(token, null));
        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });
        assertTrue(connection.postMessage(token, "New title", null)
                == CustomTabsService.RESULT_SUCCESS);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return "New title".equals(currentTab.getTitle());
            }
        });
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side.
     */
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedFromPage() throws InterruptedException {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        warmUpAndWait();
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
                getInstrumentation().getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        assertTrue(session.postMessage("Message", null)
                == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);

        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }

        try {
            messageChannelHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }

        try {
            onPostMessageHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side even though
     * the request is sent after the page is created.
     */
    @SmallTest
    @RetryOnFailure
    public void testPostMessageReceivedFromPageWithLateRequest() throws InterruptedException {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        warmUpAndWait();
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
                getInstrumentation().getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });

        session.requestPostMessageChannel(null);

        try {
            messageChannelHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }

        try {
            onPostMessageHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }

        assertTrue(session.postMessage("Message", null)
                == CustomTabsService.RESULT_SUCCESS);
    }

    private static final int BEFORE_MAY_LAUNCH_URL = 0;
    private static final int BEFORE_INTENT = 1;
    private static final int AFTER_INTENT = 2;

    /**
     * Tests a postMessage request chain can start while prerendering and continue afterwards.
     * Request sent before prerendering starts.
     */
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
    @SmallTest
    @RetryOnFailure
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughPrerenderWithRequestAfterIntent()
            throws InterruptedException {
        sendPostMessageDuringPrerenderTransition(AFTER_INTENT);
    }

    private void sendPostMessageDuringPrerenderTransition(int requestTime)
            throws InterruptedException {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        warmUpAndWait();
        final CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onMessageChannelReady(Bundle extras) {
                messageChannelHelper.notifyCalled();
            }
        });
        boolean channelRequested = false;
        String titleString = "";
        String currentMessage = "";

        if (requestTime == BEFORE_MAY_LAUNCH_URL) {
            channelRequested = session.requestPostMessageChannel(null);
            assertTrue(channelRequested);
        }

        session.mayLaunchUrl(Uri.parse(url), null, null);
        try {
            ensureCompletedPrerenderForUrl(
                    CustomTabsConnection.getInstance((Application)
                            getInstrumentation().getTargetContext().getApplicationContext()), url);
        } catch (Exception e) {
            fail();
        }

        if (requestTime == BEFORE_INTENT) {
            channelRequested = session.requestPostMessageChannel(null);
            assertTrue(channelRequested);
        }

        if (channelRequested) {
            try {
                messageChannelHelper.waitForCallback(0);
            } catch (TimeoutException e) {
                fail();
            }
            currentMessage = "Prerendering ";
            // Initial title update during prerender.
            assertTrue(session.postMessage(currentMessage, null)
                    == CustomTabsService.RESULT_SUCCESS);
            titleString = currentMessage;
        }

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                getInstrumentation().getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return url.equals(currentTab.getUrl());
            }
        });

        if (requestTime == AFTER_INTENT) {
            channelRequested = session.requestPostMessageChannel(null);
            assertTrue(channelRequested);
            try {
                messageChannelHelper.waitForCallback(0);
            } catch (TimeoutException e) {
                fail();
            }
        }

        currentMessage = "and loading ";
        // Update title again and verify both updates went through with the channel still intact.
        assertTrue(session.postMessage(currentMessage, null)
                == CustomTabsService.RESULT_SUCCESS);
        titleString += currentMessage;

        // Request a new channel, verify it was created.
        session.requestPostMessageChannel(null);
        try {
            messageChannelHelper.waitForCallback(1);
        } catch (TimeoutException e) {
            fail();
        }

        currentMessage = "and refreshing";
        // Update title again and verify both updates went through with the channel still intact.
        assertTrue(session.postMessage(currentMessage, null)
                == CustomTabsService.RESULT_SUCCESS);
        titleString += currentMessage;

        final String title = titleString;
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return title.equals(currentTab.getTitle());
            }
        });
    }

    /**
     * Tests that when we use a pre-created renderer, the page loaded is the
     * only one in the navigation history.
     */
    @SmallTest
    @RetryOnFailure
    public void testPrecreatedRenderer() throws InterruptedException {
        CustomTabsConnection connection = warmUpAndWait();
        Context context = getInstrumentation().getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        assertTrue(connection.newSession(token));
        Bundle extras = new Bundle();
        extras.putInt(
                CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.NO_PRERENDERING);
        assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), extras, null));
        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final Tab currentTab = getActivity().getActivityTab();
                return mTestPage.equals(currentTab.getUrl());
            }
        });
        assertFalse(getActivity().getActivityTab().canGoBack());
    }

    /** Tests that calling mayLaunchUrl() without warmup() succeeds. */
    @SmallTest
    @RetryOnFailure
    public void testMayLaunchUrlWithoutWarmup() {
        mayLaunchUrlWithoutWarmup(false);
    }

    /** Tests that calling warmup() is optional without prerendering. */
    @SmallTest
    @RetryOnFailure
    public void testMayLaunchUrlWithoutWarmupNoPrerendering() {
        mayLaunchUrlWithoutWarmup(true);
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
    @DisabledTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRegularChrome() {
        warmUpAndWait();
        Intent intent =
                new Intent(getInstrumentation().getTargetContext(), ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Instrumentation.ActivityMonitor monitor =
                getInstrumentation().addMonitor(ChromeTabbedActivity.class.getName(), null, false);
        Activity activity = getInstrumentation().startActivitySync(intent);
        assertNotNull("Main activity did not start", activity);
        ChromeTabbedActivity tabbedActivity =
                (ChromeTabbedActivity) monitor.waitForActivityWithTimeout(
                        ACTIVITY_START_TIMEOUT_MS);
        assertNotNull("ChromeTabbedActivity did not start", tabbedActivity);
        assertNotNull("Should have a tab switcher button.",
                tabbedActivity.findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests that launching a Custom Tab after warmup() gives the right layout.
     *
     * Non-regression test for crbug.com/547121.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRightToolbarLayout() {
        warmUpAndWait();
        startActivityCompletely(createMinimalCustomTabIntent());
        assertNull("Should not have a tab switcher button.",
                mActivity.findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * The expected behavior is that the prerender shouldn't be dropped, and that the fragment is
     * updated.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingAndChangingFragmentIgnoreFragments() throws Exception {
        prerenderAndChangeFragment(true, true);
    }

    /** Same as above, but the prerender matching should not ignore the fragment. */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingAndChangingFragmentDontIgnoreFragments() throws Exception {
        prerenderAndChangeFragment(false, true);
    }

    /** Same as above, prerender matching ignores the fragment, don't wait. */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingAndChangingFragmentDontWait() throws Exception {
        prerenderAndChangeFragment(true, false);
    }

    /** Same as above, prerender matching doesn't ignore the fragment, don't wait. */
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
        String testUrl = mTestServer.getURL(FRAGMENT_TEST_PAGE);
        String initialFragment = "#test";
        String initialUrl = testUrl + initialFragment;
        String fragment = "#yeah";
        String urlWithFragment = testUrl + fragment;

        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection = warmUpAndWait();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, urlWithFragment);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        connection.setIgnoreUrlFragmentsForSession(token, ignoreFragments);
        assertTrue(connection.mayLaunchUrl(token, Uri.parse(initialUrl), null, null));

        if (wait) ensureCompletedPrerenderForUrl(connection, initialUrl);

        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
        final Tab tab = mActivity.getActivityTab();
        ElementContentCriteria initialVisibilityCriteria = new ElementContentCriteria(
                tab, "visibility", ignoreFragments ? "prerender" : "visible");
        ElementContentCriteria initialFragmentCriteria = new ElementContentCriteria(
                tab, "initial-fragment", ignoreFragments ? initialFragment : fragment);
        ElementContentCriteria fragmentCriteria = new ElementContentCriteria(
                tab, "fragment", fragment);

        if (wait) {
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
        assertFalse(tab.canGoForward());
        assertFalse(tab.canGoBack());
    }

    /**
     * Test whether the url shown on prerender gets updated from about:blank when the prerender
     * completes in the background.
     * Non-regression test for crbug.com/554236.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingCorrectUrl() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection = warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        ensureCompletedPrerenderForUrl(connection, mTestPage);

        try {
            startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                    context, mTestPage));
        } catch (InterruptedException e) {
            fail();
        }
        assertEquals(Uri.parse(mTestPage).getHost() + ":" + Uri.parse(mTestPage).getPort(),
                ((EditText) mActivity.findViewById(R.id.url_bar)).getText()
                        .toString());
    }

    /**
     * Test whether invalid urls are avoided for prerendering.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingInvalidUrl() throws Exception {
        final CustomTabsConnection connection = warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        assertFalse(connection.mayLaunchUrl(token, Uri.parse("chrome://version"), null, null));
    }

    /**
     * Tests that the activity knows there is already a child process when warmup() has been called.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionWithWarmup() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection = warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        try {
            startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            fail();
        }
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse("Warmup() should have allocated a child connection",
                        mActivity.shouldAllocateChildConnection());
            }
        });
    }

    /**
     * Tests that the activity knows there is no child process.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionNoWarmup() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection =
                CustomTabsConnection.getInstance((Application) context);
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);

        try {
            startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2));
        } catch (InterruptedException e) {
            fail();
        }
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue("No spare renderer available, should allocate a child connection.",
                        mActivity.shouldAllocateChildConnection());
            }
        });
    }

    /**
     * Tests that the activity knows there is already a child process when prerendering.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testAllocateChildConnectionWithPrerender() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection = warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        try {
            startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            fail();
        }
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse("Prerendering should have allocated a child connection",
                        mActivity.shouldAllocateChildConnection());
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRecreateSpareRendererOnTabClose() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        warmUpAndWait();

        try {
            startCustomTabActivityWithIntent(
                    CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        } catch (InterruptedException e) {
            fail();
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse(WarmupManager.getInstance().hasSpareWebContents());
                final CustomTabActivity activity = getActivity();
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
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingWithReferrer() throws Exception {
        String referrer = "android-app://com.foo.me/";
        maybePrerenderAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, referrer, referrer);

        Tab tab = mActivity.getActivityTab();
        // The tab hasn't been reloaded.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "prerender"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrer), 2000, 200);
    }

    /**
     * Tests that prerendering accepts a referrer, and that the prerender is dropped when the tab
     * is launched with a mismatched referrer.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testPrerenderingWithMismatchedReferrers() throws Exception {
        String prerenderReferrer = "android-app://com.foo.me/";
        String launchReferrer = "android-app://com.foo.me.i.changed.my.mind/";
        maybePrerenderAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, prerenderReferrer, launchReferrer);

        Tab tab = mActivity.getActivityTab();
        // Prerender has been dropped.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "visible"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, launchReferrer), 2000, 200);
    }

    /** Tests that a client can set a referrer, without prerendering. */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @RetryOnFailure
    public void testClientCanSetReferrer() throws Exception {
        String referrerUrl = "android-app://com.foo.me/";
        maybePrerenderAndLaunchWithReferrers(mTestPage, false, null, referrerUrl);

        Tab tab = mActivity.getActivityTab();
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrerUrl), 2000, 200);
    }

    @MediumTest
    public void testLaunchIncognitoURL() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        startCustomTabActivityWithIntent(intent);

        final CustomTabActivity cctActivity = getActivity();
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
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchWebLiteURL() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), testUrl));
        Tab tab = getActivity().getActivityTab();
        assertEquals(mTestPage, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy previews are not being used.
     */
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "data-reduction-proxy-lo-fi=always-on"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoPreviews() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), testUrl));
        Tab tab = getActivity().getActivityTab();
        assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy is not using Lo-Fi.
     */
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoLoFi() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), testUrl));
        Tab tab = getActivity().getActivityTab();
        assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when Data
     * Reduction Proxy is not being used.
     */
    @SmallTest
    @CommandLineFlags.Add({"data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchWebLiteURLNoDataReductionProxy() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage;
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), testUrl));
        Tab tab = getActivity().getActivityTab();
        assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a Weblite URL from an external app does not use the lite_url param when the param
     * is an https URL.
     */
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchHttpsWebLiteURL() throws Exception {
        final String testUrl = WEBLITE_PREFIX + mTestPage.replaceFirst("http", "https");
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), testUrl));
        Tab tab = getActivity().getActivityTab();
        assertEquals(testUrl, tab.getUrl());
    }

    /**
     * Tests that a URL from an external app does not use the lite_url param when the prefix is not
     * the WebLite url.
     */
    @SmallTest
    @CommandLineFlags.Add({"enable-spdy-proxy-auth", "data-reduction-proxy-lo-fi=always-on",
            "enable-data-reduction-proxy-lite-page"})
    @RetryOnFailure
    public void testLaunchNonWebLiteURL() throws Exception {
        final String testUrl = mTestPage2 + "/?lite_url=" + mTestPage;
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), testUrl));
        Tab tab = getActivity().getActivityTab();
        assertEquals(testUrl, tab.getUrl());
    }

    /** Maybe prerenders a URL with a referrer, then launch it with another one. */
    private void maybePrerenderAndLaunchWithReferrers(String url, boolean prerender,
            String prerenderReferrer, String launchReferrer) throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection = null;
        CustomTabsSessionToken token = null;
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        if (prerender) {
            connection = warmUpAndWait();
            token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            connection.newSession(token);
            Bundle extras = null;
            if (prerenderReferrer != null) {
                extras = new Bundle();
                extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(prerenderReferrer));
            }
            assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), extras, null));
            ensureCompletedPrerenderForUrl(connection, url);
        }

        if (launchReferrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(launchReferrer));
        }
        try {
            startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            fail();
        }
    }

    private void mayLaunchUrlWithoutWarmup(boolean noPrerendering) {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection =
                CustomTabsTestUtils.setUpConnection((Application) context);
        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        connection.newSession(token);
        Bundle extras = null;
        if (noPrerendering) {
            extras = new Bundle();
            extras.putInt(
                    CustomTabsConnection.DEBUG_OVERRIDE_KEY, CustomTabsConnection.NO_PRERENDERING);
        }
        assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), extras, null));
        try {
            startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                    context, mTestPage));
        } catch (InterruptedException e) {
            fail();
        }
        Tab tab = getActivity().getActivityTab();
        assertEquals(mTestPage, tab.getUrl());
    }

    private CustomTabsConnection warmUpAndWait() {
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection =
                CustomTabsTestUtils.setUpConnection((Application) context);
        final CallbackHelper startupCallbackHelper = new CallbackHelper();
        assertTrue(connection.warmup(0));
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .addStartupCompletedObserver(new StartupCallback() {
                            @Override
                            public void onSuccess(boolean alreadyStarted) {
                                startupCallbackHelper.notifyCalled();
                            }

                            @Override
                            public void onFailure() {
                                fail();
                            }
                        });
            }
        });

        try {
            startupCallbackHelper.waitForCallback(0);
        } catch (TimeoutException | InterruptedException e) {
            fail();
        }
        return connection;
    }

    private ChromeActivity reparentAndVerifyTab() throws InterruptedException {
        ActivityResult result = null;
        final ActivityMonitor monitor = getInstrumentation().addMonitor(
                ChromeTabbedActivity.class.getName(), result, false);
        final Tab tabToBeReparented = mActivity.getActivityTab();
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
                mActivity.openCurrentUrlInBrowser(true);
                assertNull(mActivity.getActivityTab());
            }
        });
        // Use the extended CriteriaHelper timeout to make sure we get an activity
        final Activity lastActivity =
                monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        assertNotNull("Monitor did not get an activity before hitting the timeout", lastActivity);
        assertTrue("Expected lastActivity to be a ChromeActivity, was "
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
        assertEquals(newActivity.getWindowAndroid(), tabToBeReparented.getWindowAndroid());
        assertEquals(newActivity.getWindowAndroid(),
                tabToBeReparented.getContentViewCore().getWindowAndroid());
        assertFalse(tabToBeReparented.getDelegateFactory() instanceof CustomTabDelegateFactory);
        assertEquals("The tab should never be hidden during the reparenting process",
                0, tabHiddenHelper.getCallCount());
        tabToBeReparented.removeObserver(observer);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(tabToBeReparented);
        while (observers.hasNext()) {
            assertFalse(observers.next() instanceof CustomTabObserver);
        }
        return newActivity;
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession(Intent intentWithSession)
            throws InterruptedException {
        CustomTabsConnection connection = warmUpAndWait();
        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intentWithSession);
        connection.newSession(token);
        intentWithSession.setData(Uri.parse(mTestPage));
        startCustomTabActivityWithIntent(intentWithSession);
        return token;
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession() throws InterruptedException {
        return warmUpAndLaunchUrlWithSession(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        getInstrumentation().getTargetContext(), mTestPage));
    }

    private static void ensureCompletedPrerenderForUrl(
            final CustomTabsConnection connection, final String url) throws Exception {
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

    private CustomTabsSession bindWithCallback(final CustomTabsCallback callback) {
        final AtomicReference<CustomTabsSession> sessionReference = new AtomicReference<>(null);
        CustomTabsClient.bindCustomTabsService(getInstrumentation().getContext(),
                getInstrumentation().getTargetContext().getPackageName(),
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
}
