// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNotNull;

import android.os.StrictMode;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.AnnotationRule;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.ui.test.util.UiRestriction;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Utility annotation and rule to enable or disable ChromeHome. Handles setting and resetting the
 * feature flag and the preference.
 *
 * @see ChromeHome.Processor
 * @see FeatureUtilities#isChromeHomeEnabled()
 */
public interface ChromeHome {
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Features.EnableFeatures(ChromeFeatureList.CHROME_HOME)
    @interface Enable {}

    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Features.DisableFeatures(ChromeFeatureList.CHROME_HOME)
    @interface Disable {}

    /**
     * Rule to handle setting and resetting the cached feature state for ChromeHome. Can be used by
     * explicitly calling methods ({@link #setPrefs(boolean)} and {@link #clearTestState()}) or by
     * using the {@link ChromeHome.Enable} and {@link ChromeHome.Disable} annotations on tests.
     */
    class Processor extends AnnotationRule {
        private Boolean mOldState;

        public Processor() {
            super(ChromeHome.Enable.class, ChromeHome.Disable.class);
        }

        @Override
        public Statement apply(Statement base, Description description) {
            Statement wrappedStatement = super.apply(base, description);
            return getAnnotations().isEmpty() ? base : wrappedStatement;
        }

        @Override
        protected void before() throws Throwable {
            boolean featureEnabled = getClosestAnnotation() instanceof ChromeHome.Enable;
            setPrefs(featureEnabled);
        }

        @Override
        protected void after() {
            clearTestState();
        }

        public void setPrefs(boolean enabled) {
            // Chrome relies on a shared preference to determine if the ChromeHome feature is
            // enabled during start up, so we need to manually set the preference to enable
            // ChromeHome before starting Chrome.
            ChromePreferenceManager prefsManager = ChromePreferenceManager.getInstance();
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                mOldState = prefsManager.isChromeHomeEnabled();
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }

            FeatureUtilities.resetChromeHomeEnabledForTests();

            // The native library should not be enabled yet, so we set the preference here and
            // cache it by checking for the feature. Ideally we instead would call
            // FeatureUtilities.cacheChromeHomeEnabled()
            prefsManager.setChromeHomeEnabled(enabled);
            assertEquals(enabled, FeatureUtilities.isChromeHomeEnabled());
        }

        public void clearTestState() {
            assertNotNull(mOldState);
            ChromePreferenceManager.getInstance().setChromeHomeEnabled(mOldState);
        }
    }
}
