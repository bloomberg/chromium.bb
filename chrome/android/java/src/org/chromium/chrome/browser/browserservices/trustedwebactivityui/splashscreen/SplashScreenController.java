// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.customtabs.TrustedWebUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.TranslucentCustomTabActivity;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;

import java.lang.reflect.Method;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Orchestrates the flow of showing and removing splash screens for apps based on Trusted Web
 * Activities.
 *
 * The flow is as follows:
 * - TWA client app verifies conditions for showing splash screen. If the checks pass, it shows the
 * splash screen immediately.
 * - The client passes the URI to a file with the splash image to
 * {@link android.support.customtabs.CustomTabsService}. The image is decoded and put into
 * {@link SplashImageHolder}.
 * - The client then launches a TWA, at which point the Bitmap is already available.
 * - ChromeLauncherActivity calls {@link #handleIntent}, which starts
 * {@link TranslucentCustomTabActivity} - a CustomTabActivity with translucent style. The
 * translucency is necessary in order to avoid a flash that might be seen when starting the activity
 * before the splash screen is attached.
 * - {@link TranslucentCustomTabActivity} creates an instance of this class, which immediately
 * displays the splash screen in an ImageView on top of the rest of view hierarchy.
 * - It also immediately removes the translucency via a reflective call to
 * {@link Activity#convertFromTranslucent}. This is important for performance, otherwise the windows
 * under Chrome will continue being drawn (e.g. launcher with wallpaper). Without removing
 * traslucency, we also see visual glitches when closing TWA and sending it to background (example:
 * https://crbug.com/856544#c30).
 * - It waits for the page to load, and removes the splash image once first paint (or a failure)
 * occurs.
 *
 * Lifecycle: this class is resolved only once when CustomTabActivity is launched, and is
 * gc-ed when it finishes its job (to that end, it removes all observers it has set).
 * If these lifecycle assumptions change, consider whether @ActivityScope needs to be added.
 */
public class SplashScreenController implements InflationObserver {

    private static final String TAG = "TwaSplashScreens";

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final Lazy<CompositorViewHolder> mCompositorViewHolder;
    private final SplashImageHolder mSplashImageCache;
    private final CustomTabIntentDataProvider mIntentDataProvider;

    @Nullable
    private ImageView mSplashView;

    @Inject
    public SplashScreenController(TabObserverRegistrar tabObserverRegistrar,
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Lazy<CompositorViewHolder> compositorViewHolder,
            SplashImageHolder splashImageCache,
            CustomTabIntentDataProvider intentDataProvider) {
        mSplashImageCache = splashImageCache;
        mIntentDataProvider = intentDataProvider;
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabObserverRegistrar = tabObserverRegistrar;
        mCompositorViewHolder = compositorViewHolder;

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onPreInflationStartup() {
        createAndAttachSplashView();
        removeTranslucency();
    }

    private void removeTranslucency() {
        // Removing the temporary translucency, so that underlying windows don't get drawn.
        try {
            Method method = Activity.class.getDeclaredMethod("convertFromTranslucent");
            method.setAccessible(true);
            method.invoke(mActivity);
        } catch (ReflectiveOperationException e) {
            // Method not found or threw an exception.
            assert false : "Failed to remove activity translucency reflectively";
            Log.e(TAG, "Failed to remove activity translucency reflectively");
        }
    }

    private void createAndAttachSplashView() {
        Bitmap bitmap = mSplashImageCache.takeImage(mIntentDataProvider.getSession());
        if (bitmap == null) {
            mLifecycleDispatcher.unregister(this);
            return;
        }
        Bundle params = mIntentDataProvider.getIntent().getBundleExtra(
                TrustedWebUtils.EXTRA_SPLASH_SCREEN_PARAMS);
        int backgroundColor = params.getInt(TrustedWebUtils.SplashScreenParamKey.BACKGROUND_COLOR,
                Color.WHITE);

        mSplashView = new ImageView(mActivity);
        mSplashView.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        mSplashView.setImageBitmap(bitmap);
        mSplashView.setBackgroundColor(backgroundColor);
        mSplashView.setScaleType(ImageView.ScaleType.FIT_CENTER);
        // At this point the actual layout is not yet inflated. When it is, it replaces the
        // splash screen, but we put it back on top in onPostInflationStartup.
        mActivity.setContentView(mSplashView);
    }

    @Override
    public void onPostInflationStartup() {
        if (mSplashView == null) {
            assert false;
            return;
        }

        // In rare cases I see toolbar flickering. TODO(pshmakov): investigate why.
        mActivity.findViewById(R.id.coordinator).setVisibility(View.INVISIBLE);

        getRootView().addView(mSplashView);
        observeTab();
    }

    private ViewGroup getRootView() {
        return mActivity.findViewById(android.R.id.content);
    }

    private void observeTab() {
        mTabObserverRegistrar.registerTabObserver(new EmptyTabObserver() {

            // Whichever of the events below occurs first will trigger hiding of splash screen.
            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                onPageReady(tab);
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                onPageReady(tab);
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                onPageReady(tab);
            }

            @Override
            public void onCrash(Tab tab) {
                onPageReady(tab);
            }

            private void onPageReady(Tab tab) {
                tab.removeObserver(this); // TODO(pshmakov): make TabObserverRegistrar do this.
                mTabObserverRegistrar.unregisterTabObserver(this);
                removeSplashScreen();
            }
        });
    }

    private void removeSplashScreen() {
        if (mSplashView == null) {
            assert false : "Tried to remove splash screen twice";
            return;
        }

        final View splashView = mSplashView;
        mSplashView = null;

        mActivity.findViewById(R.id.coordinator).setVisibility(View.VISIBLE);
        mCompositorViewHolder.get().getCompositorView().surfaceRedrawNeededAsync(
                () -> getRootView().removeView(splashView));

        mLifecycleDispatcher.unregister(this);  // Unregister to get gc-ed
    }

    /**
     * Returns true if the intent corresponds to a TWA with a splash screen.
     */
    public static boolean intentIsForTwaWithSplashScreen(Intent intent) {
        boolean isTrustedWebActivity = IntentUtils.safeGetBooleanExtra(intent,
                TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
        boolean requestsSplashScreen = IntentUtils.safeGetParcelableExtra(intent,
                        TrustedWebUtils.EXTRA_SPLASH_SCREEN_PARAMS) != null;
        return isTrustedWebActivity && requestsSplashScreen;
    }

    /**
     * Handles the intent if it should launch a TWA with splash screen.
     * @param activity Activity, from which to start the next one.
     * @param intent Incoming intent.
     * @return Whether the intent was handled.
     */
    public static boolean handleIntent(Activity activity, Intent intent) {
        if (!intentIsForTwaWithSplashScreen(intent)) return false;

        intent.setClassName(activity, TranslucentCustomTabActivity.class.getName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        activity.startActivity(intent);
        activity.overridePendingTransition(0, 0);
        return true;
    }
}
