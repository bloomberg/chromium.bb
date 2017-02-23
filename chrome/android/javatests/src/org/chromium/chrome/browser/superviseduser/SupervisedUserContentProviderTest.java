// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.superviseduser;

import android.accounts.Account;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.webrestrictions.browser.WebRestrictionsContentProvider;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Instrumentation test for SupervisedUserContentProvider.
 */
@RetryOnFailure
public class SupervisedUserContentProviderTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String DEFAULT_ACCOUNT = "test@gmail.com";
    private static final String AUTHORITY_SUFFIX = ".SupervisedUserProvider";
    private ContentResolver mResolver;
    private String mAuthority;
    private Uri mUri;

    public SupervisedUserContentProviderTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mResolver = getInstrumentation().getTargetContext().getContentResolver();
        assertNotNull(mResolver);
        mAuthority = getInstrumentation().getTargetContext().getPackageName() + AUTHORITY_SUFFIX;
        mUri = new Uri.Builder()
                       .scheme(ContentResolver.SCHEME_CONTENT)
                       .authority(mAuthority)
                       .path("authorized")
                       .build();
        SigninTestUtil.resetSigninState();
    }

    @Override
    public void tearDown() throws Exception {
        SigninTestUtil.resetSigninState();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        SigninTestUtil.setUpAuthForTest(getInstrumentation());

        // In principle the SupervisedUserContentProvider should work whenever Chrome is installed
        // (even if it isn't running), but to test it we need to set up a dummy child, and to do
        // this within a test we need to start Chrome.
        startMainActivityOnBlankPage();
    }

    @SmallTest
    public void testSupervisedUserDisabled() throws RemoteException, ExecutionException {
        ContentProviderClient client = mResolver.acquireContentProviderClient(mAuthority);
        assertNotNull(client);
        Cursor cursor = client.query(mUri, null, "url = 'http://google.com'", null, null);
        assertNull(cursor);
    }

    @SmallTest
    public void testNoSupervisedUser() throws RemoteException, ExecutionException {
        assertFalse(ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {

            @Override
            public Boolean call() throws Exception {
                PrefServiceBridge.getInstance().setSupervisedUserId("");
                return ChildAccountService.isChildAccount();
            }

        }));
        ContentProviderClient client = mResolver.acquireContentProviderClient(mAuthority);
        assertNotNull(client);
        SupervisedUserContentProvider.enableContentProviderForTesting();
        Cursor cursor = client.query(mUri, null, "url = 'http://google.com'", null, null);
        assertNotNull(cursor);
        assertEquals(WebRestrictionsContentProvider.BLOCKED, cursor.getInt(0));
        cursor = client.query(mUri, null, "url = 'http://www.notgoogle.com'", null, null);
        assertNotNull(cursor);
        assertEquals(WebRestrictionsContentProvider.BLOCKED, cursor.getInt(0));
    }

    @SmallTest
    public void testWithSupervisedUser() throws RemoteException, ExecutionException {
        final Account account = SigninTestUtil.addAndSignInTestAccount();
        assertNotNull(account);
        assertTrue(ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {

            @Override
            public Boolean call() throws Exception {
                PrefServiceBridge.getInstance().setSupervisedUserId("ChildAccountSUID");
                return ChildAccountService.isChildAccount();
            }

        }));
        ContentProviderClient client = mResolver.acquireContentProviderClient(mAuthority);
        assertNotNull(client);
        SupervisedUserContentProvider.enableContentProviderForTesting();
        // setFilter for testing sets a default filter that blocks by default.
        mResolver.call(mUri, "setFilterForTesting", null, null);
        Cursor cursor = client.query(mUri, null, "url = 'http://www.google.com'", null, null);
        assertNotNull(cursor);
        assertEquals(WebRestrictionsContentProvider.BLOCKED, cursor.getInt(0));
    }
}
