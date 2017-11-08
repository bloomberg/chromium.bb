// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNotNull;

import android.os.StrictMode;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.AnnotationProcessor;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.util.FeatureUtilities;

import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
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
@Inherited
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD, ElementType.TYPE})
public @interface ChromeHome {
    boolean value() default true;

    String FEATURES = ChromeFeatureList.CHROME_HOME;
    String ENABLE_FLAGS = "enable-features=" + FEATURES;

    /**
     * Rule to Handle setting and resetting the feature flag and preference for ChromeHome. Can be
     * used by explicitly calling methods ({@link #setPrefs(boolean)} and {@link #clearTestState()})
     * or by using the {@link ChromeHome} annotation on tests.
     */
    class Processor extends AnnotationProcessor<ChromeHome> {
        private Boolean mOldState;

        public Processor() {
            super(ChromeHome.class);
        }

        @Override
        protected void before() throws Throwable {
            boolean enabled = getRequestedState();
            if (enabled) {
                Features.getInstance().enable(ChromeFeatureList.CHROME_HOME);
            } else {
                Features.getInstance().disable(ChromeFeatureList.CHROME_HOME);
            }
            setPrefs(enabled);
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

        private void updateCommandLine() {
            // TODO(dgn): Possible extra work: detect the flag and combine them with existing ones
            // or support explicitly disabling the feature. Ideally this will be done by refactoring
            // the entire features setting system for tests instead or string manipulation.
            CommandLine commandLine = CommandLine.getInstance();
            String existingFeatures = commandLine.getSwitchValue("enable-features");

            if (TextUtils.equals(existingFeatures, FEATURES)) return;

            if (existingFeatures != null) {
                throw new IllegalStateException("Unable to enable ChromeHome, the feature flag is"
                        + " already set to " + existingFeatures);
            }

            commandLine.appendSwitch(ENABLE_FLAGS);
        }

        private boolean getRequestedState() {
            if (getTestAnnotation() != null) return getTestAnnotation().value();
            if (getClassAnnotation() != null) return getClassAnnotation().value();

            throw new IllegalStateException("Rule called with no annotation");
        }
    }
}
