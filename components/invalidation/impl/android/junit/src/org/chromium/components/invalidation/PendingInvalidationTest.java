// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import static org.junit.Assert.assertEquals;

import android.os.Bundle;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

/**
 * Tests for {@link PendingInvalidation}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PendingInvalidationTest {
    private static String sObjecId = "ObjectId";
    private static int sObjectSource = 4;
    private static long sVersion = 5;
    private static String sPayload = "Payload";

    @Test
    public void testParseFromBundle() {
        PendingInvalidation invalidation =
                new PendingInvalidation(sObjecId, sObjectSource, sVersion, sPayload);
        Bundle bundle =
                PendingInvalidation.createBundle(sObjecId, sObjectSource, sVersion, sPayload);
        PendingInvalidation parsedInvalidation = new PendingInvalidation(bundle);
        assertEquals(sObjecId, parsedInvalidation.mObjectId);
        assertEquals(sObjectSource, parsedInvalidation.mObjectSource);
        assertEquals(sVersion, parsedInvalidation.mVersion);
        assertEquals(sPayload, parsedInvalidation.mPayload);
        assertEquals(invalidation, parsedInvalidation);
    }

    @Test
    public void testParseToAndFromProtocolBuffer() {
        PendingInvalidation invalidation =
                new PendingInvalidation(sObjecId, sObjectSource, sVersion, sPayload);
        Bundle bundle = PendingInvalidation.decodeToBundle(invalidation.encodeToString());
        PendingInvalidation parsedInvalidation = new PendingInvalidation(bundle);
        assertEquals(sObjecId, parsedInvalidation.mObjectId);
        assertEquals(sObjectSource, parsedInvalidation.mObjectSource);
        assertEquals(sVersion, parsedInvalidation.mVersion);
        assertEquals(sPayload, parsedInvalidation.mPayload);
        assertEquals(invalidation, parsedInvalidation);
    }
}
