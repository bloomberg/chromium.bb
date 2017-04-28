// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browseractions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.customtabs.browseractions.BrowserActionsIntent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for BrowserActionActivity.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BrowserActionActivityTest {
    private static final String HTTP_SCHEME_TEST_URL = "http://www.example.com";
    private static final String HTTPS_SCHEME_TEST_URL = "https://www.example.com";
    private static final String CHROME_SCHEME_TEST_URL = "chrome://example";
    private static final String CONTENT_SCHEME_TEST_URL = "content://example";

    private BrowserActionActivity mActivity = new BrowserActionActivity();
    private Context mContext;

    @Mock
    private PendingIntent mPendingIntent;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        Answer<String> answer = new Answer<String>() {
            @Override
            public String answer(InvocationOnMock invocation) {
                return "some.other.app.package.name";
            }
        };
        doAnswer(answer).when(mPendingIntent).getCreatorPackage();
    }

    @Test
    @Feature({"BrowserActions"})
    public void testStartedUpCorrectly() {
        assertFalse(mActivity.isStartedUpCorrectly(null));
        assertFalse(mActivity.isStartedUpCorrectly(new Intent()));

        Intent mIntent = createBaseBrowserActionsIntent(HTTP_SCHEME_TEST_URL);
        assertTrue(mActivity.isStartedUpCorrectly(mIntent));

        mIntent = createBaseBrowserActionsIntent(HTTP_SCHEME_TEST_URL);
        mIntent.removeExtra(BrowserActionsIntent.EXTRA_APP_ID);
        assertFalse(mActivity.isStartedUpCorrectly(mIntent));

        mIntent = createBaseBrowserActionsIntent(HTTPS_SCHEME_TEST_URL);
        assertTrue(mActivity.isStartedUpCorrectly(mIntent));

        mIntent = createBaseBrowserActionsIntent(CHROME_SCHEME_TEST_URL);
        assertFalse(mActivity.isStartedUpCorrectly(mIntent));

        mIntent = createBaseBrowserActionsIntent(CONTENT_SCHEME_TEST_URL);
        assertFalse(mActivity.isStartedUpCorrectly(mIntent));

        mIntent = createBaseBrowserActionsIntent(HTTP_SCHEME_TEST_URL);
        mIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertFalse(mActivity.isStartedUpCorrectly(mIntent));

        mIntent = createBaseBrowserActionsIntent(HTTP_SCHEME_TEST_URL);
        mIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        assertFalse(mActivity.isStartedUpCorrectly(mIntent));
    }

    /**
     * Creates a simple Intent for Browser Actions which contains a url, the {@link
     * BrowserActionsIntent.ACTION_BROWSER_ACTIONS_OPEN} action and source package name.
     * @param url The url for the data of the Intent.
     * @return The simple Intent for Browser Actions.
     */
    private Intent createBaseBrowserActionsIntent(String url) {
        return new BrowserActionsIntent.Builder(mContext, Uri.parse(url))
                .build()
                .getIntent()
                .putExtra(BrowserActionsIntent.EXTRA_APP_ID, mPendingIntent);
    }
}
