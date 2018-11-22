// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.PersistentNotificationController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityDisclosureController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityOpenTimeRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityToolbarController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.PersistentNotificationView;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.TrustedWebActivityDisclosureView;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.TrustedWebActivityToolbarView;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

/**
 * Coordinator for the Trusted Web Activity component.
 * Add methods here if other components need to communicate with Trusted Web Activity component.
 */
@ActivityScope
public class TrustedWebActivityCoordinator {
    @Inject
    public TrustedWebActivityCoordinator(
            PersistentNotificationController persistentNotificationController,
            TrustedWebActivityDisclosureController disclosureController,
            TrustedWebActivityToolbarController toolbarController,
            TrustedWebActivityToolbarView toolbarView,
            TrustedWebActivityDisclosureView disclosureView,
            PersistentNotificationView notificationView,
            TrustedWebActivityOpenTimeRecorder openTimeRecorder) {
        // Do nothing for now, just resolve the classes that need to start working.
    }
}
