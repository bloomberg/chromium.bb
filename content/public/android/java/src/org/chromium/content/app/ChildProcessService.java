// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/**
 * This is the base class for child services; the [Sandboxed|Privileged]ProcessService0, 1.. etc
 * subclasses provide the concrete service entry points, to enable the browser to connect
 * to more than one distinct process (i.e. one process per service number, up to limit of N).
 * The embedding application must declare these service instances in the application section
 * of its AndroidManifest.xml, first with some meta-data describing the services:
 *     <meta-data android:name="org.chromium.content.browser.NUM_[SANDBOXED|PRIVILEGED]_SERVICES"
 *           android:value="N"/>
 *     <meta-data android:name="org.chromium.content.browser.[SANDBOXED|PRIVILEGED]_SERVICES_NAME"
 *           android:value="org.chromium.content.app.[Sandboxed|Privileged]ProcessService"/>
 * and then N entries of the form:
 *     <service android:name="org.chromium.content.app.[Sandboxed|Privileged]ProcessServiceX"
 *              android:process=":[sandboxed|privileged]_processX" />
 *
 * Subclasses must also provide a delegate in this class constructor. That delegate is responsible
 * for loading native libraries and running the main entry point of the service.
 */
public abstract class ChildProcessService extends Service {
    private final ChildProcessServiceImpl mChildProcessServiceImpl;

    protected ChildProcessService(ChildProcessServiceDelegate delegate) {
        mChildProcessServiceImpl = new ChildProcessServiceImpl(delegate);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mChildProcessServiceImpl.create(getApplicationContext(), getApplicationContext());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mChildProcessServiceImpl.destroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // We call stopSelf() to request that this service be stopped as soon as the client
        // unbinds. Otherwise the system may keep it around and available for a reconnect. The
        // child processes do not currently support reconnect; they must be initialized from
        // scratch every time.
        stopSelf();
        return mChildProcessServiceImpl.bind(intent, -1);
    }
}
