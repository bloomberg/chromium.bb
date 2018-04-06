// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Looper;
import android.os.Process;
import android.util.Log;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.TokenBindingService;
import android.webkit.WebStorage;
import android.webkit.WebViewDatabase;
import android.webkit.WebViewFactory;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwNetworkChangeNotifierRegistrationPolicy;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwResource;
import org.chromium.android_webview.AwServiceWorkerController;
import org.chromium.android_webview.AwTracingController;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.android_webview.VariationsSeedLoader;
import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathService;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Class controlling the Chromium initialization for WebView.
 * We hold on to most static objects used by WebView here.
 * This class is shared between the webkit glue layer and the support library glue layer.
 */
public class WebViewChromiumAwInit {
    private static final String TAG = "WebViewChromiumAwInit";

    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    // TODO(gsennton): store aw-objects instead of adapters here
    // Initialization guarded by mLock.
    private AwBrowserContext mBrowserContext;
    private SharedStatics mSharedStatics;
    private GeolocationPermissionsAdapter mGeolocationPermissions;
    private CookieManagerAdapter mCookieManager;
    private Object mTokenBindingManager;
    private WebIconDatabaseAdapter mWebIconDatabase;
    private WebStorageAdapter mWebStorage;
    private WebViewDatabaseAdapter mWebViewDatabase;
    private AwServiceWorkerController mServiceWorkerController;
    private AwTracingController mAwTracingController;
    private VariationsSeedLoader mSeedLoader;

    // Guards accees to the other members, and is notifyAll() signalled on the UI thread
    // when the chromium process has been started.
    // This member is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    /* package */ final Object mLock = new Object();

    // Read/write protected by mLock.
    private boolean mStarted;

    private final WebViewChromiumFactoryProvider mFactory;

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
    }

    AwTracingController getAwTracingController() {
        synchronized (mLock) {
            if (mAwTracingController == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mAwTracingController;
    }

    // TODO: DIR_RESOURCE_PAKS_ANDROID needs to live somewhere sensible,
    // inlined here for simplicity setting up the HTMLViewer demo. Unfortunately
    // it can't go into base.PathService, as the native constant it refers to
    // lives in the ui/ layer. See ui/base/ui_base_paths.h
    private static final int DIR_RESOURCE_PAKS_ANDROID = 3003;

    protected void startChromiumLocked() {
        assert Thread.holdsLock(mLock) && ThreadUtils.runningOnUiThread();

        // The post-condition of this method is everything is ready, so notify now to cover all
        // return paths. (Other threads will not wake-up until we release |mLock|, whatever).
        mLock.notifyAll();

        if (mStarted) {
            return;
        }

        final PackageInfo webViewPackageInfo = WebViewFactory.getLoadedPackageInfo();
        final Context context = ContextUtils.getApplicationContext();

        // Make sure that ResourceProvider is initialized before starting the browser process.
        Thread startUpResourcesThread = new Thread(new Runnable() {
            @Override
            public void run() {
                // Run this in parallel as it takes some time.
                setUpResources(webViewPackageInfo, context);
            }
        });
        startUpResourcesThread.start();

        // We are rewriting Java resources in the background.
        // NOTE: Any reference to Java resources will cause a crash.

        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_WEBVIEW).ensureInitialized();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Error initializing WebView library", e);
        }

        PathService.override(PathService.DIR_MODULE, "/system/lib/");
        PathService.override(DIR_RESOURCE_PAKS_ANDROID, "/system/framework/webview/paks");

        initPlatSupportLibrary();
        doNetworkInitializations(context);

        try {
            startUpResourcesThread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        // NOTE: Finished writing Java resources. From this point on, it's safe to use them.

        AwBrowserProcess.configureChildProcessLauncher();

        // finishVariationsInitLocked() must precede native initialization so the seed is available
        // when AwFieldTrialCreator::SetUpFieldTrials() runs.
        finishVariationsInitLocked();

        AwBrowserProcess.start();
        AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(true /* updateMetricsConsent */);

        mSharedStatics = new SharedStatics();
        if (CommandLineUtil.isBuildDebuggable()) {
            mSharedStatics.setWebContentsDebuggingEnabledUnconditionally(true);
        }

        TraceEvent.setATraceEnabled(mFactory.getWebViewDelegate().isTraceTagEnabled());
        mFactory.getWebViewDelegate().setOnTraceEnabledChangeListener(
                new WebViewDelegate.OnTraceEnabledChangeListener() {
                    @Override
                    public void onTraceEnabledChange(boolean enabled) {
                        TraceEvent.setATraceEnabled(enabled);
                    }
                });

        mStarted = true;

        RecordHistogram.recordSparseSlowlyHistogram(
                "Android.WebView.TargetSdkVersion", context.getApplicationInfo().targetSdkVersion);

        // Initialize thread-unsafe singletons.
        AwBrowserContext awBrowserContext = getBrowserContextOnUiThread();
        mGeolocationPermissions = new GeolocationPermissionsAdapter(
                mFactory, awBrowserContext.getGeolocationPermissions());
        mWebStorage = new WebStorageAdapter(mFactory, AwQuotaManagerBridge.getInstance());
        mAwTracingController = awBrowserContext.getTracingController();
        mServiceWorkerController = awBrowserContext.getServiceWorkerController();

        mFactory.getRunQueue().drainQueue();
    }

    private void setUpResources(PackageInfo webViewPackageInfo, Context context) {
        String packageName = webViewPackageInfo.packageName;
        if (webViewPackageInfo.applicationInfo.metaData != null) {
            packageName = webViewPackageInfo.applicationInfo.metaData.getString(
                    "com.android.webview.WebViewDonorPackage", packageName);
        }
        ResourceRewriter.rewriteRValues(
                mFactory.getWebViewDelegate().getPackageId(context.getResources(), packageName));

        AwResource.setResources(context.getResources());
        AwResource.setConfigKeySystemUuidMapping(android.R.array.config_keySystemUuidMapping);
    }

    boolean hasStarted() {
        return mStarted;
    }

    void startYourEngines(boolean onMainThread) {
        synchronized (mLock) {
            ensureChromiumStartedLocked(onMainThread);
        }
    }

    // This method is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    /* package */ void ensureChromiumStartedLocked(boolean onMainThread) {
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

    private void initPlatSupportLibrary() {
        DrawGLFunctor.setChromiumAwDrawGLFunction(AwContents.getAwDrawGLFunction());
        AwContents.setAwDrawSWFunctionTable(GraphicsUtils.getDrawSWFunctionTable());
        AwContents.setAwDrawGLFunctionTable(GraphicsUtils.getDrawGLFunctionTable());
    }

    private void doNetworkInitializations(Context applicationContext) {
        if (applicationContext.checkPermission(Manifest.permission.ACCESS_NETWORK_STATE,
                Process.myPid(), Process.myUid()) == PackageManager.PERMISSION_GRANTED) {
            NetworkChangeNotifier.init();
            NetworkChangeNotifier.setAutoDetectConnectivityState(
                    new AwNetworkChangeNotifierRegistrationPolicy());
        }

        AwContentsStatics.setCheckClearTextPermitted(
                applicationContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.O);
    }

    // Only on UI thread.
    AwBrowserContext getBrowserContextOnUiThread() {
        assert mStarted;

        if (BuildConfig.DCHECK_IS_ON && !ThreadUtils.runningOnUiThread()) {
            throw new RuntimeException(
                    "getBrowserContextOnUiThread called on " + Thread.currentThread());
        }

        if (mBrowserContext == null) {
            mBrowserContext = new AwBrowserContext(
                mFactory.getWebViewPrefs(), ContextUtils.getApplicationContext());
        }
        return mBrowserContext;
    }

    /**
     * Returns the lock used for guarding chromium initialization.
     * We make this public to let higher-level classes use this lock to guard variables
     * dependent on this class, to avoid introducing new locks (which can cause deadlocks).
     */
    public Object getLock() {
        return mLock;
    }

    public SharedStatics getStatics() {
        synchronized (mLock) {
            if (mSharedStatics == null) {
                // TODO: Optimization potential: most these methods only need the native library
                // loaded and initialized, not the entire browser process started.
                // See also http://b/7009882
                ensureChromiumStartedLocked(true);
            }
        }
        return mSharedStatics;
    }

    public GeolocationPermissions getGeolocationPermissions() {
        synchronized (mLock) {
            if (mGeolocationPermissions == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mGeolocationPermissions;
    }

    public CookieManager getCookieManager() {
        synchronized (mLock) {
            if (mCookieManager == null) {
                mCookieManager = new CookieManagerAdapter(new AwCookieManager());
            }
        }
        return mCookieManager;
    }

    public AwServiceWorkerController getServiceWorkerController() {
        synchronized (mLock) {
            if (mServiceWorkerController == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mServiceWorkerController;
    }

    public TokenBindingService getTokenBindingService() {
        synchronized (mLock) {
            if (mTokenBindingManager == null) {
                mTokenBindingManager = new TokenBindingManagerAdapter(mFactory);
            }
        }
        return (TokenBindingService) mTokenBindingManager;
    }

    public android.webkit.WebIconDatabase getWebIconDatabase() {
        synchronized (mLock) {
            if (mWebIconDatabase == null) {
                ensureChromiumStartedLocked(true);
                mWebIconDatabase = new WebIconDatabaseAdapter();
            }
        }
        return mWebIconDatabase;
    }

    public WebStorage getWebStorage() {
        synchronized (mLock) {
            if (mWebStorage == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mWebStorage;
    }

    public WebViewDatabase getWebViewDatabase(final Context context) {
        synchronized (mLock) {
            if (mWebViewDatabase == null) {
                ensureChromiumStartedLocked(true);
                mWebViewDatabase = new WebViewDatabaseAdapter(
                        mFactory, HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE));
            }
        }
        return mWebViewDatabase;
    }

    // See comments in VariationsSeedLoader.java on when it's safe to call this.
    public void startVariationsInit() {
        synchronized (mLock) {
            if (mSeedLoader == null) {
                mSeedLoader = new VariationsSeedLoader();
                mSeedLoader.startVariationsInit();
            }
        }
    }

    private void finishVariationsInitLocked() {
        assert Thread.holdsLock(mLock);
        if (mSeedLoader == null) {
            Log.e(TAG, "finishVariationsInitLocked() called before startVariationsInit()");
            startVariationsInit();
        }
        mSeedLoader.finishVariationsInit();
        mSeedLoader = null; // Allow this to be GC'd after its background thread finishes.
    }
}
