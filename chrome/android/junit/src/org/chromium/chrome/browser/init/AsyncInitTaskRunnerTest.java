// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.util.concurrent.RoboExecutorService;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link AsyncInitTaskRunner}
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AsyncInitTaskRunnerTest {
    private static final int THREAD_WAIT_TIME_MS = 1000;

    private LibraryLoader mLoader;
    private AsyncInitTaskRunner mRunner;
    private CountDownLatch mLatch;

    private VariationsSeedFetcher mVariationsSeedFetcher;

    public AsyncInitTaskRunnerTest() throws ProcessInitException {
        mLoader = spy(LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER));
        doNothing().when(mLoader).ensureInitialized();
        doNothing().when(mLoader).asyncPrefetchLibrariesToMemory();
        LibraryLoader.setLibraryLoaderForTesting(mLoader);
        mVariationsSeedFetcher = mock(VariationsSeedFetcher.class);
        VariationsSeedFetcher.setVariationsSeedFetcherForTesting(mVariationsSeedFetcher);
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);

        mLatch = new CountDownLatch(1);
        mRunner = spy(new AsyncInitTaskRunner() {
            @Override
            protected void onSuccess() {
                mLatch.countDown();
            }
            @Override
            protected void onFailure() {
                mLatch.countDown();
            }
            @Override
            protected Executor getExecutor() {
                return new RoboExecutorService();
            }
        });
        // Allow test to run on all builds
        when(mRunner.shouldFetchVariationsSeedDuringFirstRun()).thenReturn(true);
    }

    @Test
    public void libraryLoaderOnlyTest()
            throws InterruptedException, ProcessInitException, ExecutionException {
        mRunner.startBackgroundTasks(false, false);

        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mLoader).ensureInitialized();
        verify(mLoader).asyncPrefetchLibrariesToMemory();
        verify(mRunner).onSuccess();
        verify(mVariationsSeedFetcher, never()).fetchSeed();
    }

    @Test
    public void libraryLoaderFailTest() throws InterruptedException, ProcessInitException {
        doThrow(new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED))
                .when(mLoader)
                .ensureInitialized();
        mRunner.startBackgroundTasks(false, false);

        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mRunner).onFailure();
        verify(mVariationsSeedFetcher, never()).fetchSeed();
    }

    @Test
    public void fetchVariationsTest() throws InterruptedException, ProcessInitException {
        mRunner.startBackgroundTasks(false, true);

        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mLoader).ensureInitialized();
        verify(mLoader).asyncPrefetchLibrariesToMemory();
        verify(mRunner).onSuccess();
        verify(mVariationsSeedFetcher).fetchSeed();
    }

    // TODO(aberent) Test for allocateChildConnection. Needs refactoring of ChildProcessLauncher to
    // make it mockable.
}
