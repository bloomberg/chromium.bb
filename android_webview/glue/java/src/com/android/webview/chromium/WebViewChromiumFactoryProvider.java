// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.Manifest;
import android.app.ActivityManager;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Looper;
import android.os.Process;
import android.os.StrictMode;
import android.os.UserManager;
import android.provider.Settings;
import android.util.Log;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.TokenBindingService;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewDatabase;
import android.webkit.WebViewFactory;
import android.webkit.WebViewFactoryProvider;
import android.webkit.WebViewProvider;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwDevToolsServer;
import org.chromium.android_webview.AwNetworkChangeNotifierRegistrationPolicy;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwResource;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.android_webview.ResourcesContextWrapperFactory;
import org.chromium.base.BuildConfig;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.PackageUtils;
import org.chromium.base.PathService;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.NativeLibraries;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.ContentViewStatics;
import org.chromium.net.NetworkChangeNotifier;

import java.io.File;
import java.util.Queue;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * Entry point to the WebView. The system framework talks to this class to get instances of the
 * implementation classes.
 */
@SuppressWarnings("deprecation")
public class WebViewChromiumFactoryProvider implements WebViewFactoryProvider {
    private static final String TAG = "WebViewChromiumFactoryProvider";

    private static final String CHROMIUM_PREFS_NAME = "WebViewChromiumPrefs";
    private static final String VERSION_CODE_PREF = "lastVersionCodeUsed";
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/webview-command-line";
    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    private class WebViewChromiumRunQueue {
        public WebViewChromiumRunQueue() {
            mQueue = new ConcurrentLinkedQueue<Runnable>();
        }

        public void addTask(Runnable task) {
            mQueue.add(task);
            if (WebViewChromiumFactoryProvider.this.hasStarted()) {
                ThreadUtils.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        drainQueue();
                    }
                });
            }
        }

        public void drainQueue() {
            if (mQueue == null || mQueue.isEmpty()) {
                return;
            }

            Runnable task = mQueue.poll();
            while (task != null) {
                task.run();
                task = mQueue.poll();
            }
        }

        private final Queue<Runnable> mQueue;
    }

    private final WebViewChromiumRunQueue mRunQueue = new WebViewChromiumRunQueue();

    private <T> T runBlockingFuture(FutureTask<T> task) {
        if (!hasStarted()) throw new RuntimeException("Must be started before we block!");
        if (ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("This method should only be called off the UI thread");
        }
        mRunQueue.addTask(task);
        try {
            return task.get(4, TimeUnit.SECONDS);
        } catch (java.util.concurrent.TimeoutException e) {
            throw new RuntimeException("Probable deadlock detected due to WebView API being called "
                            + "on incorrect thread while the UI thread is blocked.", e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    // We have a 4 second timeout to try to detect deadlocks to detect and aid in debuggin
    // deadlocks.
    // Do not call this method while on the UI thread!
    /* package */ void runVoidTaskOnUiThreadBlocking(Runnable r) {
        FutureTask<Void> task = new FutureTask<Void>(r, null);
        runBlockingFuture(task);
    }

    /* package */ <T> T runOnUiThreadBlocking(Callable<T> c) {
        return runBlockingFuture(new FutureTask<T>(c));
    }

    /* package */ void addTask(Runnable task) {
        mRunQueue.addTask(task);
    }

    // Guards accees to the other members, and is notifyAll() signalled on the UI thread
    // when the chromium process has been started.
    private final Object mLock = new Object();

    // Initialization guarded by mLock.
    private AwBrowserContext mBrowserContext;
    private Statics mStaticMethods;
    private GeolocationPermissionsAdapter mGeolocationPermissions;
    private CookieManagerAdapter mCookieManager;
    private Object mTokenBindingManager;
    private WebIconDatabaseAdapter mWebIconDatabase;
    private WebStorageAdapter mWebStorage;
    private WebViewDatabaseAdapter mWebViewDatabase;
    private AwDevToolsServer mDevToolsServer;
    private Object mServiceWorkerController;

    // Read/write protected by mLock.
    private boolean mStarted;

    private SharedPreferences mWebViewPrefs;
    private WebViewDelegate mWebViewDelegate;

    private boolean mShouldDisableThreadChecking;

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

    @SuppressFBWarnings("DMI_HARDCODED_ABSOLUTE_FILENAME")
    private void initialize(WebViewDelegate webViewDelegate) {
        mWebViewDelegate = webViewDelegate;
        Context ctx = mWebViewDelegate.getApplication().getApplicationContext();

        // If the application context is DE, but we have credentials, use a CE context instead
        try {
            checkStorageIsNotDeviceProtected(mWebViewDelegate.getApplication());
        } catch (IllegalArgumentException e) {
            if (!ctx.getSystemService(UserManager.class).isUserUnlocked()) {
                throw e;
            }
            ctx = ctx.createCredentialProtectedStorageContext();
        }

        // WebView needs to make sure to always use the wrapped application context.
        ContextUtils.initApplicationContext(ResourcesContextWrapperFactory.get(ctx));

        if (isBuildDebuggable()) {
            // Suppress the StrictMode violation as this codepath is only hit on debuggable builds.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            CommandLine.initFromFile(COMMAND_LINE_FILE);
            StrictMode.setThreadPolicy(oldPolicy);
        } else {
            CommandLine.init(null);
        }

        if (Settings.Global.getInt(ContextUtils.getApplicationContext().getContentResolver(),
                    Settings.Global.WEBVIEW_MULTIPROCESS, 0)
                == 1) {
            CommandLine cl = CommandLine.getInstance();
            cl.appendSwitch("webview-sandboxed-renderer");
        }

        ThreadUtils.setWillOverrideUiThread();
        // Load chromium library.
        AwBrowserProcess.loadLibrary();

        final PackageInfo packageInfo = WebViewFactory.getLoadedPackageInfo();

        // Load glue-layer support library.
        System.loadLibrary("webviewchromium_plat_support");

        // Use shared preference to check for package downgrade.
        mWebViewPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                CHROMIUM_PREFS_NAME, Context.MODE_PRIVATE);
        int lastVersion = mWebViewPrefs.getInt(VERSION_CODE_PREF, 0);
        int currentVersion = packageInfo.versionCode;
        if (!versionCodeGE(currentVersion, lastVersion)) {
            // The WebView package has been downgraded since we last ran in this application.
            // Delete the WebView data directory's contents.
            String dataDir = PathUtils.getDataDirectory();
            Log.i(TAG, "WebView package downgraded from " + lastVersion + " to " + currentVersion
                            + "; deleting contents of " + dataDir);
            deleteContents(new File(dataDir));
        }
        if (lastVersion != currentVersion) {
            mWebViewPrefs.edit().putInt(VERSION_CODE_PREF, currentVersion).apply();
        }

        mShouldDisableThreadChecking =
                shouldDisableThreadChecking(ContextUtils.getApplicationContext());
        // Now safe to use WebView data directory.
    }

    static void checkStorageIsNotDeviceProtected(Context context) {
        if ((Build.VERSION.CODENAME.equals("N") || Build.VERSION.SDK_INT > Build.VERSION_CODES.M)
                && context.isDeviceProtectedStorage()) {
            throw new IllegalArgumentException(
                    "WebView cannot be used with device protected storage");
        }
    }

    private static boolean isBuildDebuggable() {
        return !Build.TYPE.equals("user");
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

    private void initPlatSupportLibrary() {
        DrawGLFunctor.setChromiumAwDrawGLFunction(AwContents.getAwDrawGLFunction());
        AwContents.setAwDrawSWFunctionTable(GraphicsUtils.getDrawSWFunctionTable());
        AwContents.setAwDrawGLFunctionTable(GraphicsUtils.getDrawGLFunctionTable());
    }

    private void initNetworkChangeNotifier(Context applicationContext) {
        if (applicationContext.checkPermission(Manifest.permission.ACCESS_NETWORK_STATE,
                Process.myPid(), Process.myUid()) == PackageManager.PERMISSION_GRANTED) {
            NetworkChangeNotifier.init(applicationContext);
            NetworkChangeNotifier.setAutoDetectConnectivityState(
                    new AwNetworkChangeNotifierRegistrationPolicy());
        }
    }

    private void ensureChromiumStartedLocked(boolean onMainThread) {
        assert Thread.holdsLock(mLock);

        if (mStarted) { // Early-out for the common case.
            return;
        }

        Looper looper = !onMainThread ? Looper.myLooper() : Looper.getMainLooper();
        Log.v(TAG, "Binding Chromium to "
                        + (Looper.getMainLooper().equals(looper) ? "main" : "background")
                        + " looper " + looper);
        ThreadUtils.setUiThread(looper);

        if (ThreadUtils.runningOnUiThread()) {
            startChromiumLocked();
            return;
        }

        // We must post to the UI thread to cover the case that the user has invoked Chromium
        // startup by using the (thread-safe) CookieManager rather than creating a WebView.
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                synchronized (mLock) {
                    startChromiumLocked();
                }
            }
        });
        while (!mStarted) {
            try {
                // Important: wait() releases |mLock| the UI thread can take it :-)
                mLock.wait();
            } catch (InterruptedException e) {
                // Keep trying... eventually the UI thread will process the task we sent it.
            }
        }
    }

    // TODO: DIR_RESOURCE_PAKS_ANDROID needs to live somewhere sensible,
    // inlined here for simplicity setting up the HTMLViewer demo. Unfortunately
    // it can't go into base.PathService, as the native constant it refers to
    // lives in the ui/ layer. See ui/base/ui_base_paths.h
    private static final int DIR_RESOURCE_PAKS_ANDROID = 3003;

    private void startChromiumLocked() {
        assert Thread.holdsLock(mLock) && ThreadUtils.runningOnUiThread();

        // The post-condition of this method is everything is ready, so notify now to cover all
        // return paths. (Other threads will not wake-up until we release |mLock|, whatever).
        mLock.notifyAll();

        if (mStarted) {
            return;
        }

        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_WEBVIEW).ensureInitialized();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Error initializing WebView library", e);
        }

        PathService.override(PathService.DIR_MODULE, "/system/lib/");
        PathService.override(DIR_RESOURCE_PAKS_ANDROID, "/system/framework/webview/paks");

        // Make sure that ResourceProvider is initialized before starting the browser process.
        final String webViewPackageName = WebViewFactory.getLoadedPackageInfo().packageName;
        Context context = ContextUtils.getApplicationContext();
        setUpResources(webViewPackageName, context);
        initPlatSupportLibrary();
        initNetworkChangeNotifier(context);
        final boolean isExternalService = true;
        AwBrowserProcess.configureChildProcessLauncher(webViewPackageName, isExternalService);
        AwBrowserProcess.start();

        if (isBuildDebuggable()) {
            setWebContentsDebuggingEnabled(true);
        }

        TraceEvent.setATraceEnabled(mWebViewDelegate.isTraceTagEnabled());
        mWebViewDelegate.setOnTraceEnabledChangeListener(
                new WebViewDelegate.OnTraceEnabledChangeListener() {
                    @Override
                    public void onTraceEnabledChange(boolean enabled) {
                        TraceEvent.setATraceEnabled(enabled);
                    }
                });

        mStarted = true;

        // Initialize thread-unsafe singletons.
        AwBrowserContext awBrowserContext = getBrowserContextOnUiThread();
        mGeolocationPermissions = new GeolocationPermissionsAdapter(
                this, awBrowserContext.getGeolocationPermissions());
        mWebStorage = new WebStorageAdapter(this, AwQuotaManagerBridge.getInstance());
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            mServiceWorkerController = new ServiceWorkerControllerAdapter(
                    awBrowserContext.getServiceWorkerController());
        }

        mRunQueue.drainQueue();
    }

    boolean hasStarted() {
        return mStarted;
    }

    void startYourEngines(boolean onMainThread) {
        synchronized (mLock) {
            ensureChromiumStartedLocked(onMainThread);
        }
    }

    // Only on UI thread.
    AwBrowserContext getBrowserContextOnUiThread() {
        assert mStarted;

        if (BuildConfig.DCHECK_IS_ON && !ThreadUtils.runningOnUiThread()) {
            throw new RuntimeException(
                    "getBrowserContextOnUiThread called on " + Thread.currentThread());
        }

        if (mBrowserContext == null) {
            mBrowserContext =
                    new AwBrowserContext(mWebViewPrefs, ContextUtils.getApplicationContext());
        }
        return mBrowserContext;
    }

    private void setWebContentsDebuggingEnabled(boolean enable) {
        if (Looper.myLooper() != ThreadUtils.getUiThreadLooper()) {
            throw new RuntimeException(
                    "Toggling of Web Contents Debugging must be done on the UI thread");
        }
        if (mDevToolsServer == null) {
            if (!enable) return;
            mDevToolsServer = new AwDevToolsServer();
        }
        mDevToolsServer.setRemoteDebuggingEnabled(enable);
    }

    private void setUpResources(String webViewPackageName, Context context) {
        ResourceRewriter.rewriteRValues(
                mWebViewDelegate.getPackageId(context.getResources(), webViewPackageName));

        AwResource.setResources(context.getResources());
        AwResource.setConfigKeySystemUuidMapping(android.R.array.config_keySystemUuidMapping);
    }

    @Override
    public Statics getStatics() {
        synchronized (mLock) {
            if (mStaticMethods == null) {
                // TODO: Optimization potential: most these methods only need the native library
                // loaded and initialized, not the entire browser process started.
                // See also http://b/7009882
                ensureChromiumStartedLocked(true);
                mStaticMethods = new WebViewFactoryProvider.Statics() {
                    @Override
                    public String findAddress(String addr) {
                        return ContentViewStatics.findAddress(addr);
                    }

                    @Override
                    public String getDefaultUserAgent(Context context) {
                        return AwSettings.getDefaultUserAgent();
                    }

                    @Override
                    public void setWebContentsDebuggingEnabled(boolean enable) {
                        // Web Contents debugging is always enabled on debug builds.
                        if (!isBuildDebuggable()) {
                            WebViewChromiumFactoryProvider.this.setWebContentsDebuggingEnabled(
                                    enable);
                        }
                    }

                    @Override
                    public void clearClientCertPreferences(Runnable onCleared) {
                        AwContentsStatics.clearClientCertPreferences(onCleared);
                    }

                    @Override
                    public void freeMemoryForTests() {
                        if (ActivityManager.isRunningInTestHarness()) {
                            MemoryPressureListener.maybeNotifyMemoryPresure(
                                    ComponentCallbacks2.TRIM_MEMORY_COMPLETE);
                        }
                    }

                    @Override
                    public void enableSlowWholeDocumentDraw() {
                        WebViewChromium.enableSlowWholeDocumentDraw();
                    }

                    @Override
                    public Uri[] parseFileChooserResult(int resultCode, Intent intent) {
                        return AwContentsClient.parseFileChooserResult(resultCode, intent);
                    }
                };
            }
        }
        return mStaticMethods;
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

        boolean shouldDisable = false;

        // crbug.com/651706
        final String lgeMailPackageId = "com.lge.email";
        if (lgeMailPackageId.equals(appName)) {
            // The version code is provided by LGE.
            if (versionCode == -1 || versionCode >= 67700000) return false;
            shouldDisable = true;
        }

        // crbug.com/655759
        // Also want to cover ".att" variant suffix package name.
        final String yahooMailPackageId = "com.yahoo.mobile.client.android.mail";
        if (appName.startsWith(yahooMailPackageId)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.M) return false;
            if (versionCode == -1 || versionCode > 1315849) return false;
            shouldDisable = true;
        }

        // crbug.com/622151
        final String htcMailPackageId = "com.htc.android.mail";
        if (htcMailPackageId.equals(appName)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.M) return false;
            if (versionCode == -1) return false;
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
        synchronized (mLock) {
            if (mGeolocationPermissions == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mGeolocationPermissions;
    }

    @Override
    public CookieManager getCookieManager() {
        synchronized (mLock) {
            if (mCookieManager == null) {
                mCookieManager = new CookieManagerAdapter(new AwCookieManager());
            }
        }
        return mCookieManager;
    }

    @Override
    public ServiceWorkerController getServiceWorkerController() {
        synchronized (mLock) {
            if (mServiceWorkerController == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return (ServiceWorkerController) mServiceWorkerController;
    }

    public TokenBindingService getTokenBindingService() {
       synchronized (mLock) {
            if (mTokenBindingManager == null) {
                mTokenBindingManager = new TokenBindingManagerAdapter(this);
            }
        }
        return (TokenBindingService) mTokenBindingManager;
    }

    @Override
    public android.webkit.WebIconDatabase getWebIconDatabase() {
        synchronized (mLock) {
            if (mWebIconDatabase == null) {
                ensureChromiumStartedLocked(true);
                mWebIconDatabase = new WebIconDatabaseAdapter();
            }
        }
        return mWebIconDatabase;
    }

    @Override
    public WebStorage getWebStorage() {
        synchronized (mLock) {
            if (mWebStorage == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mWebStorage;
    }

    @Override
    public WebViewDatabase getWebViewDatabase(final Context context) {
        synchronized (mLock) {
            if (mWebViewDatabase == null) {
                ensureChromiumStartedLocked(true);
                mWebViewDatabase = new WebViewDatabaseAdapter(
                        this, HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE));
            }
        }
        return mWebViewDatabase;
    }

    WebViewDelegate getWebViewDelegate() {
        return mWebViewDelegate;
    }
}
