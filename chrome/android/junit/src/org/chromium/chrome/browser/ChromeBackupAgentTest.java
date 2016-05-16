// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
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

    @Test
    public void testOnRestoreFinished() {
        SharedPreferences sharedPrefs =
                PreferenceManager.getDefaultSharedPreferences(Robolectric.application);
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

}
