// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Unit tests for {@link org.chromium.chrome.browser.ChromeBackupAgent}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeBackupAgentTest {

    static class ChromeTestBackupAgent extends ChromeBackupAgent {
        ChromeTestBackupAgent() {
            // This is protected in ContextWrapper, so can only be called within a derived
            // class.
            attachBaseContext(Robolectric.application);
        }
    }

    @Before
    public void setUp() throws Exception {
        ContextUtils.initApplicationContextForTests(Robolectric.application);
        AccountManager manager =
                (AccountManager) Robolectric.application.getSystemService(Context.ACCOUNT_SERVICE);
        manager.addAccountExplicitly(new Account("user1", "dummy"), null, null);
        manager.addAccountExplicitly(new Account("user2", "dummy"), null, null);
    }

    @Test
    public void testOnRestoreFinished() {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putBoolean("crash_dump_upload", false);
        editor.putString("google.services.username", "user1");
        editor.putString("junk", "junk");
        editor.commit();

        new ChromeTestBackupAgent().onRestoreFinished();

        // Check that we have only restored the correct preferences
        assertThat(sharedPrefs.getBoolean("crash_dump_upload", true), equalTo(false));
        assertThat(sharedPrefs.getString("google.services.username", null), nullValue());
        assertThat(sharedPrefs.getString("junk", null), nullValue());

        // Check that the preferences for which there is special code are correct
        assertThat(sharedPrefs.getString("first_run_signin_account_name", null), equalTo("user1"));
    }

    @Test
    public void testOnRestoreFinishedNoUser() {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putBoolean("crash_dump_upload", false);
        editor.putString("junk", "junk");
        editor.commit();

        new ChromeTestBackupAgent().onRestoreFinished();

        // Check that we haven't restored any preferences
        assertThat(sharedPrefs.getBoolean("crash_dump_upload", true), equalTo(true));
        assertThat(sharedPrefs.getString("google.services.username", null), nullValue());
        assertThat(sharedPrefs.getString("junk", null), nullValue());
        assertThat(sharedPrefs.getString("first_run_signin_account_name", null), nullValue());
    }

    @Test
    public void testOnRestoreFinishedWrongUser() {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putBoolean("crash_dump_upload", false);
        editor.putString("google.services.username", "wrong_user");
        editor.putString("junk", "junk");
        editor.commit();

        new ChromeTestBackupAgent().onRestoreFinished();

        // Check that we haven't restored any preferences
        assertThat(sharedPrefs.getBoolean("crash_dump_upload", true), equalTo(true));
        assertThat(sharedPrefs.getString("google.services.username", null), nullValue());
        assertThat(sharedPrefs.getString("junk", null), nullValue());
        assertThat(sharedPrefs.getString("first_run_signin_account_name", null), nullValue());
    }
}
