// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import android.os.Handler;
import android.os.HandlerThread;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.framehost.RenderFrameHostImpl;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.media.mojom.AndroidOverlayClient;
import org.chromium.media.mojom.AndroidOverlayConfig;
import org.chromium.mojo.common.mojom.UnguessableToken;
import org.chromium.mojo.system.MojoException;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Base class for tests for DialogOverlayImpl.
 */
public abstract class DialogOverlayImplTestBase extends ContentShellTestBase {
    // overlay-ui thread.
    private HandlerThread mOverlayUiThread;
    private Handler mOverlayUiHandler;

    // Runnable that will be called on the browser UI thread when an overlay is released.
    private Runnable mReleasedRunnable;

    // The routing token that we'll use to create overlays.  This may be modified by the tests prior
    // to calling createOverlay().
    protected UnguessableToken mRoutingToken;

    // True if we should create a secure overlay.
    protected boolean mSecure;

    /**
     * AndroidOverlay client that supports waiting operations for callbacks.  One may call
     * nextEvent() to get the next callback, waiting if needed.
     */
    public static class Client implements AndroidOverlayClient {
        // AndroidOverlayClient
        public static final int SURFACE_READY = 0;
        public static final int DESTROYED = 1;
        public static final int CLOSE = 2;
        public static final int CONNECTION_ERROR = 2;
        // AndroidOverlayProviderImpl.Callbacks
        public static final int RELEASED = 100;
        // Internal to test only.
        public static final int TEST_MARKER = 200;

        /**
         * Records one callback event.
         */
        public static class Event {
            public Event(int which) {
                this.which = which;
            }

            public Event(int which, long surfaceKey) {
                this.which = which;
                this.surfaceKey = surfaceKey;
            }

            public Event(int which, MojoException exception) {
                this.which = which;
            }

            public int which;
            public long surfaceKey;
        }

        private ArrayBlockingQueue<Event> mPending;

        public Client() {
            mPending = new ArrayBlockingQueue<Event>(10);
        }

        @Override
        public void onSurfaceReady(long surfaceKey) {
            mPending.add(new Event(SURFACE_READY, surfaceKey));
        }

        @Override
        public void onDestroyed() {
            mPending.add(new Event(DESTROYED));
        }

        @Override
        public void close() {
            mPending.add(new Event(CLOSE));
        }

        @Override
        public void onConnectionError(MojoException exception) {
            mPending.add(new Event(CONNECTION_ERROR, exception));
        }

        // This isn't part of the overlay client.  It's called by the overlay to indicate that it
        // has been released by the client, but it's routed to us anyway.  It's on the Browser UI
        // thread, and it's convenient for us to keep track of it here.
        public void notifyReleased() {
            mPending.add(new Event(RELEASED));
        }

        // Inject a marker event, so that the test can checkpoint things.
        public void injectMarkerEvent() {
            mPending.add(new Event(TEST_MARKER));
        }

        // Wait for something to happen.  We enforce a timeout, since the test harness doesn't
        // always seem to fail the test when it times out.  Plus, it takes ~minute for that, but
        // none of our messages should take that long.
        Event nextEvent() {
            try {
                return mPending.poll(500, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                Assert.fail(e.toString());
            }

            // NOTREACHED
            return null;
        }

        boolean isEmpty() {
            return mPending.size() == 0;
        }
    }

    private Client mClient = new Client();

    // Return the URL to start with.
    protected abstract String getInitialUrl();

    protected Client getClient() {
        return mClient;
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(getInitialUrl());
        waitForActiveShellToBeDoneLoading(); // Do we need this?

        // Fetch the routing token.
        mRoutingToken =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<UnguessableToken>() {
                    @Override
                    public UnguessableToken call() {
                        RenderFrameHostImpl host =
                                (RenderFrameHostImpl) getWebContents().getMainFrame();
                        org.chromium.base.UnguessableToken routingToken =
                                host.getAndroidOverlayRoutingToken();
                        UnguessableToken mojoToken = new UnguessableToken();
                        mojoToken.high = routingToken.getHighForSerialization();
                        mojoToken.low = routingToken.getLowForSerialization();
                        return mojoToken;
                    }
                });

        // Set up the overlay UI thread
        mOverlayUiThread = new HandlerThread("TestOverlayUI");
        mOverlayUiThread.start();
        mOverlayUiHandler = new Handler(mOverlayUiThread.getLooper());

        // Just delegate to |mClient| when an overlay is released.
        mReleasedRunnable = new Runnable() {
            @Override
            public void run() {
                mClient.notifyReleased();
            }
        };
    }

    // Create an overlay with the given parameters and return it.
    DialogOverlayImpl createOverlay(final int x, final int y, final int width, final int height) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<DialogOverlayImpl>() {
            @Override
            public DialogOverlayImpl call() {
                AndroidOverlayConfig config = new AndroidOverlayConfig();
                config.routingToken = mRoutingToken;
                config.rect = new org.chromium.gfx.mojom.Rect();
                config.rect.x = x;
                config.rect.y = y;
                config.rect.width = width;
                config.rect.height = height;
                config.secure = mSecure;
                DialogOverlayImpl impl = new DialogOverlayImpl(
                        mClient, config, mOverlayUiHandler, mReleasedRunnable, true /* asPanel */);

                return impl;
            }
        });
    }
}
