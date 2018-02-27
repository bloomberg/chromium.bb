// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.os.UserManager;
import android.provider.Settings;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.TokenBindingService;
import android.webkit.ValueCallback;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewDatabase;
import android.webkit.WebViewFactory;
import android.webkit.WebViewFactoryProvider;
import android.webkit.WebViewProvider;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwAutofillProvider;
import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.ResourcesContextWrapperFactory;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.NativeLibraries;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.content.browser.selection.LGEmailActionModeWorkaround;

import java.io.File;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/**
 * Entry point to the WebView. The system framework talks to this class to get instances of the
 * implementation classes.
 */
@SuppressWarnings("deprecation")
public class WebViewChromiumFactoryProvider implements WebViewFactoryProvider {
    private static final String TAG = "WebViewChromiumFactoryProvider";

    private static final String CHROMIUM_PREFS_NAME = "WebViewChromiumPrefs";
    private static final String VERSION_CODE_PREF = "lastVersionCodeUsed";

    private final static Object sSingletonLock = new Object();
    private static WebViewChromiumFactoryProvider sSingleton;

    private final WebViewChromiumRunQueue mRunQueue = new WebViewChromiumRunQueue(
            () -> { return WebViewChromiumFactoryProvider.this.mAwInit.hasStarted(); });

    /* package */ WebViewChromiumRunQueue getRunQueue() {
        return mRunQueue;
    }

    // We have a 4 second timeout to try to detect deadlocks to detect and aid in debuggin
    // deadlocks.
    // Do not call this method while on the UI thread!
    /* package */ void runVoidTaskOnUiThreadBlocking(Runnable r) {
        mRunQueue.runVoidTaskOnUiThreadBlocking(r);
    }

    /* package */ <T> T runOnUiThreadBlocking(Callable<T> c) {
        return mRunQueue.runBlockingFuture(new FutureTask<T>(c));
    }

    /* package */ void addTask(Runnable task) {
        mRunQueue.addTask(task);
    }

    /**
     * Class that takes care of chromium lazy initialization.
     * This is package-public so that a downstream subclass can access it.
     */
    /* package */ WebViewChromiumAwInit mAwInit;

    private SharedPreferences mWebViewPrefs;
    private WebViewDelegate mWebViewDelegate;

    boolean mShouldDisableThreadChecking;

    // Initialization guarded by mAwInit.getLock()
    private Statics mStaticsAdapter;

    // TODO(gsennton) remove this when downstream doesn't depend on it anymore
    // Guards accees to adapters.
    // This member is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    /* package */ final Object mAdapterLock = new Object();

    /**
     * Thread-safe way to set the one and only WebViewChromiumFactoryProvider.
     */
    private static void setSingleton(WebViewChromiumFactoryProvider provider) {
        synchronized (sSingletonLock) {
            if (sSingleton != null) {
                throw new RuntimeException(
                        "WebViewChromiumFactoryProvider should only be set once!");
            }
            sSingleton = provider;
        }
    }

    /**
     * Thread-safe way to get the one and only WebViewChromiumFactoryProvider.
     */
    static WebViewChromiumFactoryProvider getSingleton() {
        synchronized (sSingletonLock) {
            if (sSingleton == null) {
                throw new RuntimeException("WebViewChromiumFactoryProvider has not been set!");
            }
            return sSingleton;
        }
    }

    /**
     * Entry point for newer versions of Android.
     */
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProvider(delegate);
    }

    /**
     * Constructor called by the API 21 version of {@link WebViewFactory} and earlier.
     */
    public WebViewChromiumFactoryProvider() {
        initialize(WebViewDelegateFactory.createApi21CompatibilityDelegate());
    }

    /**
     * Constructor called by the API 22 version of {@link WebViewFactory} and later.
     */
    public WebViewChromiumFactoryProvider(android.webkit.WebViewDelegate delegate) {
        initialize(WebViewDelegateFactory.createProxyDelegate(delegate));
    }

    /**
     * Constructor for internal use when a proxy delegate has already been created.
     */
    WebViewChromiumFactoryProvider(WebViewDelegate delegate) {
        initialize(delegate);
    }

    // Protected to allow downstream to override.
    protected WebViewChromiumAwInit createAwInit() {
        return new WebViewChromiumAwInit(this);
    }

    @TargetApi(Build.VERSION_CODES.N) // For getSystemService() and isUserUnlocked().
    private void initialize(WebViewDelegate webViewDelegate) {
        mAwInit = createAwInit();
        mWebViewDelegate = webViewDelegate;
        Context ctx = mWebViewDelegate.getApplication().getApplicationContext();

        // If the application context is DE, but we have credentials, use a CE context instead
        try {
            checkStorageIsNotDeviceProtected(mWebViewDelegate.getApplication());
        } catch (IllegalArgumentException e) {
            assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
            if (!ctx.getSystemService(UserManager.class).isUserUnlocked()) {
                throw e;
            }
            ctx = ctx.createCredentialProtectedStorageContext();
        }

        // WebView needs to make sure to always use the wrapped application context.
        ContextUtils.initApplicationContext(ResourcesContextWrapperFactory.get(ctx));

        CommandLineUtil.initCommandLine();

        boolean multiProcess = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Ask the system if multiprocess should be enabled on O+.
            multiProcess = mWebViewDelegate.isMultiProcessEnabled();
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            // Check the multiprocess developer setting directly on N.
            multiProcess = Settings.Global.getInt(
                    ContextUtils.getApplicationContext().getContentResolver(),
                    Settings.Global.WEBVIEW_MULTIPROCESS, 0) == 1;
        }
        if (multiProcess) {
            CommandLine cl = CommandLine.getInstance();
            cl.appendSwitch("webview-sandboxed-renderer");
        }

        ThreadUtils.setWillOverrideUiThread();
        // Load chromium library.
        AwBrowserProcess.loadLibrary(mWebViewDelegate.getDataDirectorySuffix());

        final PackageInfo packageInfo = WebViewFactory.getLoadedPackageInfo();

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            // Load glue-layer support library.
            System.loadLibrary("webviewchromium_plat_support");

            // Use shared preference to check for package downgrade.
            // Since N, getSharedPreferences creates the preference dir if it doesn't exist,
            // causing a disk write.
            mWebViewPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                    CHROMIUM_PREFS_NAME, Context.MODE_PRIVATE);
            int lastVersion = mWebViewPrefs.getInt(VERSION_CODE_PREF, 0);
            int currentVersion = packageInfo.versionCode;
            if (!versionCodeGE(currentVersion, lastVersion)) {
                // The WebView package has been downgraded since we last ran in this application.
                // Delete the WebView data directory's contents.
                String dataDir = PathUtils.getDataDirectory();
                Log.i(TAG, "WebView package downgraded from " + lastVersion
                        + " to " + currentVersion + "; deleting contents of " + dataDir);
                deleteContents(new File(dataDir));
            }
            if (lastVersion != currentVersion) {
                mWebViewPrefs.edit().putInt(VERSION_CODE_PREF, currentVersion).apply();
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        mShouldDisableThreadChecking =
                shouldDisableThreadChecking(ContextUtils.getApplicationContext());
        // Now safe to use WebView data directory.

        setSingleton(this);
    }

    /* package */ static void checkStorageIsNotDeviceProtected(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && context.isDeviceProtectedStorage()) {
            throw new IllegalArgumentException(
                    "WebView cannot be used with device protected storage");
        }
    }

    /**
     * Both versionCodes should be from a WebView provider package implemented by Chromium.
     * VersionCodes from other kinds of packages won't make any sense in this method.
     *
     * An introduction to Chromium versionCode scheme:
     * "BBBBPPPAX"
     * BBBB: 4 digit branch number. It monotonically increases over time.
     * PPP: patch number in the branch. It is padded with zeroes to the left. These three digits may
     * change their meaning in the future.
     * A: architecture digit.
     * X: A digit to differentiate APKs for other reasons.
     *
     * This method takes the "BBBB" of versionCodes and compare them.
     *
     * @return true if versionCode1 is higher than or equal to versionCode2.
     */
    private static boolean versionCodeGE(int versionCode1, int versionCode2) {
        int v1 = versionCode1 / 100000;
        int v2 = versionCode2 / 100000;

        return v1 >= v2;
    }

    private static void deleteContents(File dir) {
        File[] files = dir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    deleteContents(file);
                }
                if (!file.delete()) {
                    Log.w(TAG, "Failed to delete " + file);
                }
            }
        }
    }

    public static boolean preloadInZygote() {
        for (String library : NativeLibraries.LIBRARIES) {
            System.loadLibrary(library);
        }
        return true;
    }

   SharedPreferences getWebViewPrefs() {
      return mWebViewPrefs;
   }

    @Override
    public Statics getStatics() {
        synchronized (mAwInit.getLock()) {
            SharedStatics sharedStatics = mAwInit.getStatics();
            if (mStaticsAdapter == null) {
                mStaticsAdapter = new WebViewChromiumFactoryProvider.Statics() {
                    @Override
                    public String findAddress(String addr) {
                        return sharedStatics.findAddress(addr);
                    }

                    @Override
                    public String getDefaultUserAgent(Context context) {
                        return sharedStatics.getDefaultUserAgent(context);
                    }

                    @Override
                    public void setWebContentsDebuggingEnabled(boolean enable) {
                        sharedStatics.setWebContentsDebuggingEnabled(enable);
                    }

                    @Override
                    public void clearClientCertPreferences(Runnable onCleared) {
                        sharedStatics.clearClientCertPreferences(onCleared);
                    }

                    @Override
                    public void freeMemoryForTests() {
                        sharedStatics.freeMemoryForTests();
                    }

                    @Override
                    public void enableSlowWholeDocumentDraw() {
                        sharedStatics.enableSlowWholeDocumentDraw();
                    }

                    @Override
                    public Uri[] parseFileChooserResult(int resultCode, Intent intent) {
                        return sharedStatics.parseFileChooserResult(resultCode, intent);
                    }

                    @Override
                    public void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
                        sharedStatics.initSafeBrowsing(
                                context, CallbackConverter.fromValueCallback(callback));
                    }

                    @Override
                    public void setSafeBrowsingWhitelist(
                            List<String> urls, ValueCallback<Boolean> callback) {
                        sharedStatics.setSafeBrowsingWhitelist(
                                urls, CallbackConverter.fromValueCallback(callback));
                    }

                    @Override
                    public Uri getSafeBrowsingPrivacyPolicyUrl() {
                        return sharedStatics.getSafeBrowsingPrivacyPolicyUrl();
                    }
                };
            }
        }
        return mStaticsAdapter;
    }

    @Override
    public WebViewProvider createWebView(WebView webView, WebView.PrivateAccess privateAccess) {
        return new WebViewChromium(this, webView, privateAccess, mShouldDisableThreadChecking);
    }

    // Workaround for IME thread crashes on grandfathered OEM apps.
    private boolean shouldDisableThreadChecking(Context context) {
        String appName = context.getPackageName();
        int versionCode = PackageUtils.getPackageVersion(context, appName);
        int appTargetSdkVersion = context.getApplicationInfo().targetSdkVersion;
        if (versionCode == -1) return false;

        boolean shouldDisable = false;

        // crbug.com/651706
        final String lgeMailPackageId = "com.lge.email";
        if (lgeMailPackageId.equals(appName)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.N) return false;
            // This is the last broken version shipped on LG V20/NRD90M.
            if (versionCode > LGEmailActionModeWorkaround.LGEmailWorkaroundMaxVersion) return false;
            shouldDisable = true;
        }

        // crbug.com/655759
        // Also want to cover ".att" variant suffix package name.
        final String yahooMailPackageId = "com.yahoo.mobile.client.android.mail";
        if (appName.startsWith(yahooMailPackageId)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.M) return false;
            if (versionCode > 1315850) return false;
            shouldDisable = true;
        }

        // crbug.com/622151
        final String htcMailPackageId = "com.htc.android.mail";
        if (htcMailPackageId.equals(appName)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.M) return false;
            // This value is provided by HTC.
            if (versionCode >= 866001861) return false;
            shouldDisable = true;
        }

        if (shouldDisable) {
            Log.w(TAG, "Disabling thread check in WebView. "
                            + "APK name: " + appName + ", versionCode: " + versionCode
                            + ", targetSdkVersion: " + appTargetSdkVersion);
        }
        return shouldDisable;
    }

    @Override
    public GeolocationPermissions getGeolocationPermissions() {
        return mAwInit.getGeolocationPermissions();
    }

    @Override
    public CookieManager getCookieManager() {
        return mAwInit.getCookieManager();
    }

    @Override
    public ServiceWorkerController getServiceWorkerController() {
        return mAwInit.getServiceWorkerController();
    }

    @Override
    public TokenBindingService getTokenBindingService() {
        return mAwInit.getTokenBindingService();
    }

    @Override
    public android.webkit.WebIconDatabase getWebIconDatabase() {
        return mAwInit.getWebIconDatabase();
    }

    @Override
    public WebStorage getWebStorage() {
        return mAwInit.getWebStorage();
    }

    @Override
    public WebViewDatabase getWebViewDatabase(final Context context) {
        return mAwInit.getWebViewDatabase(context);
    }

    WebViewDelegate getWebViewDelegate() {
        return mWebViewDelegate;
    }

    WebViewContentsClientAdapter createWebViewContentsClientAdapter(WebView webView,
            Context context) {
        return new WebViewContentsClientAdapter(webView, context, mWebViewDelegate);
    }

    AutofillProvider createAutofillProvider(Context context, ViewGroup containerView) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return null;
        return new AwAutofillProvider(context, containerView);
    }

    void startYourEngines(boolean onMainThread) {
        mAwInit.startYourEngines(onMainThread);
    }

    boolean hasStarted() {
        return mAwInit.hasStarted();
    }

    // Only on UI thread.
    AwBrowserContext getBrowserContextOnUiThread() {
        return mAwInit.getBrowserContextOnUiThread();
    }

    WebViewChromiumAwInit getAwInit() {
        return mAwInit;
    }
}
