// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.content_public.browser.WebContents;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.inject.Inject;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;

/**
 * Provides Trusted Web Activity specific behaviour for the {@link CurrentPageVerifier}.
 */
@ActivityScope
public class TwaVerifier implements Verifier, Destroyable {
    /** The Digital Asset Link relationship used for Trusted Web Activities. */
    private static final int RELATIONSHIP = CustomTabsService.RELATION_HANDLE_ALL_URLS;

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final OriginVerifier mOriginVerifier;

    /**
     * Origins that we have yet to call OriginVerifier#start on.
     *
     * This value will be {@code null} until {@link #getPendingOrigins} is called (you can just use
     * getPendingOrigins to get a ensured non-null value).
     */
    @Nullable private Set<Origin> mPendingOrigins;

    /**
     * All the origins that have been successfully verified.
     */
    private Set<Origin> mVerifiedOrigins = new HashSet<>();

    @Inject
    public TwaVerifier(ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider,
            OriginVerifier.Factory originVerifierFactory,
            CustomTabActivityTabProvider tabProvider,
            ClientPackageNameProvider clientPackageNameProvider) {
        mIntentDataProvider = intentDataProvider;

        // TODO(peconn): See if we can get rid of the dependency on Web Contents.
        WebContents webContents =
                tabProvider.getTab() != null ? tabProvider.getTab().getWebContents() : null;
        mOriginVerifier = originVerifierFactory.create(
                clientPackageNameProvider.get(), RELATIONSHIP, webContents);

        lifecycleDispatcher.register(this);
    }

    @Override
    public void destroy() {
        // Verification may finish after activity is destroyed.
        mOriginVerifier.removeListener();
    }

    @Override
    public Promise<Boolean> verify(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return Promise.fulfilled(false);

        Promise<Boolean> promise = new Promise<>();
        if (getPendingOrigins().contains(origin)) {

            mOriginVerifier.start((packageName, unused, verified, online) -> {
                getPendingOrigins().remove(origin);
                if (verified) mVerifiedOrigins.add(origin);

                promise.fulfill(verified);
            }, origin);

        } else {
            promise.fulfill(mOriginVerifier.wasPreviouslyVerified(origin));
        }

        return promise;
    }

    @Override
    public String getVerifiedScope(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return null;
        return origin.toString();
    }

    @Override
    public boolean shouldIgnoreExternalIntentHandlers(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return false;

        return getPendingOrigins().contains(origin) || mVerifiedOrigins.contains(origin);
    }

    @Override
    public boolean wasPreviouslyVerified(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return false;
        return mOriginVerifier.wasPreviouslyVerified(origin);
    }

    private Set<Origin> getPendingOrigins() {
        // mPendingOrigins isn't populated in the constructor because
        // mIntentDataProvider.getUrlToLoad requires native to be loaded.

        if (mPendingOrigins == null) {
            mPendingOrigins = new HashSet<>();

            Origin initialOrigin = Origin.create(mIntentDataProvider.getUrlToLoad());
            if (initialOrigin != null) mPendingOrigins.add(initialOrigin);

            List<String> additionalOrigins =
                    mIntentDataProvider.getTrustedWebActivityAdditionalOrigins();

            if (additionalOrigins != null) {
                for (String originAsString : additionalOrigins) {
                    Origin origin = Origin.create(originAsString);
                    if (origin == null) continue;

                    mPendingOrigins.add(origin);
                }
            }
        }

        return mPendingOrigins;
    }
}
