// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;

/** Tests for SendTabToSelfAndroidBridge */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SendTabToSelfShareActivityTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock
    private Tab mTab;
    @Mock
    private ChromeActivity mChromeActivity;
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private Resources mResources;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;
    @Mock
    private NavigationHistory mNavigationHistory;
    @Mock
    private NavigationEntry mNavigationEntry;

    private Profile mProfile;

    private static final String URL = "http://www.tanyastacos.com";
    private static final String TITLE = "Come try Tanya's famous tacos";
    private static final long TIMESTAMP = 123456;
    // TODO(crbug/946808) Add actual target device ID.
    private static final String TARGET_DEVICE_SYNC_CACHE_GUID = "";

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    public void testIsFeatureAvailable() {
        boolean expected = true;
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mNativeMock.isFeatureAvailable(eq(mWebContents))).thenReturn(expected);

        boolean actual = SendTabToSelfShareActivity.featureIsAvailable(mTab);
        Assert.assertEquals(expected, actual);
    }

    @Test
    @SmallTest
    public void testHandleShareAction() {
        // Setup the mocked object chain to get to the profile.
        when(mChromeActivity.getActivityTabProvider()).thenReturn(mActivityTabProvider);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);

        // Setup the mocked object chain to get to the url, title and timestamp.
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getNavigationHistory()).thenReturn(mNavigationHistory);
        when(mNavigationHistory.getCurrentEntryIndex()).thenReturn(1);
        when(mNavigationHistory.getEntryAtIndex(anyInt())).thenReturn(mNavigationEntry);
        when(mNavigationEntry.getUrl()).thenReturn(URL);
        when(mNavigationEntry.getTitle()).thenReturn(TITLE);
        when(mNavigationEntry.getTimestamp()).thenReturn(TIMESTAMP);

        // Setup the mocked object chain to get the string needed by the Toast.
        when(mChromeActivity.getResources()).thenReturn(mResources);
        when(mResources.getText(anyInt())).thenReturn("ToastText");

        SendTabToSelfShareActivity shareActivity = new SendTabToSelfShareActivity();
        shareActivity.handleShareAction(mChromeActivity);
        verify(mNativeMock)
                .addEntry(eq(mProfile), eq(URL), eq(TITLE), eq(TIMESTAMP),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID));
    }
}
