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

import android.os.Handler;
import android.os.Looper;
import android.util.Pair;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import java.util.concurrent.CountDownLatch;

/**
 * Tests of SupervisedUserContentProvider. This is tested as a simple class, not as a content
 * provider. The content provider aspects are tested with WebRestrictionsContentProviderTest.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SupervisedUserContentProviderUnitTest.ShadowHander.class})
public class SupervisedUserContentProviderUnitTest {
    private static CountDownLatch sLatch;

    /**
     * Special Handler shadow class that allows the test to wait for a post() call.
     */
    @Implements(Handler.class)
    public static class ShadowHander extends org.robolectric.shadows.ShadowHandler {
        @Override
        @Implementation
        public boolean post(Runnable r) {
            boolean result = super.post(r);
            // If the test wants to wait it should initialize sLatch before post is called.
            if (sLatch != null) sLatch.countDown();
            return result;
        }
    }

    private SupervisedUserContentProvider mSupervisedUserContentProvider;
    private Looper mNativeCallLooper;

    @Before
    public void setUp() {
        sLatch = null;
        mNativeCallLooper = null;
        mSupervisedUserContentProvider = Mockito.spy(new SupervisedUserContentProvider());
        mSupervisedUserContentProvider.setNativeSupervisedUserContentProviderForTesting(1234L);
    }

    @Test
    public void testShouldProceed() throws InterruptedException {
        // Runnable for test thread.
        class TestRunnable implements Runnable {
            private Pair<Boolean, String> mResult;

            @Override
            public void run() {
                this.mResult = mSupervisedUserContentProvider.shouldProceed("url");
            }

            public Pair<Boolean, String> getResult() {
                return mResult;
            }
        }
        // Mock the native call for a permitted URL
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                mNativeCallLooper = Looper.myLooper();
                ((SupervisedUserContentProvider.SupervisedUserQueryReply) args[1])
                        .onQueryComplete(true, null);
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeShouldProceed(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserQueryReply.class),
                        anyString());
        TestRunnable r1 = new TestRunnable();
        // Because SupervisedUserContentProvider uses a CountDownLatch (which is a java.util
        // class, so can't be shadowed or mocked) we need to make the calls to shouldProceed on a
        // real thread. This can't be the main thread because Robolectric emulates UI event handling
        // on the main thread.
        Thread t1 = new Thread(r1);
        sLatch = new CountDownLatch(1);
        t1.start();
        // Wait for the event to be posted to the emulated UI thread.
        sLatch.await();
        Robolectric.runUiThreadTasks();
        t1.join();
        verify(mSupervisedUserContentProvider)
                .nativeShouldProceed(eq(1234L),
                        any(SupervisedUserContentProvider.SupervisedUserQueryReply.class),
                        eq("url"));
        // Assert has to be on main thread for failures to cause test failure.
        assertThat(r1.getResult(), is(new Pair<Boolean, String>(true, null)));
        // Check that the native code was called on the right thread.
        assertThat(mNativeCallLooper, is(Looper.getMainLooper()));
        // Mock the native call for a forbidden URL
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                mNativeCallLooper = Looper.myLooper();
                ((SupervisedUserContentProvider.SupervisedUserQueryReply) args[1])
                        .onQueryComplete(false, "Hello");
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeShouldProceed(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserQueryReply.class),
                        anyString());
        TestRunnable r2 = new TestRunnable();
        // Create a new thread for the second call
        Thread t2 = new Thread(r2);
        sLatch = new CountDownLatch(1);
        t2.start();
        // Wait for the event to be posted to the emulated UI thread.
        sLatch.await();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        t2.join();
        assertThat(r2.getResult(), is(new Pair<Boolean, String>(false, "Hello")));
        // Check that the native code was called on the UI thread.
        assertThat(mNativeCallLooper, is(Looper.getMainLooper()));
    }

    @Test
    public void testRequestInsert() throws InterruptedException {
        // Runnable for test thread.
        class TestRunnable implements Runnable {
            private boolean mResult;

            @Override
            public void run() {
                this.mResult = mSupervisedUserContentProvider.requestInsert("url");
            }

            public boolean getResult() {
                return mResult;
            }
        }
        // Mock native call.
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                mNativeCallLooper = Looper.myLooper();
                ((SupervisedUserContentProvider.SupervisedUserInsertReply) args[1])
                        .onInsertRequestSendComplete(true);
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeRequestInsert(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        anyString());
        TestRunnable r1 = new TestRunnable();
        Thread t1 = new Thread(r1);
        sLatch = new CountDownLatch(1);
        t1.start();
        sLatch.await();
        Robolectric.runUiThreadTasks();
        t1.join();
        verify(mSupervisedUserContentProvider)
                .nativeRequestInsert(eq(1234L),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        eq("url"));
        assertThat(r1.getResult(), is(true));
        assertThat(mNativeCallLooper, is(Looper.getMainLooper()));
        doAnswer(new Answer<Void>() {

            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                Object args[] = invocation.getArguments();
                mNativeCallLooper = Looper.myLooper();
                ((SupervisedUserContentProvider.SupervisedUserInsertReply) args[1])
                        .onInsertRequestSendComplete(false);
                return null;
            }

        })
                .when(mSupervisedUserContentProvider)
                .nativeRequestInsert(anyLong(),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        anyString());
        TestRunnable r2 = new TestRunnable();
        Thread t2 = new Thread(r2);
        sLatch = new CountDownLatch(1);
        t2.start();
        sLatch.await();
        Robolectric.runUiThreadTasks();
        t2.join();
        Robolectric.runUiThreadTasks();
        verify(mSupervisedUserContentProvider, times(2))
                .nativeRequestInsert(eq(1234L),
                        any(SupervisedUserContentProvider.SupervisedUserInsertReply.class),
                        eq("url"));
        assertThat(r2.getResult(), is(false));
        assertThat(mNativeCallLooper, is(Looper.getMainLooper()));
    }
}
