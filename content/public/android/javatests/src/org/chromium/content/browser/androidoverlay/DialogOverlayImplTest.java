// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.androidoverlay;

import android.support.test.filters.SmallTest;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;

/**
 * Tests for DialogOverlayImpl.
 * TODO(liberato): Convert to junit4.
 */
public class DialogOverlayImplTest extends DialogOverlayImplTestBase {
    private static final String BLANK_URL = "about://blank";

    @Override
    protected String getInitialUrl() {
        return BLANK_URL;
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateDestroyOverlay() {
        Assert.assertFalse(getClient().hasReceivedOverlayModeChange());
        Assert.assertFalse(getClient().isUsingOverlayMode());

        final DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);

        // We should get a new overlay with a valid surface key.
        Client.Event event = getClient().nextEvent();
        Assert.assertEquals(Client.SURFACE_READY, event.which);
        Assert.assertTrue(event.surfaceKey > 0);

        Assert.assertTrue(getClient().hasReceivedOverlayModeChange());
        Assert.assertTrue(getClient().isUsingOverlayMode());

        // Close the overlay, and make sure that the provider is notified.
        // Note that we should not get a 'destroyed' message when we close it.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                overlay.close();
            }
        });
        Assert.assertEquals(Client.RELEASED, getClient().nextEvent().which);
        Assert.assertFalse(getClient().isUsingOverlayMode());
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateOverlayFailsIfUnknownRoutingToken() {
        // Try to create an overlay with a bad routing token.
        mRoutingToken.high++;
        DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        // We should be notified that the overlay is destroyed.
        Client.Event event = getClient().nextEvent();
        Assert.assertEquals(Client.DESTROYED, event.which);
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testCreateOverlayFailsIfWebContentsHidden() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getWebContents().onHide();
            }
        });

        DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        // We should be notified that the overlay is destroyed.
        Client.Event event = getClient().nextEvent();
        Assert.assertEquals(Client.DESTROYED, event.which);
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testHiddingWebContentsDestroysOverlay() {
        DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);
        Assert.assertNotNull(overlay);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getWebContents().onHide();
            }
        });

        // We should be notified that the overlay is destroyed.
        Client.Event event = getClient().nextEvent();
        Assert.assertEquals(Client.DESTROYED, event.which);
    }

    @SmallTest
    @Feature({"AndroidOverlay"})
    public void testScheduleLayoutDoesntCrash() {
        // Make sure that we don't get any messages due to scheduleLayout, and we don't crash.
        final DialogOverlayImpl overlay = createOverlay(0, 0, 10, 10);

        // Wait for the surface.
        Assert.assertEquals(Client.SURFACE_READY, getClient().nextEvent().which);
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
        Assert.assertTrue(getClient().isEmpty());
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
        Client.Event event = getClient().nextEvent();
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
        Assert.assertEquals(Client.RELEASED, getClient().nextEvent().which);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                overlay.close();
                getClient().injectMarkerEvent();
            }
        });
        Assert.assertEquals(Client.TEST_MARKER, getClient().nextEvent().which);
    }
}
