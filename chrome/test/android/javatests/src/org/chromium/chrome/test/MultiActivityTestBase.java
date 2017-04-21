// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.parameter.BaseParameter;
import org.chromium.base.test.util.parameter.Parameter;
import org.chromium.base.test.util.parameter.Parameterizable;
import org.chromium.base.test.util.parameter.parameters.MethodParameter;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;
import org.chromium.chrome.test.util.parameters.AddFakeAccountToAppParameter;
import org.chromium.chrome.test.util.parameters.AddFakeAccountToOsParameter;
import org.chromium.chrome.test.util.parameters.AddGoogleAccountToOsParameter;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.HashMap;
import java.util.Map;

/**
 * Base for testing and interacting with multiple Activities (e.g. Document or Webapp Activities).
 */
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE
        })
public abstract class MultiActivityTestBase extends InstrumentationTestCase
        implements Parameterizable {
    private Parameter.Reader mParameterReader;
    private Map<String, BaseParameter> mAvailableParameters;
    protected MockStorageDelegate mStorageDelegate;
    protected Context mContext;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        RecordHistogram.setDisabledForTests(true);
        mContext = getInstrumentation().getTargetContext();
        CommandLineFlags.setUp(mContext, getClass().getMethod(getName()));
        ApplicationTestUtils.setUp(mContext, true);

        // Make the DocumentTabModelSelector use a mocked out directory so that test runs don't
        // interfere with each other.
        mStorageDelegate = new MockStorageDelegate(mContext.getCacheDir());
        DocumentTabModelSelector.setStorageDelegateForTests(mStorageDelegate);
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        mStorageDelegate.ensureDirectoryDestroyed();
        ApplicationTestUtils.tearDown(mContext);
        RecordHistogram.setDisabledForTests(false);
    }

    /**
     * See {@link #waitForFullLoad(ChromeActivity,String,boolean)}.
     */
    protected void waitForFullLoad(final ChromeActivity activity, final String expectedTitle) {
        waitForFullLoad(activity, expectedTitle, false);
    }

    /**
     * Approximates when a ChromeActivity is fully ready and loaded, which is hard to gauge
     * because Android's Activity transition animations are not monitorable.
     */
    protected void waitForFullLoad(final ChromeActivity activity, final String expectedTitle,
            boolean waitLongerForLoad) {
        ApplicationTestUtils.assertWaitForPageScaleFactorMatch(activity, 0.5f, waitLongerForLoad);
        final Tab tab = activity.getActivityTab();
        assert tab != null;

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!tab.isLoadingAndRenderingDone()) return false;
                if (!TextUtils.equals(expectedTitle, tab.getTitle())) return false;
                return true;
            }
        });
    }

    /**
     * Creates the {@link Map} of available parameters for the test to use.
     *
     * @return a {@link Map} of {@link BaseParameter} objects.
     */
    protected Map<String, BaseParameter> createAvailableParameters() {
        Map<String, BaseParameter> availableParameters = new HashMap<>();
        availableParameters
                .put(MethodParameter.PARAMETER_TAG, new MethodParameter(getParameterReader()));
        availableParameters.put(AddFakeAccountToAppParameter.PARAMETER_TAG,
                new AddFakeAccountToAppParameter(getParameterReader(), getInstrumentation()));
        availableParameters.put(AddFakeAccountToOsParameter.PARAMETER_TAG,
                new AddFakeAccountToOsParameter(getParameterReader(), getInstrumentation()));
        availableParameters.put(AddGoogleAccountToOsParameter.PARAMETER_TAG,
                new AddGoogleAccountToOsParameter(getParameterReader(), getInstrumentation()));
        return availableParameters;
    }

    /**
     * Gets the {@link Map} of available parameters that inherited classes can use.
     *
     * @return a {@link Map} of {@link BaseParameter} objects to set as the available parameters.
     */
    @Override
    public Map<String, BaseParameter> getAvailableParameters() {
        return mAvailableParameters;
    }

    /**
     * Gets a specific parameter from the current test.
     *
     * @param parameterTag a string with the name of the {@link BaseParameter} we want.
     * @return a parameter that extends {@link BaseParameter} that has the matching parameterTag.
     */
    @Override
    @SuppressWarnings("unchecked")
    public <T extends BaseParameter> T getAvailableParameter(String parameterTag) {
        return (T) mAvailableParameters.get(parameterTag);
    }

    /**
     * Setter method for {@link Parameter.Reader}.
     *
     * @param parameterReader the {@link Parameter.Reader} to set.
     */
    @Override
    public void setParameterReader(Parameter.Reader parameterReader) {
        mParameterReader = parameterReader;
        mAvailableParameters = createAvailableParameters();
    }

    /**
     * Getter method for {@link Parameter.Reader} object to be used by test cases reading the
     * parameter.
     *
     * @return the {@link Parameter.Reader} for the current {@link
     * org.chromium.base.test.util.parameter.ParameterizedTest} being run.
     */
    protected Parameter.Reader getParameterReader() {
        return mParameterReader;
    }
}
