// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.AdvancedMockContext;

/**
 * Test of BrowserStartupController
 */
public class BrowserStartupControllerTest extends InstrumentationTestCase {

    private TestBrowserStartupController mController;

    private static class TestBrowserStartupController extends BrowserStartupController {

        private int mStartupResult;
        private boolean mLibraryLoadSucceeds;
        private int mInitializedCounter = 0;

        @Override
        void prepareToStartBrowserProcess(boolean singleProcess) throws ProcessInitException {
            if (!mLibraryLoadSucceeds) {
                throw new ProcessInitException(
                        LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED);
            }
        }

        private TestBrowserStartupController(Context context) {
            super(context);
        }

        @Override
        int contentStart() {
            mInitializedCounter++;
            if (BrowserStartupController.browserMayStartAsynchonously()) {
                // Post to the UI thread to emulate what would happen in a real scenario.
                ThreadUtils.postOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        BrowserStartupController.browserStartupComplete(mStartupResult);
                    }
                });
            } else {
                BrowserStartupController.browserStartupComplete(mStartupResult);
            }
            return mStartupResult;
        }

        private int initializedCounter() {
            return mInitializedCounter;
        }
    }

    private static class TestStartupCallback implements BrowserStartupController.StartupCallback {
        private boolean mWasSuccess;
        private boolean mWasFailure;
        private boolean mHasStartupResult;
        private boolean mAlreadyStarted;

        @Override
        public void onSuccess(boolean alreadyStarted) {
            assert !mHasStartupResult;
            mWasSuccess = true;
            mAlreadyStarted = alreadyStarted;
            mHasStartupResult = true;
        }

        @Override
        public void onFailure() {
            assert !mHasStartupResult;
            mWasFailure = true;
            mHasStartupResult = true;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mController = new TestBrowserStartupController(context);
        // Setting the static singleton instance field enables more correct testing, since it is
        // is possible to call {@link BrowserStartupController#browserStartupComplete(int)} instead
        // of {@link BrowserStartupController#executeEnqueuedCallbacks(int, boolean)} directly.
        BrowserStartupController.overrideInstanceForTest(mController);
    }

    @SmallTest
    public void testSingleAsynchronousStartupRequest() {
        mController.mStartupResult = BrowserStartupController.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the asynchronous startup request.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });

        assertTrue("Asynchronous mode should have been set.",
                BrowserStartupController.browserMayStartAsynchonously());
        assertEquals("The browser process should have been initialized one time.", 1,
                mController.initializedCounter());

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        assertTrue("Callback should have been a success.", callback.mWasSuccess);
        assertFalse("Callback should be told that the browser process was not already started.",
                callback.mAlreadyStarted);
    }

    @SmallTest
    public void testMultipleAsynchronousStartupRequests() {
        mController.mStartupResult = BrowserStartupController.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();
        final TestStartupCallback callback3 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback1);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback2);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mController.addStartupCompletedObserver(callback3);
            }
        });

        assertTrue("Asynchronous mode should have been set.",
                BrowserStartupController.browserMayStartAsynchonously());
        assertEquals("The browser process should have been initialized one time.", 1,
                mController.initializedCounter());

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);
        assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        assertTrue("Callback 3 should have been a success.", callback3.mWasSuccess);
        // Some startup tasks might have been enqueued after the browser process was started, but
        // not the first one which kicked of the startup.
        assertFalse("Callback 1 should be told that the browser process was not already started.",
                callback1.mAlreadyStarted);
    }

    @SmallTest
    public void testConsecutiveAsynchronousStartupRequests() {
        mController.mStartupResult = BrowserStartupController.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback1);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mController.addStartupCompletedObserver(callback2);
            }
        });

        assertTrue("Asynchronous mode should have been set.",
                BrowserStartupController.browserMayStartAsynchonously());
        assertEquals("The browser process should have been initialized one time.", 1,
                mController.initializedCounter());

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        assertTrue("Callback 1 should have been a success.", callback1.mWasSuccess);
        assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        assertTrue("Callback 2 should have been a success.", callback2.mWasSuccess);

        final TestStartupCallback callback3 = new TestStartupCallback();
        final TestStartupCallback callback4 = new TestStartupCallback();

        // Kick off more asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback3);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mController.addStartupCompletedObserver(callback4);
            }
        });

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        assertTrue("Callback 3 should have been a success.", callback3.mWasSuccess);
        assertTrue("Callback 3 should be told that the browser process was already started.",
                callback3.mAlreadyStarted);
        assertTrue("Callback 4 should have been executed.", callback4.mHasStartupResult);
        assertTrue("Callback 4 should have been a success.", callback4.mWasSuccess);
        assertTrue("Callback 4 should be told that the browser process was already started.",
                callback4.mAlreadyStarted);
    }

    @SmallTest
    public void testSingleFailedAsynchronousStartupRequest() {
        mController.mStartupResult = BrowserStartupController.STARTUP_FAILURE;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the asynchronous startup request.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });

        assertTrue("Asynchronous mode should have been set.",
                BrowserStartupController.browserMayStartAsynchonously());
        assertEquals("The browser process should have been initialized one time.", 1,
                mController.initializedCounter());

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        assertTrue("Callback should have been a failure.", callback.mWasFailure);
    }

    @SmallTest
    public void testConsecutiveFailedAsynchronousStartupRequests() {
        mController.mStartupResult = BrowserStartupController.STARTUP_FAILURE;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback1 = new TestStartupCallback();
        final TestStartupCallback callback2 = new TestStartupCallback();

        // Kick off the asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback1);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mController.addStartupCompletedObserver(callback2);
            }
        });

        assertTrue("Asynchronous mode should have been set.",
                BrowserStartupController.browserMayStartAsynchonously());
        assertEquals("The browser process should have been initialized one time.", 1,
                mController.initializedCounter());

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback 1 should have been executed.", callback1.mHasStartupResult);
        assertTrue("Callback 1 should have been a failure.", callback1.mWasFailure);
        assertTrue("Callback 2 should have been executed.", callback2.mHasStartupResult);
        assertTrue("Callback 2 should have been a failure.", callback2.mWasFailure);

        final TestStartupCallback callback3 = new TestStartupCallback();
        final TestStartupCallback callback4 = new TestStartupCallback();

        // Kick off more asynchronous startup requests.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback3);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mController.addStartupCompletedObserver(callback4);
            }
        });

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback 3 should have been executed.", callback3.mHasStartupResult);
        assertTrue("Callback 3 should have been a failure.", callback3.mWasFailure);
        assertTrue("Callback 4 should have been executed.", callback4.mHasStartupResult);
        assertTrue("Callback 4 should have been a failure.", callback4.mWasFailure);
    }

    @SmallTest
    public void testSingleSynchronousRequest() {
        mController.mStartupResult = BrowserStartupController.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        // Kick off the synchronous startup.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesSync(false);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        assertFalse("Synchronous mode should have been set",
                BrowserStartupController.browserMayStartAsynchonously());

        assertEquals("The browser process should have been initialized one time.", 1,
                mController.initializedCounter());
    }

    @SmallTest
    public void testAsyncThenSyncRequests() {
        mController.mStartupResult = BrowserStartupController.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the startups.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
                // To ensure that the async startup doesn't complete too soon we have
                // to do both these in a since Runnable instance. This avoids the
                // unpredictable race that happens in real situations.
                try {
                    mController.startBrowserProcessesSync(false);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });
        assertFalse("Synchronous mode should have been set",
                BrowserStartupController.browserMayStartAsynchonously());

        assertEquals("The browser process should have been initialized twice.", 2,
                mController.initializedCounter());

        assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        assertTrue("Callback should have been a success.", callback.mWasSuccess);
        assertFalse("Callback should be told that the browser process was not already started.",
                callback.mAlreadyStarted);
    }

    @SmallTest
    public void testSyncThenAsyncRequests() {
        mController.mStartupResult = BrowserStartupController.STARTUP_SUCCESS;
        mController.mLibraryLoadSucceeds = true;
        final TestStartupCallback callback = new TestStartupCallback();

        // Do a synchronous startup first.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesSync(false);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });

        assertEquals("The browser process should have been initialized once.", 1,
                mController.initializedCounter());

        assertFalse("Synchronous mode should have been set",
                BrowserStartupController.browserMayStartAsynchonously());

        // Kick off the asynchronous startup request. This should just queue the callback.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback);
                } catch (Exception e) {
                    fail("Browser should have started successfully");
                }
            }
        });

        assertEquals("The browser process should not have been initialized a second time.", 1,
                mController.initializedCounter());

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();

        assertTrue("Callback should have been executed.", callback.mHasStartupResult);
        assertTrue("Callback should have been a success.", callback.mWasSuccess);
        assertTrue("Callback should be told that the browser process was already started.",
                callback.mAlreadyStarted);
    }

    @SmallTest
    public void testLibraryLoadFails() {
        mController.mLibraryLoadSucceeds = false;
        final TestStartupCallback callback = new TestStartupCallback();

        // Kick off the asynchronous startup request.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    mController.startBrowserProcessesAsync(callback);
                    fail("Browser should not have started successfully");
                } catch (Exception e) {
                    // Exception expected, ignore.
                }
            }
        });

        assertEquals("The browser process should not have been initialized.", 0,
                mController.initializedCounter());

        // Wait for callbacks to complete.
        getInstrumentation().waitForIdleSync();
    }

}
