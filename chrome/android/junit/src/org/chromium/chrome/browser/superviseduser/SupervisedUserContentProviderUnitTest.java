// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.superviseduser;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.chromium.components.webrestrictions.WebRestrictionsContentProvider.WebRestrictionsResult;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

/**
 * Tests of SupervisedUserContentProvider. This is tested as a simple class, not as a content
 * provider. The content provider aspects are tested with WebRestrictionsContentProviderTest.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SupervisedUserContentProviderUnitTest {

    private SupervisedUserContentProvider mSupervisedUserContentProvider;

    @Before
    public void setUp() {
        mSupervisedUserContentProvider = Mockito.spy(new SupervisedUserContentProvider());
        mSupervisedUserContentProvider.setNativeSupervisedUserContentProviderForTesting(1234L);
    }

    @Test
    public void testShouldProceed() throws InterruptedException {
        // Mock the native call for a permitted URL
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                ((SupervisedUserContentProvider.SupervisedUserQueryReply) args[1])
                        .onQueryComplete();
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeShouldProceed(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserQueryReply.class),
                        anyString());
        WebRestrictionsResult result = mSupervisedUserContentProvider.shouldProceed("url");
        assertThat(result.shouldProceed(), is(true));
        verify(mSupervisedUserContentProvider)
                .nativeShouldProceed(eq(1234L),
                        any(SupervisedUserContentProvider.SupervisedUserQueryReply.class),
                        eq("url"));
        // Mock the native call for a forbidden URL
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                ((SupervisedUserContentProvider.SupervisedUserQueryReply) args[1])
                        .onQueryFailed(1, 2, 3, "url1", "url2", "custodian", "custodianEmail",
                                "secondCustodian", "secondCustodianEmail");
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeShouldProceed(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserQueryReply.class),
                        anyString());
        result = mSupervisedUserContentProvider.shouldProceed("url");
        assertThat(result.shouldProceed(), is(false));
        assertThat(result.errorIntCount(), is(3));
        assertThat(result.getErrorInt(0), is(1));
        assertThat(result.getErrorInt(1), is(2));
        assertThat(result.getErrorInt(2), is(3));
        assertThat(result.errorStringCount(), is(6));
        assertThat(result.getErrorString(0), is("url1"));
        assertThat(result.getErrorString(1), is("url2"));
        assertThat(result.getErrorString(2), is("custodian"));
        assertThat(result.getErrorString(3), is("custodianEmail"));
        assertThat(result.getErrorString(4), is("secondCustodian"));
        assertThat(result.getErrorString(5), is("secondCustodianEmail"));
    }

    @Test
    public void testRequestInsert() throws InterruptedException {
        // Mock native call.
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                ((SupervisedUserContentProvider.SupervisedUserInsertReply) args[1])
                        .onInsertRequestSendComplete(true);
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeRequestInsert(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        anyString());
        assertThat(mSupervisedUserContentProvider.requestInsert("url"), is(true));
        verify(mSupervisedUserContentProvider)
                .nativeRequestInsert(eq(1234L),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        eq("url"));
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                ((SupervisedUserContentProvider.SupervisedUserInsertReply) args[1])
                        .onInsertRequestSendComplete(false);
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeRequestInsert(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        anyString());
        assertThat(mSupervisedUserContentProvider.requestInsert("url"), is(false));
        verify(mSupervisedUserContentProvider, times(2))
                .nativeRequestInsert(eq(1234L),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        eq("url"));
    }
}
