// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import android.os.Bundle;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for {@link PendingInvalidation}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
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
