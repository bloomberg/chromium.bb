// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blink.mojom.FaceDetection;
import org.chromium.content.browser.shapedetection.FaceDetectionFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.BatteryMonitor;
import org.chromium.device.VibrationManager;
import org.chromium.device.battery.BatteryMonitorFactory;
import org.chromium.device.nfc.mojom.Nfc;
import org.chromium.device.vibration.VibrationManagerImpl;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.services.service_manager.InterfaceRegistry;

@JNINamespace("content")
class InterfaceRegistrarImpl {
    @CalledByNative
    static void createInterfaceRegistryForContext(int nativeHandle, Context applicationContext) {
        ensureContentRegistrarsAreRegistered();

        InterfaceRegistry registry = InterfaceRegistry.create(
                CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle());
        InterfaceRegistrar.Registry.applyContextRegistrars(registry, applicationContext);
    }

    @CalledByNative
    static void createInterfaceRegistryForWebContents(int nativeHandle, WebContents webContents) {
        ensureContentRegistrarsAreRegistered();

        InterfaceRegistry registry = InterfaceRegistry.create(
                CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle());
        InterfaceRegistrar.Registry.applyWebContentsRegistrars(registry, webContents);
    }

    private static void ensureContentRegistrarsAreRegistered() {
        if (sHasRegisteredRegistrars) return;
        sHasRegisteredRegistrars = true;
        InterfaceRegistrar.Registry.addContextRegistrar(new ContentContextInterfaceRegistrar());
        InterfaceRegistrar.Registry.addWebContentsRegistrar(
                new ContentWebContentsInterfaceRegistrar());
    }

    private static boolean sHasRegisteredRegistrars = false;
}

class ContentContextInterfaceRegistrar implements InterfaceRegistrar<Context> {
    @Override
    public void registerInterfaces(InterfaceRegistry registry, final Context applicationContext) {
        registry.addInterface(
                VibrationManager.MANAGER, new VibrationManagerImpl.Factory(applicationContext));
        registry.addInterface(
                BatteryMonitor.MANAGER, new BatteryMonitorFactory(applicationContext));
        registry.addInterface(
                FaceDetection.MANAGER, new FaceDetectionFactory(applicationContext));
        // TODO(avayvod): Register the PresentationService implementation here.
    }
}

class ContentWebContentsInterfaceRegistrar implements InterfaceRegistrar<WebContents> {
    @Override
    public void registerInterfaces(InterfaceRegistry registry, final WebContents webContents) {
        registry.addInterface(Nfc.MANAGER, new NfcFactory(webContents));
    }
}
