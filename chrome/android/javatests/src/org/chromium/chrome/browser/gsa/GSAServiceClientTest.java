// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.Log;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for GSAServiceClient. */
public class GSAServiceClientTest extends InstrumentationTestCase {
    private static final String TAG = "GSAServiceClientTest";

    @SmallTest
    public void testGetPssForService() throws Exception {
        Context context = getInstrumentation().getTargetContext();

        if (!GSAState.getInstance(context).isGsaAvailable()) {
            Log.w(TAG, "GSA is not available");
            return;
        }

        final AtomicReference<ComponentName> componentNameRef = new AtomicReference<>();
        final Semaphore semaphore = new Semaphore(0);
        Intent intent =
                new Intent(GSAServiceClient.GSA_SERVICE).setPackage(GSAState.SEARCH_INTENT_PACKAGE);

        ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                componentNameRef.set(name);
                semaphore.release();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };
        context.bindService(intent, connection, Context.BIND_AUTO_CREATE);
        try {
            assertTrue("Timeout", semaphore.tryAcquire(10, TimeUnit.SECONDS));

            int pss = GSAServiceClient.getPssForService(componentNameRef.get());
            assertTrue(pss != GSAServiceClient.INVALID_PSS);
            assertTrue(pss > 0);
        } finally {
            context.unbindService(connection);
        }
    }

    @SmallTest
    public void testGetPssForServiceServiceNotFound() throws Exception {
        ComponentName componentName = new ComponentName("unknown.package.name", "UnknownClass");
        int pss = GSAServiceClient.getPssForService(componentName);
        assertTrue(pss == GSAServiceClient.INVALID_PSS);
    }
}
