// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webrestrictions;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

import org.chromium.base.ContextUtils;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;

/**
 * Tests of WebRestrictionsClient.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebRestrictionsClientTest {
    private static final String TEST_CONTENT_PROVIDER = "example.com";
    private WebRestrictionsClient mTestClient;
    ContentProvider mProvider;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForJUnitTests(Robolectric.application);
        mTestClient = Mockito.spy(new WebRestrictionsClient());
        Mockito.doNothing().when(mTestClient).nativeNotifyWebRestrictionsChanged(anyLong());
        mProvider = Mockito.mock(ContentProvider.class);

        mTestClient.init(TEST_CONTENT_PROVIDER, 1234L);
    }

    @Test
    public void testSupportsRequest() {
        assertThat(mTestClient.supportsRequest(), is(false));

        ShadowContentResolver.registerProvider(TEST_CONTENT_PROVIDER, mProvider);

        when(mProvider.getType(any(Uri.class))).thenReturn(null);
        assertThat(mTestClient.supportsRequest(), is(false));

        when(mProvider.getType(any(Uri.class))).thenReturn("dummy");
        assertThat(mTestClient.supportsRequest(), is(true));
    }

    @Test
    public void testShouldProceed() {
        ShadowContentResolver.registerProvider(TEST_CONTENT_PROVIDER, mProvider);

        when(mProvider.query(any(Uri.class), any(String[].class), anyString(), any(String[].class),
                     anyString()))
                .thenReturn(null);
        WebRestrictionsClient.ShouldProceedResult result =
                mTestClient.shouldProceed("http://example.com");
        verify(mProvider).query(Uri.parse("content://" + TEST_CONTENT_PROVIDER + "/authorized"),
                null, "url = 'http://example.com'", null, null);
        assertThat(result.shouldProceed(), is(true));
        assertThat(result.getErrorPage(), is(nullValue()));

        Cursor cursor = Mockito.mock(Cursor.class);
        when(cursor.getInt(0)).thenReturn(1);
        when(cursor.getString(1)).thenReturn("No error");
        when(mProvider.query(any(Uri.class), any(String[].class), anyString(), any(String[].class),
                     anyString()))
                .thenReturn(cursor);
        result = mTestClient.shouldProceed("http://example.com");
        assertThat(result.shouldProceed(), is(true));
        assertThat(result.getErrorPage(), is(nullValue()));

        when(cursor.getInt(0)).thenReturn(0);
        when(cursor.getString(1)).thenReturn("Error");
        when(mProvider.query(any(Uri.class), any(String[].class), anyString(), any(String[].class),
                     anyString()))
                .thenReturn(cursor);
        result = mTestClient.shouldProceed("http://example.com");
        assertThat(result.shouldProceed(), is(false));
        assertThat(result.getErrorPage(), is("Error"));
    }

    @Test
    public void testRequestPermission() {
        ShadowContentResolver.registerProvider(TEST_CONTENT_PROVIDER, mProvider);

        ContentValues expectedValues = new ContentValues();
        expectedValues.put("url", "http://example.com");
        when(mProvider.insert(any(Uri.class), any(ContentValues.class))).thenReturn(null);
        assertThat(mTestClient.requestPermission("http://example.com"), is(false));
        verify(mProvider).insert(
                Uri.parse("content://" + TEST_CONTENT_PROVIDER + "/requested"), expectedValues);

        when(mProvider.insert(any(Uri.class), any(ContentValues.class)))
                .thenReturn(Uri.parse("content://example.com/xxx"));
        assertThat(mTestClient.requestPermission("http://example.com"), is(true));
    }

    @Test
    public void testNotifyChange() {
        ShadowContentResolver.registerProvider(TEST_CONTENT_PROVIDER, mProvider);

        ContentResolver resolver = Robolectric.application.getContentResolver();
        resolver.notifyChange(Uri.parse("content://" + TEST_CONTENT_PROVIDER), null);
        verify(mTestClient).nativeNotifyWebRestrictionsChanged(1234L);
    }
}
