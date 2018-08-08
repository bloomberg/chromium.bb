// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;

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

    private VrModuleProvider() {}

    // TODO(crbug/870056): Move resources into VR DFM.
    private void silenceLintErrors() {
        int[] res = new int[] {
                R.string.vr_shell_feedback_infobar_feedback_button,
                R.string.vr_shell_feedback_infobar_description,
                R.string.vr_services_check_infobar_install_text,
                R.string.vr_services_check_infobar_update_text,
                R.string.vr_services_check_infobar_install_button,
                R.string.vr_services_check_infobar_update_button, R.anim.stay_hidden,
                R.drawable.vr_services,
        };
    }

    private static native void nativeRegisterJni();
}
