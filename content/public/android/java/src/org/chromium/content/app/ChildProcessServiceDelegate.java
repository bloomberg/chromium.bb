// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

/**
 * The interface that embedders should implement to specialize child service creation.
 */
public interface ChildProcessServiceDelegate {
    /** Invoked when the service was created. This is the first method invoked on the delegate. */
    void onServiceCreated();

    /**
     * Called when the service is bound. Invoked on a background thread.
     * @param intent the intent that started the service.
     */
    void onServiceBound(Intent intent);

    /**
     * Called once the connection has been setup. Invoked on a background thread.
     * @param connectionBundle the bundle pass to the setupConnection call
     * @param callback the IBinder provided by the client
     */
    void onConnectionSetup(Bundle connectionBundle, IBinder callback);

    /** Called when the service gets destroyed. */
    void onDestroy();

    /**
     * Called when the delegate should load the native library.
     * @param hostContext The host context the library should be loaded with (i.e. Chrome).
     * @return true if the library was loaded successfully, false otherwise in which case the
     * service stops.
     */
    boolean loadNativeLibrary(Context hostContext);

    /** Called before the main method is invoked. */
    void onBeforeMain();

    /**
     * The main entry point for the service. This method should block as long as the service should
     * be running.
     */
    void runMain();
}
