// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;

/**
 * The main controller for calling into the native snippets component to fetch snippets
 */
public class SnippetsController implements SignInStateObserver {
    private static SnippetsController sInstance;

    private long mNativeSnippetsController;

    public SnippetsController(Context applicationContext) {
        // |applicationContext| can be null in tests.
        if (applicationContext != null) {
            SigninManager.get(applicationContext).addSignInStateObserver(this);
        }
    }

    /**
     * Fetches new snippets
     *
     * @param overwrite true if an update to the current snippets should be forced, and false if
     * snippets should be downloaded only if there are no existing ones.
     */
    public void fetchSnippets(boolean overwrite) {
        nativeFetchSnippets(Profile.getLastUsedProfile(), overwrite);
    }

    /**
     * Retrieve the singleton instance of this class.
     *
     * @param context the current context.
     * @return the singleton instance.
     */
    public static SnippetsController get(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SnippetsController(context.getApplicationContext());
        }
        return sInstance;
    }

    @Override
    public void onSignedIn() {
        fetchSnippets(true);
    }

    @Override
    public void onSignedOut() {}

    @VisibleForTesting
    public static void setInstanceForTesting(SnippetsController instance) {
        sInstance = instance;
    }

    private native void nativeFetchSnippets(Profile profile, boolean overwrite);
}
