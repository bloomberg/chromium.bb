// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CheckDiscard;
import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.Set;

/**
 * A field trial parameter in the variations framework that is cached by {@link CachedFeatureFlags}.
 */
public abstract class CachedFieldTrialParameter {
    /**
     * Data types of field trial parameters.
     */
    @IntDef({FieldTrialParameterType.STRING, FieldTrialParameterType.BOOLEAN,
            FieldTrialParameterType.INT, FieldTrialParameterType.DOUBLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FieldTrialParameterType {
        int STRING = 0;
        int BOOLEAN = 1;
        int INT = 2;
        int DOUBLE = 3;
    }

    @CheckDiscard("crbug.com/1067145")
    private static Set<CachedFieldTrialParameter> sAllInstances;

    private final String mFeatureName;
    private final String mParameterName;
    private final @FieldTrialParameterType int mType;
    private final String mPreferenceKeyOverride;

    CachedFieldTrialParameter(String featureName, String parameterName,
            @FieldTrialParameterType int type, String preferenceKeyOverride) {
        mFeatureName = featureName;
        mParameterName = parameterName;
        mType = type;
        mPreferenceKeyOverride = preferenceKeyOverride;

        registerInstance();
    }

    @RemovableInRelease
    private void registerInstance() {
        if (sAllInstances == null) {
            sAllInstances = new HashSet<>();
        }
        sAllInstances.add(this);
    }

    @CheckDiscard("crbug.com/1067145")
    public static Set<CachedFieldTrialParameter> getAllInstances() {
        return sAllInstances;
    }

    /**
     * @return The name of the related field trial.
     */
    public String getFeatureName() {
        return mFeatureName;
    }

    /**
     * @return The name of the field trial parameter.
     */
    public String getParameterName() {
        return mParameterName;
    }

    /**
     * @return The data type of the field trial parameter.
     */
    public @FieldTrialParameterType int getType() {
        return mType;
    }

    /**
     * @return A human-readable string uniquely identifying the field trial parameter.
     */
    private static String generateFullName(String featureName, String parameterName) {
        return featureName + ":" + parameterName;
    }

    static String generateSharedPreferenceKey(String featureName, String parameterName) {
        return ChromePreferenceKeys.FLAGS_FIELD_TRIAL_PARAM_CACHED.createKey(
                generateFullName(featureName, parameterName));
    }

    /**
     * @return The SharedPreferences key to cache the field trial parameter.
     */
    String getSharedPreferenceKey() {
        if (mPreferenceKeyOverride != null) {
            return mPreferenceKeyOverride;
        }

        return generateSharedPreferenceKey(getFeatureName(), getParameterName());
    }

    /**
     * Get the current value of the parameter and cache it to disk. Calls to getValue() in a
     * future run will return it, if native is not loaded yet.
     */
    abstract void cacheToDisk();

    /**
     * Forces a field trial parameter value for testing. This is only for the annotation processor
     * to use. Tests should use "PARAMETER.setForTesting()" instead.
     */
    @VisibleForTesting
    public static void setForTesting(
            String featureName, String variationName, String stringVariationValue) {
        CachedFeatureFlags.setOverrideTestValue(
                generateSharedPreferenceKey(featureName, variationName), stringVariationValue);
    }
}
