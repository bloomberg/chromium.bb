// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.Intent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for the "Save Passwords" settings screen.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordReauthenticationFragmentTest {
    /**
     * Creates a dummy fragment, pushes the reauth fragment on top of it, then resolves the activity
     * for the reauth fragment and checks that back stack is in a correct state.
     * @param resultCode The code which is passed to the reauth fragment as the result of the
     *                   activity.
     */
    private void checkPopFromBackStackOnResult(int resultCode) {
        PasswordReauthenticationFragment passwordReauthentication =
                new PasswordReauthenticationFragment();

        // Replacement fragment for PasswordEntryEditor, which is the fragment that
        // replaces PasswordReauthentication after popBackStack is called.
        Fragment mockPasswordEntryEditor = new Fragment();

        Activity testActivity = Robolectric.setupActivity(Activity.class);
        Intent returnIntent = new Intent();
        returnIntent.putExtra("result", "This is the result");
        PasswordReauthenticationFragment.preventLockingForTesting();

        FragmentManager fragmentManager = testActivity.getFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        fragmentTransaction.add(mockPasswordEntryEditor, "password_entry_editor");
        fragmentTransaction.addToBackStack("add_password_entry_editor");
        fragmentTransaction.commit();

        FragmentTransaction fragmentTransaction2 = fragmentManager.beginTransaction();
        fragmentTransaction2.add(passwordReauthentication, "password_reauthentication");
        fragmentTransaction2.addToBackStack("add_password_reauthentication");
        fragmentTransaction2.commit();

        passwordReauthentication.onActivityResult(
                PasswordReauthenticationFragment.CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE, resultCode,
                returnIntent);
        fragmentManager.executePendingTransactions();

        // Assert that the number of fragments in the Back Stack is equal to 1 after
        // reauthentication, as PasswordReauthenticationFragment is popped.
        assertEquals(1, fragmentManager.getBackStackEntryCount());

        // Assert that the remaining fragment in the Back Stack is PasswordEntryEditor.
        assertEquals("add_password_entry_editor", fragmentManager.getBackStackEntryAt(0).getName());
    }

    /**
     * Ensure that upon successful reauthentication PasswordReauthenticationFragment is popped from
     * the FragmentManager backstack.
     */
    @Test
    public void testOnOkActivityResult() {
        checkPopFromBackStackOnResult(Activity.RESULT_OK);
    }

    /**
     * Ensure that upon canceled reauthentication PasswordReauthenticationFragment is popped from
     * the FragmentManager backstack.
     */
    @Test
    public void testOnCanceledActivityResult() {
        checkPopFromBackStackOnResult(Activity.RESULT_CANCELED);
    }
}
