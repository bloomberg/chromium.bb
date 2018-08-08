// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import java.util.ArrayList;
import java.util.List;

/**
 * Instantiates the VR delegates. If the VR module is not available this provider will
 * instantiate a fallback implementation.
 */
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
}
