// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.ClientAppDataRegister;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNightModeStateController;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.init.StartupTabPreloader;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigator;

import dagger.Lazy;
import dagger.Module;
import dagger.Provides;
import dagger.Reusable;

/**
 * Module for custom tab specific bindings.
 */
@Module
public class CustomTabActivityModule {
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabNightModeStateController mNightModeController;
    private final IntentIgnoringCriterion mIntentIgnoringCriterion;
    private final StartupTabPreloader mStartupTabPreloader;

    public CustomTabActivityModule(CustomTabIntentDataProvider intentDataProvider,
            CustomTabNightModeStateController nightModeController,
            IntentIgnoringCriterion intentIgnoringCriterion,
            StartupTabPreloader startupTabPreloader) {
        mIntentDataProvider = intentDataProvider;
        mNightModeController = nightModeController;
        mIntentIgnoringCriterion = intentIgnoringCriterion;
        mStartupTabPreloader = startupTabPreloader;
    }

    @Provides
    public CustomTabIntentDataProvider provideIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Provides
    public ClientAppDataRegister provideClientAppDataRegister() {
        return new ClientAppDataRegister();
    }

    @Provides
    public CustomTabNightModeStateController provideNightModeController() {
        return mNightModeController;
    }

    @Provides
    public CustomTabIntentHandlingStrategy provideIntentHandler(
            Lazy<DefaultCustomTabIntentHandlingStrategy> defaultHandler,
            Lazy<TwaIntentHandlingStrategy> twaHandler) {
        return mIntentDataProvider.isTrustedWebActivity() ? twaHandler.get() : defaultHandler.get();
    }

    @Provides
    public IntentIgnoringCriterion provideIntentIgnoringCriterion() {
        return mIntentIgnoringCriterion;
    }

    @Provides
    @Reusable
    public WebApkPostShareTargetNavigator providePostShareTargetNavigator() {
        return new WebApkPostShareTargetNavigator();
    }

    @Provides
    public StartupTabPreloader provideStartupTabPreloader() {
        return mStartupTabPreloader;
    }
}
