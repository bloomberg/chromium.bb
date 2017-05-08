// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import android.os.Handler;
import android.os.HandlerThread;
import android.support.test.filters.SmallTest;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
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
 * Tests for DialogOverlayImpl.
 */
public class DialogOverlayImplTest extends ContentShellTestBase {
    private static final String BLANK_URL = "about://blank";

    // The routing token that we'll use to create overlays.
    UnguessableToken mRoutingToken;

    // overlay-ui thread.
    HandlerThread mOverlayUiThread;
    Handler mOverlayUiHandler;

    // Runnable that will be called on the browser UI thread when an overlay is released.
    Runnable mReleasedRunnable;

    // True if we should create a secure overlay.
    boolean mSecure;

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

    @Override
    public void setUp() throws Exception {
        super.setUp();

        startActivityWithTestUrl(BLANK_URL);

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
                HandlerThread overlayUiThread = new HandlerThread("TestOverlayUI");
                overlayUiThread.start();
                DialogOverlayImpl impl = new DialogOverlayImpl(
                        mClient, config, mOverlayUiHandler, mReleasedRunnable);

                return impl;
            }
        });
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateDestroyOverlay() {
        final DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);

        // We should get a new overlay with a valid surface key.
        Client.Event event = mClient.nextEvent();
        Assert.assertEquals(Client.SURFACE_READY, event.which);
        Assert.assertTrue(event.surfaceKey > 0);

        // Close the overlay, and make sure that the provider is notified.
        // Note that we should not get a 'destroyed' message when we close it.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                overlay.close();
            }
        });
        Assert.assertEquals(Client.RELEASED, mClient.nextEvent().which);
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateOverlayFailsIfUnknownRoutingToken() {
        // Try to create an overlay with a bad routing token.
        mRoutingToken.high++;
        DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        // We should be notified that the overlay is destroyed.
        Client.Event event = mClient.nextEvent();
        Assert.assertEquals(Client.DESTROYED, event.which);
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testScheduleLayoutDoesntCrash() {
        // Make sure that we don't get any messages due to scheduleLayout, and we don't crash.
        final DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);

        // Wait for the surface.
        Assert.assertEquals(Client.SURFACE_READY, mClient.nextEvent().which);
        final org.chromium.gfx.mojom.Rect rect = new org.chromium.gfx.mojom.Rect();
        rect.x = 100;
        rect.y = 200;
        rect.width = 100;
        rect.height = 100;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                overlay.scheduleLayout(rect);
            }
        });

        // No additional messages should have arrived.
        Assert.assertTrue(mClient.isEmpty());
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateSecureSurface() {
        // Test that creating a secure overlay creates an overlay.  We can't really tell if it's
        // secure or not, until we can do a screen shot test.
        mSecure = true;
        final DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        // We should get a new overlay with a valid surface key.
        Client.Event event = mClient.nextEvent();
        Assert.assertEquals(Client.SURFACE_READY, event.which);
        Assert.assertTrue(event.surfaceKey > 0);
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCloseOnlyClosesOnce() {
        // Test that trying to close an overlay more than once doesn't actually do anything.
        final DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);
        // The first should generate RELEASED
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                overlay.close();
            }
        });
        Assert.assertEquals(Client.RELEASED, mClient.nextEvent().which);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                overlay.close();
                mClient.injectMarkerEvent();
            }
        });
        Assert.assertEquals(Client.TEST_MARKER, mClient.nextEvent().which);
    }
}
