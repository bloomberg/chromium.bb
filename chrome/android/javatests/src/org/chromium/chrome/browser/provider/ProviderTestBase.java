// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.pm.ProviderInfo;
import android.test.IsolatedContext;
import android.test.mock.MockContentResolver;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
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

        final ChromeActivity activity = getActivity();
        assertNotNull(activity);

        final ContentProvider provider = new ChromeBrowserProvider();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProviderInfo providerInfo = new ProviderInfo();
                providerInfo.authority = ChromeBrowserProvider.getApiAuthority(activity);
                provider.attachInfo(activity, providerInfo);
            }
        });

        MockContentResolver resolver = new MockContentResolver();
        resolver.addProvider(ChromeBrowserProvider.getApiAuthority(activity), provider);

        mContext = new IsolatedContext(resolver, activity);
        assertTrue(getContentResolver() instanceof MockContentResolver);
    }

    protected ContentResolver getContentResolver() {
        return mContext.getContentResolver();
    }
}
