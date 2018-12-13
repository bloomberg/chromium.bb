// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.components.module_installer.ModuleInstaller;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/**
 * Instantiates the VR delegates. If the VR module is not available this provider will
 * instantiate a fallback implementation.
 */
@JNINamespace("vr")
public class VrModuleProvider {
    private static VrDelegateProvider sDelegateProvider;
    private static final List<VrModeObserver> sVrModeObservers = new ArrayList<>();

    private long mNativeVrModuleProvider;

    /**
     * Need to be called after native libraries are available. Has no effect if VR is not compiled
     * into Chrome.
     **/
    public static void maybeInit() {
        if (VrBuildConfig.IS_VR_ENABLED) {
            nativeInit();
        }
    }

    public static VrDelegate getDelegate() {
        return getDelegateProvider().getDelegate();
    }

    public static VrIntentDelegate getIntentDelegate() {
        return getDelegateProvider().getIntentDelegate();
    }

    /**
     * Registers the given {@link VrModeObserver}.
     *
     * @param observer The VrModeObserver to register.
     */
    public static void registerVrModeObserver(VrModeObserver observer) {
        sVrModeObservers.add(observer);
    }

    /**
     * Unregisters the given {@link VrModeObserver}.
     *
     * @param observer The VrModeObserver to remove.
     */
    public static void unregisterVrModeObserver(VrModeObserver observer) {
        sVrModeObservers.remove(observer);
    }

    public static void onEnterVr() {
        for (VrModeObserver observer : sVrModeObservers) observer.onEnterVr();
    }

    public static void onExitVr() {
        for (VrModeObserver observer : sVrModeObservers) observer.onExitVr();
    }

    // TODO(crbug.com/870055): JNI should be registered in the shared VR library's JNI_OnLoad
    // function. Do this once we have a shared VR library.
    /* package */ static void registerJni() {
        nativeRegisterJni();
    }

    private static VrDelegateProvider getDelegateProvider() {
        if (sDelegateProvider == null) {
            // Need to be called before trying to access the VR module.
            ModuleInstaller.init();
            try {
                sDelegateProvider =
                        (VrDelegateProvider) Class
                                .forName("org.chromium.chrome.browser.vr.VrDelegateProviderImpl")
                                .newInstance();
            } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                    | IllegalArgumentException e) {
                sDelegateProvider = new VrDelegateProviderFallback();
            }
        }
        return sDelegateProvider;
    }

    @CalledByNative
    private static VrModuleProvider create(long nativeVrModuleProvider) {
        return new VrModuleProvider(nativeVrModuleProvider);
    }

    @CalledByNative
    private static boolean isModuleInstalled() {
        return !(getDelegateProvider() instanceof VrDelegateProviderFallback);
    }

    private VrModuleProvider(long nativeVrModuleProvider) {
        mNativeVrModuleProvider = nativeVrModuleProvider;
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeVrModuleProvider = 0;
    }

    @CalledByNative
    private void installModule() {
        assert !isModuleInstalled();

        // TODO(crbug.com/863064): This is a placeholder UI. Replace once proper UI is spec'd.
        Toast.makeText(ContextUtils.getApplicationContext(), R.string.vr_module_install_start_text,
                     Toast.LENGTH_SHORT)
                .show();

        ModuleInstaller.install("vr", (success) -> {
            if (success) {
                // Re-create delegate provider.
                sDelegateProvider = null;
                VrDelegate delegate = getDelegate();
                assert !(delegate instanceof VrDelegateFallback);
                delegate.onNativeLibraryAvailable();
            }
            // TODO(crbug.com/863064): This is a placeholder UI. Replace once proper UI is spec'd.
            int mToastTextRes = success ? R.string.vr_module_install_success_text
                                        : R.string.vr_module_install_failure_text;
            Toast.makeText(ContextUtils.getApplicationContext(), mToastTextRes, Toast.LENGTH_SHORT)
                    .show();
            if (mNativeVrModuleProvider != 0) {
                nativeOnInstalledModule(mNativeVrModuleProvider, success);
            }
        });
    }

    private static native void nativeInit();
    private static native void nativeRegisterJni();
    private native void nativeOnInstalledModule(long nativeVrModuleProvider, boolean success);
}
