// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ServiceRegistry.ImplementationFactory;
import org.chromium.device.battery.BatteryMonitorFactory;
import org.chromium.device.vibration.VibrationManagerImpl;
import org.chromium.mojom.device.BatteryMonitor;
import org.chromium.mojom.device.VibrationManager;

/**
 * Registers mojo services exposed by the browser in the given registry.
 */
@JNINamespace("content")
class ServiceRegistrar {
    // BatteryMonitorFactory can't implement ImplementationFactory itself, as we don't depend on
    // /content in /device. Hence we use BatteryMonitorImplementationFactory as a wrapper.
    private static class BatteryMonitorImplementationFactory
            implements ImplementationFactory<BatteryMonitor> {
        private final BatteryMonitorFactory mFactory;

        BatteryMonitorImplementationFactory(Context applicationContext) {
            mFactory = new BatteryMonitorFactory(applicationContext);
        }

        @Override
        public BatteryMonitor createImpl() {
            return mFactory.createMonitor();
        }
    }

    private static class VibrationManagerImplementationFactory
            implements ImplementationFactory<VibrationManager> {
        private final Context mApplicationContext;

        VibrationManagerImplementationFactory(Context applicationContext) {
            mApplicationContext = applicationContext;
        }

        @Override
        public VibrationManager createImpl() {
            return new VibrationManagerImpl(mApplicationContext);
        }
    }

    @CalledByNative
    static void registerProcessHostServices(ServiceRegistry registry, Context applicationContext) {
        assert applicationContext != null;
        registry.addService(BatteryMonitor.MANAGER,
                new BatteryMonitorImplementationFactory(applicationContext));
        registry.addService(VibrationManager.MANAGER,
                new VibrationManagerImplementationFactory(applicationContext));
    }

    @CalledByNative
    static void registerFrameHostServices(ServiceRegistry registry, Context applicationContext) {
        assert applicationContext != null;
        // TODO(avayvod): Register the PresentationService implementation here.
    }
}
