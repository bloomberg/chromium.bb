// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.atomic.AtomicInteger;

public class CleanupReferenceTest extends InstrumentationTestCase {

    private static AtomicInteger sObjectCount = new AtomicInteger();

    private static class ReferredObject {

        private CleanupReference mRef;

        // Remember: this MUST be a static class, to avoid an implicit ref back to the
        // owning ReferredObject instance which would defeat GC of that object.
        private static class DestroyRunnable implements Runnable {
            @Override
            public void run() {
                sObjectCount.decrementAndGet();
            }
        };

        public ReferredObject() {
            sObjectCount.incrementAndGet();
            mRef = new CleanupReference(this, new DestroyRunnable());
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        sObjectCount.set(0);
    }

    private void collectGarbage() {
        // While this is only a 'hint' to the VM, it's generally effective and sufficient on
        // dalvik. If this changes in future, maybe try allocating a few gargantuan objects
        // too, to force the GC to work.
        System.gc();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateSingle() throws Throwable {
        assertEquals(0, sObjectCount.get());

        ReferredObject instance = new ReferredObject();
        assertEquals(1, sObjectCount.get());

        instance = null;
        collectGarbage();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return sObjectCount.get() == 0;
            }
        }));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateMany() throws Throwable {
        assertEquals(0, sObjectCount.get());

        final int INSTANCE_COUNT = 20;
        ReferredObject[] instances = new ReferredObject[INSTANCE_COUNT];

        for (int i = 0; i < INSTANCE_COUNT; ++i) {
            instances[i] = new ReferredObject();
            assertEquals(i + 1, sObjectCount.get());
        }

        instances = null;
        collectGarbage();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return sObjectCount.get() == 0;
            }
        }));
    }

}
