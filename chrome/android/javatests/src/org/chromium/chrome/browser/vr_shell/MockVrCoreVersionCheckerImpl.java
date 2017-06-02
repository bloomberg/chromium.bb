// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

/**
 * Mock version of VrCoreVersionCheckerImpl that allows setting of the return
 * value.
 */
public class MockVrCoreVersionCheckerImpl extends VrCoreVersionCheckerImpl {
    private boolean mUseActualImplementation;
    private int mMockReturnValue = VrCoreVersionChecker.VR_READY;
    private int mLastReturnValue = -1;

    @Override
    public int getVrCoreCompatibility() {
        mLastReturnValue =
                mUseActualImplementation ? super.getVrCoreCompatibility() : mMockReturnValue;
        return mLastReturnValue;
    }

    public int getLastReturnValue() {
        return mLastReturnValue;
    }

    public void setMockReturnValue(int value) {
        mMockReturnValue = value;
    }

    public void setUseActualImplementation(boolean useActual) {
        mUseActualImplementation = useActual;
    }
}
