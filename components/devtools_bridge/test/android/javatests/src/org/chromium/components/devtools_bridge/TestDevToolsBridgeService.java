// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import org.chromium.components.devtools_bridge.ui.ServiceUIFactory;

/**
 * Service for manual testing DevToolsBridgeServer with GCD signaling.
 */
public final class TestDevToolsBridgeService extends DevToolsBridgeServiceBase {
    private static final String SOCKET_NAME = "chrome_shell_devtools_remote";

    /**
     * Redirects intents to the TestDevToolsBridgeService.
     */
    public static final class Receiver extends ReceiverBase {
        public Receiver() {
            super(TestDevToolsBridgeService.class);
        }
    }

    @Override
    protected String socketName() {
        return SOCKET_NAME;
    }

    @Override
    protected ServiceUIFactory newUIFactory() {
        return new UIFactoryImpl();
    }

    private static class UIFactoryImpl extends ServiceUIFactory {
        protected String productName() {
            return "GCDHostService";
        }

        protected int notificationSmallIcon() {
            return android.R.drawable.alert_dark_frame;
        }
    }
}
