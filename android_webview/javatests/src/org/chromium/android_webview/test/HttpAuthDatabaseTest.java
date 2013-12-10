// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.base.test.util.Feature;

/**
 * Test suite for HttpAuthDatabase.
 */
public class HttpAuthDatabaseTest extends AndroidTestCase {

    private static final String TEST_DATABASE = "http_auth_for_HttpAuthDatabaseTest.db";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        getContext().deleteDatabase(TEST_DATABASE);
    }

    @Override
    protected void tearDown() throws Exception {
        getContext().deleteDatabase(TEST_DATABASE);
        super.tearDown();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAccessHttpAuthUsernamePassword() throws Exception {
        HttpAuthDatabase instance = new HttpAuthDatabase(getContext(), TEST_DATABASE);

        String host = "http://localhost:8080";
        String realm = "testrealm";
        String userName = "user";
        String password = "password";

        String[] result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNull(result);

        instance.setHttpAuthUsernamePassword(host, realm, userName, password);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNotNull(result);
        assertEquals(userName, result[0]);
        assertEquals(password, result[1]);

        String newPassword = "newpassword";
        instance.setHttpAuthUsernamePassword(host, realm, userName, newPassword);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNotNull(result);
        assertEquals(userName, result[0]);
        assertEquals(newPassword, result[1]);

        String newUserName = "newuser";
        instance.setHttpAuthUsernamePassword(host, realm, newUserName, newPassword);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNotNull(result);
        assertEquals(newUserName, result[0]);
        assertEquals(newPassword, result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, null, password);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNotNull(result);
        assertNull(result[0]);
        assertEquals(password, result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, userName, null);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNotNull(result);
        assertEquals(userName, result[0]);
        assertEquals(null, result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, null, null);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNotNull(result);
        assertNull(result[0]);
        assertNull(result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, newUserName, newPassword);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        assertNotNull(result);
        assertEquals(newUserName, result[0]);
        assertEquals(newPassword, result[1]);
    }
}
