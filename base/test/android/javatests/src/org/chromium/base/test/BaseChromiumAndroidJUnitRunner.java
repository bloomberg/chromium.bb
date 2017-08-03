// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.app.Application;
import android.app.Instrumentation;
import android.content.Context;
import android.os.Bundle;
import android.support.test.internal.runner.RunnerArgs;
import android.support.test.internal.runner.TestExecutor;
import android.support.test.internal.runner.TestRequest;
import android.support.test.internal.runner.TestRequestBuilder;
import android.support.test.runner.AndroidJUnitRunner;

import org.chromium.base.Log;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;

/**
 * A custom AndroidJUnitRunner that supports multidex installer and list out test information.
 *
 * This class is the equivalent of BaseChromiumInstrumentationTestRunner in JUnit3. Please
 * beware that is this not a class runner. It is declared in test apk AndroidManifest.xml
 * <instrumentation>
 */
public class BaseChromiumAndroidJUnitRunner extends AndroidJUnitRunner {
    private static final String LIST_ALL_TESTS_FLAG =
            "org.chromium.base.test.BaseChromiumAndroidJUnitRunner.TestList";
    private static final String TAG = "BaseJUnitRunner";

    private Bundle mArguments;

    @Override
    public Application newApplication(ClassLoader cl, String className, Context context)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        ChromiumMultiDexInstaller.install(new BaseChromiumRunnerCommon.MultiDexContextWrapper(
                getContext(), getTargetContext()));
        BaseChromiumRunnerCommon.reorderDexPathElements(cl, getContext(), getTargetContext());
        return super.newApplication(cl, className, context);
    }

    @Override
    public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
        mArguments = arguments;
    }

    /**
     * Add TestListInstrumentationRunListener when argument ask the runner to list tests info.
     *
     * The running mechanism when argument has "listAllTests" is equivalent to that of
     * {@link android.support.test.runner.AndroidJUnitRunner#onStart()} except it adds
     * only TestListInstrumentationRunListener to monitor the tests.
     */
    @Override
    public void onStart() {
        if (mArguments != null && mArguments.getString(LIST_ALL_TESTS_FLAG) != null) {
            Log.w(TAG, "Runner will list out tests info in JSON without running tests");
            listTests(); // Intentionally not calling super.onStart() to avoid additional work.
        } else {
            super.onStart();
        }
    }

    private void listTests() {
        Bundle results = new Bundle();
        try {
            TestExecutor.Builder executorBuilder = new TestExecutor.Builder(this);
            executorBuilder.addRunListener(new TestListInstrumentationRunListener(
                    mArguments.getString(LIST_ALL_TESTS_FLAG)));
            TestRequest listTestRequest = createListTestRequest(mArguments);
            results = executorBuilder.build().execute(listTestRequest);
        } catch (RuntimeException e) {
            String msg = "Fatal exception when running tests";
            Log.e(TAG, msg, e);
            // report the exception to instrumentation out
            results.putString(Instrumentation.REPORT_KEY_STREAMRESULT,
                    msg + "\n" + Log.getStackTraceString(e));
        }
        finish(Activity.RESULT_OK, results);
    }

    private TestRequest createListTestRequest(Bundle arguments) {
        RunnerArgs runnerArgs =
                new RunnerArgs.Builder().fromManifest(this).fromBundle(arguments).build();
        TestRequestBuilder builder = new TestRequestBuilder(this, arguments);
        builder.addApkToScan(getContext().getPackageCodePath());
        builder.addFromRunnerArgs(runnerArgs);
        return builder.build();
    }
}
