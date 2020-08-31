// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.SystemClock;
import android.provider.Browser;
import android.support.test.filters.SmallTest;
import android.test.mock.MockPackageManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.ui.base.PageTransition;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

/**
 * Unittests for tab redirect handler.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class RedirectHandlerTest {
    private static final int TRANS_TYPE_OF_LINK_FROM_INTENT =
            PageTransition.LINK | PageTransition.FROM_API;
    private static final String TEST_PACKAGE_NAME = "test.package.name";
    private static Intent sYtIntent;
    private static Intent sMoblieYtIntent;
    private static Intent sFooIntent;

    static {
        try {
            sYtIntent = Intent.parseUri("http://youtube.com/", Intent.URI_INTENT_SCHEME);
            sMoblieYtIntent = Intent.parseUri("http://m.youtube.com/", Intent.URI_INTENT_SCHEME);
            sFooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ue) {
            // Ignore exception.
        }
    }

    @Before
    public void setUp() {
        CommandLine.init(new String[0]);
        ContextUtils.initApplicationContextForTests(new TestContext());
    }

    private List<ResolveInfo> queryIntentActivities(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(intent, 0);
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testRealIntentRedirect() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0);
        Assert.assertTrue(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertFalse(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sFooIntent)));
        Assert.assertFalse(handler.hasNewResolver(new ArrayList<ResolveInfo>()));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testEffectiveIntentRedirect_linkNavigation() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        Assert.assertTrue(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertFalse(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sFooIntent)));
        Assert.assertFalse(handler.hasNewResolver(new ArrayList<ResolveInfo>()));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testEffectiveIntentRedirect_formSubmit() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(PageTransition.FORM_SUBMIT, false, false, 0, 1);
        Assert.assertTrue(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertFalse(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sFooIntent)));
        Assert.assertFalse(handler.hasNewResolver(new ArrayList<ResolveInfo>()));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNoIntent() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(null, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sFooIntent)));
        Assert.assertFalse(handler.hasNewResolver(new ArrayList<ResolveInfo>()));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testClear() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0);
        Assert.assertTrue(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertFalse(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        handler.clear();
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sFooIntent)));
        Assert.assertFalse(handler.hasNewResolver(new ArrayList<ResolveInfo>()));
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNonLinkFromIntent() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sFooIntent)));
        Assert.assertFalse(handler.hasNewResolver(new ArrayList<ResolveInfo>()));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testUserInteraction() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0);
        Assert.assertTrue(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertFalse(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 1);
        Assert.assertFalse(handler.isOnEffectiveIntentRedirectChain());
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sMoblieYtIntent)));
        Assert.assertTrue(handler.hasNewResolver(queryIntentActivities(sFooIntent)));
        Assert.assertFalse(handler.hasNewResolver(new ArrayList<ResolveInfo>()));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(1, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testIntentFromChrome() {
        RedirectHandler handler = RedirectHandler.create();
        Intent fooIntent = new Intent(sFooIntent);
        fooIntent.putExtra(Browser.EXTRA_APPLICATION_ID, TEST_PACKAGE_NAME);
        handler.updateIntent(fooIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2);
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNavigationFromUserTyping() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertFalse(handler.isNavigationFromUserTyping());

        handler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        Assert.assertTrue(handler.isNavigationFromUserTyping());
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        Assert.assertTrue(handler.isNavigationFromUserTyping());

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2);
        Assert.assertFalse(handler.isNavigationFromUserTyping());

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testIntentHavingChromePackageName() {
        RedirectHandler handler = RedirectHandler.create();
        Intent fooIntent = new Intent(sFooIntent);
        fooIntent.setPackage(TEST_PACKAGE_NAME);
        handler.updateIntent(fooIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2);
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testRedirectFromCurrentNavigationShouldNotOverrideUrlLoading() {
        /////////////////////////////////////////////////////
        // 1. 3XX redirection should not override URL loading.
        /////////////////////////////////////////////////////
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.shouldNotOverrideUrlLoading());

        handler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        handler.setShouldNotOverrideUrlLoadingOnCurrentRedirectChain();

        handler.updateNewUrlLoading(PageTransition.LINK, true, false, 0, 0);
        Assert.assertTrue(handler.shouldNotOverrideUrlLoading());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        /////////////////////////////////////////////////////
        // 2. Effective redirection should not override URL loading.
        /////////////////////////////////////////////////////
        handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.shouldNotOverrideUrlLoading());

        handler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        handler.setShouldNotOverrideUrlLoadingOnCurrentRedirectChain();

        // Effective redirection occurred.
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        Assert.assertTrue(handler.shouldNotOverrideUrlLoading());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        /////////////////////////////////////////////////////
        // 3. New URL loading should not be affected.
        /////////////////////////////////////////////////////
        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2);
        Assert.assertFalse(handler.shouldNotOverrideUrlLoading());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    @RetryOnFailure
    public void testNavigationFromLinkWithoutUserGesture() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        handler.updateNewUrlLoading(
                PageTransition.LINK, false, false, SystemClock.elapsedRealtime(), 0);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertTrue(handler.shouldStayInApp(true));
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, false, SystemClock.elapsedRealtime(), 1);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertTrue(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2);
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    @RetryOnFailure
    public void testNavigationFromReload() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        handler.updateNewUrlLoading(
                PageTransition.RELOAD, false, false, SystemClock.elapsedRealtime(), 0);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertTrue(handler.shouldStayInApp(true));
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, false, SystemClock.elapsedRealtime(), 1);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertTrue(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2);
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    @RetryOnFailure
    public void testNavigationWithForwardBack() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        handler.updateNewUrlLoading(PageTransition.FORM_SUBMIT | PageTransition.FORWARD_BACK, false,
                true, SystemClock.elapsedRealtime(), 0);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertTrue(handler.shouldStayInApp(true));
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, false, SystemClock.elapsedRealtime(), 1);
        Assert.assertTrue(handler.shouldStayInApp(false));
        Assert.assertTrue(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2);
        Assert.assertFalse(handler.shouldStayInApp(false));
        Assert.assertFalse(handler.shouldStayInApp(true));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNavigationWithUninitializedUserInteractionTime() {
        // User interaction time could be uninitialized when a new document activity is opened after
        // clicking a link. In that case, the value is 0.
        final long uninitializedUserInteractionTime = 0;
        RedirectHandler handler = RedirectHandler.create();

        Assert.assertFalse(handler.isOnNavigation());
        handler.updateNewUrlLoading(PageTransition.LINK, false, true,
                uninitializedUserInteractionTime, RedirectHandler.INVALID_ENTRY_INDEX);
        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(RedirectHandler.INVALID_ENTRY_INDEX,
                handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    /**
     * Tests that a client side redirect without a user gesture to an external application does
     * cause us to leave Chrome, unless the app it would be launching is trusted.
     */
    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testClientRedirectWithoutUserGesture() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sFooIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(PageTransition.CLIENT_REDIRECT, false, false, 0, 0);
        Assert.assertTrue(handler.shouldStayInApp(true));
        Assert.assertFalse(handler.shouldStayInApp(true, true));
    }

    private static class TestPackageManager extends MockPackageManager {
        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
            List<ResolveInfo> resolves = new ArrayList<ResolveInfo>();
            if (intent.getDataString().startsWith("http://m.youtube.com")
                    || intent.getDataString().startsWith("http://youtube.com")) {
                ResolveInfo youTubeApp = new ResolveInfo();
                youTubeApp.activityInfo = new ActivityInfo();
                youTubeApp.activityInfo.packageName = "youtube";
                youTubeApp.activityInfo.name = "youtube";
                resolves.add(youTubeApp);
            } else {
                ResolveInfo fooApp = new ResolveInfo();
                fooApp.activityInfo = new ActivityInfo();
                fooApp.activityInfo.packageName = "foo";
                fooApp.activityInfo.name = "foo";
                resolves.add(fooApp);
            }
            return resolves;
        }
    }

    private static class TestContext extends AdvancedMockContext {
        @Override
        public PackageManager getPackageManager() {
            return new TestPackageManager();
        }

        @Override
        public String getPackageName() {
            return TEST_PACKAGE_NAME;
        }
    }
}
