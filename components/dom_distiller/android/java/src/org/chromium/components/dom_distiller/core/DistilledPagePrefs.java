// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.chromium.base.JNINamespace;

/**
 * Wrapper for the dom_distiller::DistilledPagePrefs.
 */
@JNINamespace("dom_distiller::android")
public final class DistilledPagePrefs {

    private final long mDistilledPagePrefsAndroid;

    DistilledPagePrefs(long distilledPagePrefsPtr) {
        mDistilledPagePrefsAndroid = nativeInit(distilledPagePrefsPtr);
    }

    // TODO(sunangel): Add observer support from this Java class to native
    // counterpart so UI can be updated across tabs.
    public void setTheme(Theme theme) {
        nativeSetTheme(mDistilledPagePrefsAndroid, theme.asNativeEnum());
    }

    public Theme getTheme() {
        return Theme.getThemeForValue(
            nativeGetTheme(mDistilledPagePrefsAndroid));
    }

    private native long nativeInit(long distilledPagePrefPtr);
    private native void nativeSetTheme(long nativeDistilledPagePrefsAndroid,
        int theme);
    private native int nativeGetTheme(
        long nativeDistilledPagePrefsAndroid);
}
