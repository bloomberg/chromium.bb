// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mojo;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.payments.PaymentRequestFactory;
import org.chromium.chrome.browser.shapedetection.BarcodeDetectionFactory;
import org.chromium.chrome.browser.shapedetection.TextDetectionFactory;
import org.chromium.chrome.browser.webshare.ShareServiceImplementationFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceRegistry;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.TextDetection;
import org.chromium.webshare.mojom.ShareService;

/** Registers mojo interface implementations exposed to C++ code at the Chrome layer. */
class ChromeInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addContextRegistrar(new ChromeContextInterfaceRegistrar());
        InterfaceRegistrar.Registry.addWebContentsRegistrar(
                new ChromeWebContentsInterfaceRegistrar());
    }
}

class ChromeContextInterfaceRegistrar implements InterfaceRegistrar<Context> {
    @Override
    public void registerInterfaces(InterfaceRegistry registry, final Context applicationContext) {
        registry.addInterface(
                BarcodeDetection.MANAGER, new BarcodeDetectionFactory(applicationContext));
        registry.addInterface(TextDetection.MANAGER, new TextDetectionFactory(applicationContext));
    }
}

class ChromeWebContentsInterfaceRegistrar implements InterfaceRegistrar<WebContents> {
    @Override
    public void registerInterfaces(InterfaceRegistry registry, final WebContents webContents) {
        registry.addInterface(PaymentRequest.MANAGER, new PaymentRequestFactory(webContents));
        registry.addInterface(
                ShareService.MANAGER, new ShareServiceImplementationFactory(webContents));
    }
}
