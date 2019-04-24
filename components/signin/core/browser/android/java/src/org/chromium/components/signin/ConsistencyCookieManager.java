// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.support.annotation.MainThread;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Created by native code to get status of {@link AccountManagerFacade#isUpdatePending()} and
 * notifications when it changes.
 */
public class ConsistencyCookieManager implements ObservableValue.Observer {
    private final long mNativeConsistencyCookieManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final SigninActivityMonitor mSigninActivityMonitor;
    private boolean mIsUpdatePending;

    private ConsistencyCookieManager(long nativeConsistencyCookieManager) {
        ThreadUtils.assertOnUiThread();
        mNativeConsistencyCookieManager = nativeConsistencyCookieManager;
        mAccountManagerFacade = AccountManagerFacade.get();
        mSigninActivityMonitor = SigninActivityMonitor.get();

        mAccountManagerFacade.isUpdatePending().addObserver(this);
        mSigninActivityMonitor.hasOngoingActivity().addObserver(this);

        mIsUpdatePending = calculateIsUpdatePending();
    }

    @Override
    public void onValueChanged() {
        boolean state = calculateIsUpdatePending();

        if (mIsUpdatePending == state) return;
        mIsUpdatePending = state;
        ConsistencyCookieManagerJni.get().onIsUpdatePendingChanged(mNativeConsistencyCookieManager);
    }

    private boolean calculateIsUpdatePending() {
        return mAccountManagerFacade.isUpdatePending().get()
                || mSigninActivityMonitor.hasOngoingActivity().get();
    }

    @CalledByNative
    @MainThread
    private static ConsistencyCookieManager create(long nativeConsistencyCookieManager) {
        return new ConsistencyCookieManager(nativeConsistencyCookieManager);
    }

    @CalledByNative
    @MainThread
    private void destroy() {
        ThreadUtils.assertOnUiThread();
        mAccountManagerFacade.isUpdatePending().removeObserver(this);
    }

    @CalledByNative
    @MainThread
    private boolean getIsUpdatePending() {
        ThreadUtils.assertOnUiThread();
        return mIsUpdatePending;
    }

    @JNINamespace("signin")
    @NativeMethods
    interface Natives {
        void onIsUpdatePendingChanged(long nativeConsistencyCookieManagerAndroid);
    }
}
