// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

/** Empty implementation for tests. */
public class EmptyEnabledStateMonitor implements EnabledStateMonitor {
    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}

    @Override
    public boolean getSettingsEnabled() {
        return false;
    }

    @Override
    public boolean getEnabledState() {
        return false;
    }

    @Override
    public void onAccessibilityModeChanged() {}
}
