// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwProxyController;
import org.chromium.support_lib_boundary.ProxyControllerBoundaryInterface;

import java.util.concurrent.Executor;

/**
 * Adapter between AwProxyController and ProxyControllerBoundaryInterface.
 */
public class SupportLibProxyControllerAdapter implements ProxyControllerBoundaryInterface {
    private final AwProxyController mProxyController;

    public SupportLibProxyControllerAdapter(AwProxyController proxyController) {
        mProxyController = proxyController;
    }

    @Override
    public void setProxyOverride(
            String[][] proxyRules, String[] bypassRules, Runnable listener, Executor executor) {
        mProxyController.setProxyOverride(proxyRules, bypassRules, listener, executor);
    }

    @Override
    public void clearProxyOverride(Runnable listener, Executor executor) {
        mProxyController.clearProxyOverride(listener, executor);
    }
}