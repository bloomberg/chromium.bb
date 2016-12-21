// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.android_webview.CleanupReference;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicInteger;

/** Test suite for {@link CleanupReference}. */
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

        @SuppressFBWarnings("URF_UNREAD_FIELD")
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

    @SuppressFBWarnings("DM_GC")
    private void collectGarbage() {
        // While this is only a 'hint' to the VM, it's generally effective and sufficient on
        // dalvik. If this changes in future, maybe try allocating a few gargantuan objects
        // too, to force the GC to work.
        Runtime.getRuntime().gc();
    }

    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateSingle() throws Throwable {
        assertEquals(0, sObjectCount.get());

        ReferredObject instance = new ReferredObject();
        assertEquals(1, sObjectCount.get());

        instance = null;
        // Ensure compiler / instrumentation does not strip out the assignment.
        assertTrue(instance == null);
        collectGarbage();
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return sObjectCount.get();
            }
        }));
    }

    @SuppressFBWarnings("UC_USELESS_OBJECT")
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateMany() throws Throwable {
        assertEquals(0, sObjectCount.get());

        final int instanceCount = 20;
        ReferredObject[] instances = new ReferredObject[instanceCount];

        for (int i = 0; i < instanceCount; ++i) {
            instances[i] = new ReferredObject();
            assertEquals(i + 1, sObjectCount.get());
        }

        instances = null;
        // Ensure compiler / instrumentation does not strip out the assignment.
        assertTrue(instances == null);
        // Calling sObjectCount.get() before collectGarbage() seems to be required for the objects
        // to be GC'ed only when building using GN.
        assertTrue(sObjectCount.get() != -1);
        collectGarbage();
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return sObjectCount.get();
            }
        }));
    }

}
