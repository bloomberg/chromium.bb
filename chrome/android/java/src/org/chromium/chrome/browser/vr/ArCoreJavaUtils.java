// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.annotation.IntDef;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.module_installer.ModuleInstaller;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides ARCore classes access to java-related app functionality.
 */
@JNINamespace("vr")
public class ArCoreJavaUtils implements ModuleInstallUi.FailureUiListener {
    private static final String TAG = "ArCoreJavaUtils";
    private static final String AR_CORE_PACKAGE = "com.google.ar.core";
    private static final String METADATA_KEY_MIN_APK_VERSION = "com.google.ar.core.min_apk_version";
    private static final int ARCORE_NOT_INSTALLED_VERSION_CODE = -1;

    @IntDef({ArCoreInstallStatus.ARCORE_NEEDS_UPDATE, ArCoreInstallStatus.ARCORE_NOT_INSTALLED,
            ArCoreInstallStatus.ARCORE_INSTALLED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ArCoreInstallStatus {
        int ARCORE_NEEDS_UPDATE = 0;
        int ARCORE_NOT_INSTALLED = 1;
        int ARCORE_INSTALLED = 2;
    }

    private long mNativeArCoreJavaUtils;
    private boolean mAppInfoInitialized;
    private int mAppMinArCoreApkVersionCode = ARCORE_NOT_INSTALLED_VERSION_CODE;
    private Tab mTab;

    // Instance that requested installation of ARCore.
    // Should be non-null only if there is a pending request to install ARCore.
    private static ArCoreJavaUtils sRequestInstallInstance;

    // Cached ArCoreShim instance - valid only after AR module was installed and
    // getArCoreShimInstance() was called.
    private static ArCoreShim sArCoreInstance;

    private static ArCoreShim getArCoreShimInstance() {
        if (sArCoreInstance != null) return sArCoreInstance;

        try {
            sArCoreInstance =
                    (ArCoreShim) Class.forName("org.chromium.chrome.browser.vr.ArCoreShimImpl")
                            .newInstance();
        } catch (ClassNotFoundException e) {
            // shouldn't happen - we should only call this method once AR module is installed.
            throw new RuntimeException(e);
        } catch (InstantiationException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }

        return sArCoreInstance;
    }

    @UsedByReflection("ArDelegate.java")
    public static void installArCoreDeviceProviderFactory() {
        nativeInstallArCoreDeviceProviderFactory();
    }

    /**
     * Gets the current application context.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    @CalledByNative
    private static ArCoreJavaUtils create(long nativeArCoreJavaUtils) {
        ThreadUtils.assertOnUiThread();
        return new ArCoreJavaUtils(nativeArCoreJavaUtils);
    }

    @CalledByNative
    private static String getArCoreShimLibraryPath() {
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            return ((BaseDexClassLoader) ContextUtils.getApplicationContext().getClassLoader())
                    .findLibrary("arcore_sdk_c");
        }
    }

    @Override
    public void onRetry() {
        if (mNativeArCoreJavaUtils == 0) return;
        requestInstallArModule(mTab);
    }

    @Override
    public void onCancel() {
        if (mNativeArCoreJavaUtils == 0) return;
        nativeOnRequestInstallArModuleResult(mNativeArCoreJavaUtils, /* success = */ false);
    }

    private ArCoreJavaUtils(long nativeArCoreJavaUtils) {
        mNativeArCoreJavaUtils = nativeArCoreJavaUtils;
        initializeAppInfo();
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeArCoreJavaUtils = 0;
    }

    private void initializeAppInfo() {
        try {
            mAppMinArCoreApkVersionCode = getMinArCoreApkVersionCode();
        } catch (IllegalStateException ise) {
            mAppMinArCoreApkVersionCode = ARCORE_NOT_INSTALLED_VERSION_CODE;
        }

        mAppInfoInitialized = true;

        // Need to be called before trying to access the AR module.
        ModuleInstaller.init();
    }

    /**
     * Gets minimum required version of ARCore APK that is needed by ARCore SDK.
     *
     * If the ARCore SDK is not yet available, the method will throw IllegalStateException.
     * It might be possible to reattempt to call this method at a later time, for example
     * when the ARCore SDK gets installed.
     *
     * @return minimum required version of ARCore APK that is needed by ARCore SDK.
     */
    private int getMinArCoreApkVersionCode() {
        Context context = getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        String packageName = context.getPackageName();

        Bundle metadata;
        try {
            metadata = packageManager.getApplicationInfo(packageName, PackageManager.GET_META_DATA)
                               .metaData;
        } catch (PackageManager.NameNotFoundException e) {
            throw new IllegalStateException("Could not load application package metadata", e);
        }

        if (metadata.containsKey(METADATA_KEY_MIN_APK_VERSION)) {
            return metadata.getInt(METADATA_KEY_MIN_APK_VERSION);
        } else {
            throw new IllegalStateException(
                    "Application manifest must contain meta-data " + METADATA_KEY_MIN_APK_VERSION);
        }
    }

    private int getArCoreApkVersionNumber() {
        try {
            PackageInfo info = getApplicationContext().getPackageManager().getPackageInfo(
                    AR_CORE_PACKAGE, PackageManager.GET_SERVICES);
            int version = info.versionCode;
            if (version == 0) {
                return ARCORE_NOT_INSTALLED_VERSION_CODE;
            }
            return version;
        } catch (PackageManager.NameNotFoundException e) {
            return ARCORE_NOT_INSTALLED_VERSION_CODE;
        }
    }

    // TODO(bialpio): this method could be converted to start using ArCoreApk.checkAvailability()
    private @ArCoreInstallStatus int getArCoreInstallStatus() {
        assert mAppInfoInitialized;
        int arCoreApkVersionNumber = getArCoreApkVersionNumber();
        if (arCoreApkVersionNumber == ARCORE_NOT_INSTALLED_VERSION_CODE) {
            return ArCoreInstallStatus.ARCORE_NOT_INSTALLED;
        } else if (arCoreApkVersionNumber == 0
                || arCoreApkVersionNumber < mAppMinArCoreApkVersionCode) {
            return ArCoreInstallStatus.ARCORE_NEEDS_UPDATE;
        }
        return ArCoreInstallStatus.ARCORE_INSTALLED;
    }

    @CalledByNative
    private boolean shouldRequestInstallSupportedArCore() {
        return getArCoreInstallStatus() != ArCoreInstallStatus.ARCORE_INSTALLED;
    }

    @CalledByNative
    private void requestInstallArModule(Tab tab) {
        mTab = tab;
        ModuleInstallUi ui = new ModuleInstallUi(mTab, R.string.ar_module_title, this);
        ui.showInstallStartUi();
        ModuleInstaller.install("ar", success -> {
            assert shouldRequestInstallArModule() != success;

            if (success) {
                mAppMinArCoreApkVersionCode = getMinArCoreApkVersionCode();
            }

            if (mNativeArCoreJavaUtils != 0) {
                if (!success) {
                    ui.showInstallFailureUi();
                    return;
                }

                ui.showInstallSuccessUi();
                nativeOnRequestInstallArModuleResult(mNativeArCoreJavaUtils, success);
            }
        });
    }

    @CalledByNative
    private void requestInstallSupportedArCore(final Tab tab) {
        assert shouldRequestInstallSupportedArCore();
        @ArCoreInstallStatus
        int arcoreInstallStatus = getArCoreInstallStatus();
        final Activity activity = tab.getActivity();
        String infobarText = null;
        String buttonText = null;
        switch (arcoreInstallStatus) {
            case ArCoreInstallStatus.ARCORE_NOT_INSTALLED:
                infobarText = activity.getString(R.string.ar_core_check_infobar_install_text);
                buttonText = activity.getString(R.string.app_banner_install);
                break;
            case ArCoreInstallStatus.ARCORE_NEEDS_UPDATE:
                infobarText = activity.getString(R.string.ar_core_check_infobar_update_text);
                buttonText = activity.getString(R.string.update_from_market);
                break;
            case ArCoreInstallStatus.ARCORE_INSTALLED:
                assert false;
                break;
        }

        SimpleConfirmInfoBarBuilder.Listener listener = new SimpleConfirmInfoBarBuilder.Listener() {
            @Override
            public void onInfoBarDismissed() {
                maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
            }

            @Override
            public boolean onInfoBarButtonClicked(boolean isPrimary) {
                try {
                    assert sRequestInstallInstance == null;
                    ArCoreShim.InstallStatus installStatus =
                            getArCoreShimInstance().requestInstall(activity, true);

                    if (installStatus == ArCoreShim.InstallStatus.INSTALL_REQUESTED) {
                        // Install flow will resume in onArCoreRequestInstallReturned, mark that
                        // there is active request. Native code notification will be deferred until
                        // our activity gets resumed.
                        sRequestInstallInstance = ArCoreJavaUtils.this;
                    } else if (installStatus == ArCoreShim.InstallStatus.INSTALLED) {
                        // No need to install - notify native code.
                        maybeNotifyNativeOnRequestInstallSupportedArCoreResult(true);
                    }

                } catch (ArCoreShim.UnavailableDeviceNotCompatibleException e) {
                    sRequestInstallInstance = null;
                    Log.w(TAG, "ARCore installation request failed with exception: %s",
                            e.toString());

                    maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                } catch (ArCoreShim.UnavailableUserDeclinedInstallationException e) {
                    maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                }

                return false;
            }
        };
        // TODO(ijamardo, https://crbug.com/838833): Add icon for AR info bar.
        SimpleConfirmInfoBarBuilder.create(tab, listener, InfoBarIdentifier.AR_CORE_UPGRADE_ANDROID,
                R.drawable.vr_services, infobarText, buttonText, null, true);
    }

    @CalledByNative
    private boolean canRequestInstallArModule() {
        // We can only try to install the AR module if we are in a bundle mode.
        return BundleUtils.isBundle();
    }

    @CalledByNative
    private boolean shouldRequestInstallArModule() {
        try {
            // Try to find class in AR module that has not been obfuscated.
            Class.forName("com.google.ar.core.ArCoreApk");
            return false;
        } catch (ClassNotFoundException e) {
            return true;
        }
    }

    private void onArCoreRequestInstallReturned(Activity activity) {
        try {
            // Since |userRequestedInstall| parameter is false, the below call should
            // throw if ARCore is still not installed - no need to check the result.
            getArCoreShimInstance().requestInstall(activity, false);
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(true);
        } catch (ArCoreShim.UnavailableDeviceNotCompatibleException e) {
            Log.w(TAG, "Exception thrown when trying to validate install state of ARCore: %s",
                    e.toString());
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        } catch (ArCoreShim.UnavailableUserDeclinedInstallationException e) {
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        }
    }

    /**
     * This method should be called by the Activity that gets resumed.
     * We are only interested in the cases where our current Activity got paused
     * as a result of a call to ArCoreApk.requestInstall() method.
     */
    @UsedByReflection("ArDelegate.java")
    public static void onResumeActivityWithNative(Activity activity) {
        if (sRequestInstallInstance != null) {
            sRequestInstallInstance.onArCoreRequestInstallReturned(activity);
            sRequestInstallInstance = null;
        }
    }

    /**
     * Helper used to notify native code about the result of the request to install ARCore.
     */
    private void maybeNotifyNativeOnRequestInstallSupportedArCoreResult(boolean success) {
        if (mNativeArCoreJavaUtils != 0) {
            nativeOnRequestInstallSupportedArCoreResult(mNativeArCoreJavaUtils, success);
        }
    }

    private static native void nativeInstallArCoreDeviceProviderFactory();
    private native void nativeOnRequestInstallArModuleResult(
            long nativeArCoreJavaUtils, boolean success);
    private native void nativeOnRequestInstallSupportedArCoreResult(
            long nativeArCoreJavaUtils, boolean success);
}
