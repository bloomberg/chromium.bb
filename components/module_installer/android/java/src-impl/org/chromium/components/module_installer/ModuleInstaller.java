// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import com.google.android.play.core.splitcompat.SplitCompat;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/** Installs dynamic feature modules (DFMs). */
public class ModuleInstaller {
    /** Command line switch for activating the fake backend.  */
    private static final String FAKE_FEATURE_MODULE_INSTALL = "fake-feature-module-install";
    private static final Map<String, List<OnModuleInstallFinishedListener>> sModuleNameListenerMap =
            new HashMap<>();
    private static ModuleInstallerBackend sBackend;

    /** Needs to be called before trying to access a module. */
    public static void init() {
        // SplitCompat.install may copy modules into Chrome's internal folder or clean them up.
        try (StrictModeContext unused = StrictModeContext.allowDiskWrites()) {
            SplitCompat.install(ContextUtils.getApplicationContext());
        }
    }

    /**
     * Requests the install of a module. The install will be performed asynchronously.
     *
     * @param moduleName Name of the module as defined in GN.
     * @param onFinishedListener Listener to be called once installation is finished.
     */
    public static void install(
            String moduleName, OnModuleInstallFinishedListener onFinishedListener) {
        ThreadUtils.assertOnUiThread();

        if (!sModuleNameListenerMap.containsKey(moduleName)) {
            sModuleNameListenerMap.put(moduleName, new LinkedList<>());
        }
        List<OnModuleInstallFinishedListener> onFinishedListeners =
                sModuleNameListenerMap.get(moduleName);
        onFinishedListeners.add(onFinishedListener);
        if (onFinishedListeners.size() > 1) {
            // Request is already running.
            return;
        }
        getBackend().install(moduleName);
    }

    private static void onFinished(boolean success, List<String> moduleNames) {
        ThreadUtils.assertOnUiThread();

        for (String moduleName : moduleNames) {
            List<OnModuleInstallFinishedListener> onFinishedListeners =
                    sModuleNameListenerMap.get(moduleName);
            if (onFinishedListeners == null) continue;

            for (OnModuleInstallFinishedListener listener : onFinishedListeners) {
                listener.onFinished(success);
            }
            sModuleNameListenerMap.remove(moduleName);
        }

        if (sModuleNameListenerMap.isEmpty()) {
            sBackend.close();
            sBackend = null;
        }
    }

    private static ModuleInstallerBackend getBackend() {
        if (sBackend == null) {
            ModuleInstallerBackend.OnFinishedListener listener = ModuleInstaller::onFinished;
            sBackend = CommandLine.getInstance().hasSwitch(FAKE_FEATURE_MODULE_INSTALL)
                    ? new FakeModuleInstallerBackend(listener)
                    : new PlayCoreModuleInstallerBackend(listener);
        }
        return sBackend;
    }

    private ModuleInstaller() {}
}
