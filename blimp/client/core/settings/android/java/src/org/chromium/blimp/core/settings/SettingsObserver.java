// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

/**
 * SettingsObserver is the Java representation of a native SettingsObserver object.
 *
 * The SettingsObserver is an observer API implemented by classes which are interested in
 * various events related to {@link Settings}.
 */
public interface SettingsObserver {
    /**
     * Invoked when enabling or disabling Blimp mode in the Settings.
     */
    void onBlimpModeEnabled(boolean enable);

    /**
     * Invoked when enabling or disabling to show the network stats in the Settings.
     */
    void onShowNetworkStatsChanged(boolean enable);

    /**
     * Invoked when enabling or disabling to download the whole document in the Settings.
     */
    void onRecordWholeDocumentChanged(boolean enable);

    /**
     * Invoked when a setting changed that requires the application to restart before it can take
     * effect.
     */
    void onRestartRequired();
}
