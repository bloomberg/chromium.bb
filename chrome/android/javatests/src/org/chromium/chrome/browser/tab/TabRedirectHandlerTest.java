// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.SystemClock;
import android.provider.Browser;
import android.test.InstrumentationTestCase;
import android.test.mock.MockContext;
import android.test.mock.MockPackageManager;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.base.PageTransition;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

/**
 * Unittests for tab redirect handler.
 */
public class TabRedirectHandlerTest extends InstrumentationTestCase {
    private static final int TRANS_TYPE_OF_LINK_FROM_INTENT =
            PageTransition.LINK | PageTransition.FROM_API;
    private static final String TEST_PACKAGE_NAME = "test.package.name";
    private static Intent sYtIntent;
    private static Intent sMoblieYtIntent;
    private static Intent sFooIntent;
    private MockContext mContext;

    static {
        try {
            sYtIntent = Intent.parseUri("http://youtube.com/", Intent.URI_INTENT_SCHEME);
            sMoblieYtIntent = Intent.parseUri("http://m.youtube.com/", Intent.URI_INTENT_SCHEME);
            sFooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ue) {
            // Ignore exception.
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        CommandLine.init(new String[0]);
        mContext = new TestContext();
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testRealIntentRedirect() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, 0, 0);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, 0, 0);
        assertTrue(handler.isOnEffectiveIntentRedirectChain());
        assertFalse(handler.hasNewResolver(sMoblieYtIntent));
        assertTrue(handler.hasNewResolver(sFooIntent));
        assertFalse(handler.hasNewResolver(null));

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testEffectiveIntentRedirect() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, 0, 0);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 1);
        assertTrue(handler.isOnEffectiveIntentRedirectChain());
        assertFalse(handler.hasNewResolver(sMoblieYtIntent));
        assertTrue(handler.hasNewResolver(sFooIntent));
        assertFalse(handler.hasNewResolver(null));

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testNoIntent() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(null);
        assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, 0, 0);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, 0, 0);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        assertTrue(handler.hasNewResolver(sMoblieYtIntent));
        assertTrue(handler.hasNewResolver(sFooIntent));
        assertFalse(handler.hasNewResolver(null));

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testClear() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, 0, 0);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, 0, 0);
        assertTrue(handler.isOnEffectiveIntentRedirectChain());
        assertFalse(handler.hasNewResolver(sMoblieYtIntent));

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        handler.clear();
        assertFalse(handler.isOnNavigation());
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        assertTrue(handler.hasNewResolver(sMoblieYtIntent));
        assertTrue(handler.hasNewResolver(sFooIntent));
        assertFalse(handler.hasNewResolver(null));
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testNonLinkFromIntent() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(PageTransition.FORM_SUBMIT, false, 0, 0);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 1);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        assertTrue(handler.hasNewResolver(sMoblieYtIntent));
        assertTrue(handler.hasNewResolver(sFooIntent));
        assertFalse(handler.hasNewResolver(null));

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testUserInteraction() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, 0, 0);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, true, 0, 0);
        assertTrue(handler.isOnEffectiveIntentRedirectChain());
        assertFalse(handler.hasNewResolver(sMoblieYtIntent));

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(PageTransition.LINK, false,
                SystemClock.elapsedRealtime(), 1);
        assertFalse(handler.isOnEffectiveIntentRedirectChain());
        assertTrue(handler.hasNewResolver(sMoblieYtIntent));
        assertTrue(handler.hasNewResolver(sFooIntent));
        assertFalse(handler.hasNewResolver(null));

        assertTrue(handler.isOnNavigation());
        assertEquals(1, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testIntentFromChrome() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        Intent fooIntent = new Intent(sFooIntent);
        fooIntent.putExtra(Browser.EXTRA_APPLICATION_ID, TEST_PACKAGE_NAME);
        handler.updateIntent(fooIntent);
        assertFalse(handler.isOnNavigation());
        assertTrue(handler.shouldStayInChrome());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, 0, 0);
        assertTrue(handler.shouldStayInChrome());
        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 1);
        assertTrue(handler.shouldStayInChrome());

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(PageTransition.LINK, false, SystemClock.elapsedRealtime(), 2);
        assertFalse(handler.shouldStayInChrome());

        assertTrue(handler.isOnNavigation());
        assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testNavigationFromUserTyping() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.isOnNavigation());
        assertFalse(handler.shouldStayInChrome());

        handler.updateNewUrlLoading(PageTransition.TYPED, false, 0, 0);
        assertTrue(handler.shouldStayInChrome());
        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 1);
        assertTrue(handler.shouldStayInChrome());

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(PageTransition.LINK, false, SystemClock.elapsedRealtime(), 2);
        assertFalse(handler.shouldStayInChrome());

        assertTrue(handler.isOnNavigation());
        assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testIntentHavingChromePackageName() {
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        Intent fooIntent = new Intent(sFooIntent);
        fooIntent.setPackage(TEST_PACKAGE_NAME);
        handler.updateIntent(fooIntent);
        assertFalse(handler.isOnNavigation());
        assertTrue(handler.shouldStayInChrome());

        handler.updateNewUrlLoading(TRANS_TYPE_OF_LINK_FROM_INTENT, false, 0, 0);
        assertTrue(handler.shouldStayInChrome());
        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 1);
        assertTrue(handler.shouldStayInChrome());

        assertTrue(handler.isOnNavigation());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(PageTransition.LINK, false, SystemClock.elapsedRealtime(), 2);
        assertFalse(handler.shouldStayInChrome());

        assertTrue(handler.isOnNavigation());
        assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @SmallTest
    @Feature({"IntentHandling"})
    public void testRedirectFromCurrentNavigationShouldStayInChrome() {
        /////////////////////////////////////////////////////
        // 1. 3XX redirection should stay in Chrome.
        /////////////////////////////////////////////////////
        TabRedirectHandler handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.shouldStayInChrome());

        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 0);
        handler.setShouldStayInChromeUntilNewUrlLoading();

        handler.updateNewUrlLoading(PageTransition.LINK, true, 0, 0);
        assertTrue(handler.shouldStayInChrome());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        /////////////////////////////////////////////////////
        // 2. Effective redirection should stay in Chrome.
        /////////////////////////////////////////////////////
        handler = new TabRedirectHandler(mContext);
        handler.updateIntent(sYtIntent);
        assertFalse(handler.shouldStayInChrome());

        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 0);
        handler.setShouldStayInChromeUntilNewUrlLoading();

        // Effective redirection should stay in Chrome.
        handler.updateNewUrlLoading(PageTransition.LINK, false, 0, 1);
        assertTrue(handler.shouldStayInChrome());
        assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        /////////////////////////////////////////////////////
        // 3. New URL loading should not be affected.
        /////////////////////////////////////////////////////
        SystemClock.sleep(1);
        handler.updateNewUrlLoading(PageTransition.LINK, false, SystemClock.elapsedRealtime(), 2);
        assertFalse(handler.shouldStayInChrome());
        assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    private static class TestPackageManager extends MockPackageManager {
        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
            List<ResolveInfo> resolves = new ArrayList<ResolveInfo>();
            if (intent.getDataString().startsWith("http://m.youtube.com")
                    ||  intent.getDataString().startsWith("http://youtube.com")) {
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

    private static class TestContext extends MockContext {
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
