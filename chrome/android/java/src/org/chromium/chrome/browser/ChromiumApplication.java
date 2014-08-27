// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.content.app.ContentApplication;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 */
public abstract class ChromiumApplication extends ContentApplication {
    /**
     * Opens a protected content settings page, if available.
     */
    @CalledByNative
    protected abstract void openProtectedContentSettings();

    @CalledByNative
    protected abstract void showSyncSettings();

    @CalledByNative
    protected abstract void showAutofillSettings();

    @CalledByNative
    protected abstract void showTermsOfServiceDialog();

    /**
     * Opens the UI to clear browsing data.
     * @param tab The tab that triggered the request.
     */
    @CalledByNative
    protected abstract void openClearBrowsingData(Tab tab);

    /**
     * @return Whether parental controls are enabled.  Returning true will disable
     *         incognito mode.
     */
    @CalledByNative
    protected abstract boolean areParentalControlsEnabled();

    // TODO(yfriedman): This is too widely available. Plumb this through ChromeNetworkDelegate
    // instead.
    protected abstract PKCS11AuthenticationManager getPKCS11AuthenticationManager();
}
