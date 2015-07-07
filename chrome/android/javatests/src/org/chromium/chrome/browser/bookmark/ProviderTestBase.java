// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmark;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.test.IsolatedContext;
import android.test.mock.MockContentResolver;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeBrowserProvider;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Base class for Chrome's ContentProvider tests.
 * Sets up a local ChromeBrowserProvider associated to a mock resolver in an isolated context.
 */
public class ProviderTestBase extends ChromeActivityTestCaseBase<ChromeActivity> {

    private IsolatedContext mContext;

    public ProviderTestBase() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        ChromeActivity activity = getActivity();
        assertNotNull(activity);

        ContentProvider provider = new ChromeBrowserProvider();
        provider.attachInfo(activity, null);

        MockContentResolver resolver = new MockContentResolver();
        resolver.addProvider(ChromeBrowserProvider.getApiAuthority(activity), provider);
        resolver.addProvider(ChromeBrowserProvider.getInternalAuthority(activity), provider);

        mContext = new IsolatedContext(resolver, activity);
        assertTrue(getContentResolver() instanceof MockContentResolver);
    }

    protected ContentResolver getContentResolver() {
        return mContext.getContentResolver();
    }
}
