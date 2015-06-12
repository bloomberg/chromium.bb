// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.EmbedContentViewActivity;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.precache.PrecacheLauncher;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;

/**
 * The staging bits of the Chrome settings activity.
 *
 * TODO(newt): merge this with Preferences after upstreaming.
 */
public class PreferencesStaging extends Preferences {

    @Override
    protected void startBrowserProcessSync() throws ProcessInitException {
        ((ChromiumApplication) getApplication()).startBrowserProcessesAndLoadLibrariesSync(true);
    }

    @Override
    protected String getTopLevelFragmentName() {
        return MainPreferences.class.getName();
    }

    @Override
    public void showUrl(int titleResId, int urlResId) {
        EmbedContentViewActivity.show(this, titleResId, urlResId);
    }

    @Override
    public void logContextualSearchToggled(boolean newValue) {
        ContextualSearchUma.logPreferenceChange(newValue);
    }

    @Override
    public boolean isContextualSearchEnabled() {
        return ContextualSearchFieldTrial.isEnabled(this);
    }

    @Override
    public void updatePrecachingEnabled() {
        PrecacheLauncher.updatePrecachingEnabled(PrivacyPreferencesManager.getInstance(this), this);
    }

    @Override
    protected void onPause() {
        super.onPause();
        ChromiumApplication.flushPersistentData();
    }
}
