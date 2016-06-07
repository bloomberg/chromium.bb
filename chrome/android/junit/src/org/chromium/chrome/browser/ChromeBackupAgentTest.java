// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.junit.matchers.JUnitMatchers.containsString;

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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;

/**
 * Unit tests for {@link org.chromium.chrome.browser.ChromeBackupAgent}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeBackupAgentTest {

    static class ChromeTestBackupAgent extends ChromeBackupAgent {

        private ByteArrayInputStream mInputStream;
        private ByteArrayOutputStream mOutputStream;

        ChromeTestBackupAgent(byte[] mChromeInputPrefs) {
            // This is protected in ContextWrapper, so can only be called within a derived
            // class.
            attachBaseContext(Robolectric.application);
            mInputStream = new ByteArrayInputStream(mChromeInputPrefs);
            mOutputStream = new ByteArrayOutputStream();
        }

        @Override
        protected InputStream openInputStream(File prefsFile) throws FileNotFoundException {
            return mInputStream;

        }

        @Override
        protected OutputStream openOutputStream(File prefsFile) throws FileNotFoundException {
            return mOutputStream;
        }

        @Override
        public File getDir(String name, int mode) {
            return null;
        }

        @Override
        protected long getFileLength(File prefsFile) {
            return mInputStream.available();
        }

        byte[] getOutputData() {
            return mOutputStream.toByteArray();
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
    public void testOnRestoreFinished() throws UnsupportedEncodingException {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putBoolean("crash_dump_upload", false);
        editor.putString("google.services.username", "user1");
        editor.putString("junk", "junk");
        editor.commit();

        String chromeInputPrefs =
                "{\"junk1\":\"abc\", "
                + "\"sync\":{ \"has_setup_completed\":\"true\", "
                + "       \"keep_everything_synced\":\"false\", "
                + "       \"junk2\":\"xxx\""
                + "     }}";
        byte[] chromePrefsBuffer = chromeInputPrefs.getBytes("UTF-8");
        ChromeTestBackupAgent chromeTestBackupAgent = new ChromeTestBackupAgent(chromePrefsBuffer);
        chromeTestBackupAgent.onRestoreFinished();

        String chromeOutputPrefs = new String(chromeTestBackupAgent.getOutputData(), "UTF-8");

        // Check that we have only restored the correct Chrome preferences
        assertThat(chromeOutputPrefs, containsString("\"has_setup_completed\":\"true\""));
        assertThat(chromeOutputPrefs, containsString("\"keep_everything_synced\":\"false\""));
        assertThat(chromeOutputPrefs, not(containsString("junk")));


        // Check that we have only restored the correct Android preferences
        assertThat(sharedPrefs.getBoolean("crash_dump_upload", true), equalTo(false));
        assertThat(sharedPrefs.getString("google.services.username", null), nullValue());
        assertThat(sharedPrefs.getString("junk", null), nullValue());

        // Check that the preferences for which there is special code are correct
        assertThat(sharedPrefs.getString("first_run_signin_account_name", null), equalTo("user1"));
    }

    @Test
    public void testOnRestoreFinishedNoUser() throws UnsupportedEncodingException {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putBoolean("crash_dump_upload", false);
        editor.putString("junk", "junk");
        editor.commit();

        String chromeInputPrefs =
                "{\"junk1\":\"abc\", "
                + "\"sync\":{ \"has_setup_completed\":\"true\", "
                + "       \"keep_everything_synced\":\"false\", "
                + "       \"junk2\":\"xxx\""
                + "     }}";
        byte[] chromePrefsBuffer = chromeInputPrefs.getBytes("UTF-8");
        ChromeTestBackupAgent chromeTestBackupAgent = new ChromeTestBackupAgent(chromePrefsBuffer);
        chromeTestBackupAgent.onRestoreFinished();

        // Check that we haven't restored any Chrome preferences
        String chromeOutputPrefs = new String(chromeTestBackupAgent.getOutputData(), "UTF-8");
        assertThat(chromeOutputPrefs, equalTo(""));

        // Check that we haven't restored any Android preferences
        assertThat(sharedPrefs.getBoolean("crash_dump_upload", true), equalTo(true));
        assertThat(sharedPrefs.getString("google.services.username", null), nullValue());
        assertThat(sharedPrefs.getString("junk", null), nullValue());
        assertThat(sharedPrefs.getString("first_run_signin_account_name", null), nullValue());
    }

    @Test
    public void testOnRestoreFinishedWrongUser() throws UnsupportedEncodingException {
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putBoolean("crash_dump_upload", false);
        editor.putString("google.services.username", "wrong_user");
        editor.putString("junk", "junk");
        editor.commit();

        String chromeInputPrefs =
                "{\"junk1\":\"abc\", "
                + "\"sync\":{ \"has_setup_completed\":\"true\", "
                + "       \"keep_everything_synced\":\"false\", "
                + "       \"junk2\":\"xxx\""
                + "     }}";
        byte[] chromePrefsBuffer = chromeInputPrefs.getBytes("UTF-8");
        ChromeTestBackupAgent chromeTestBackupAgent = new ChromeTestBackupAgent(chromePrefsBuffer);
        chromeTestBackupAgent.onRestoreFinished();

        // Check that we haven't restored any Chrome preferences
        String chromeOutputPrefs = new String(chromeTestBackupAgent.getOutputData(), "UTF-8");
        assertThat(chromeOutputPrefs, equalTo(""));

        // Check that we haven't restored any Android preferences
        assertThat(sharedPrefs.getBoolean("crash_dump_upload", true), equalTo(true));
        assertThat(sharedPrefs.getString("google.services.username", null), nullValue());
        assertThat(sharedPrefs.getString("junk", null), nullValue());
        assertThat(sharedPrefs.getString("first_run_signin_account_name", null), nullValue());
    }
}
