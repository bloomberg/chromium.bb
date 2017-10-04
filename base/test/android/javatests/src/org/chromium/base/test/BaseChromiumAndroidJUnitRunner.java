// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.app.Application;
import android.app.Instrumentation;
import android.content.Context;
import android.content.pm.InstrumentationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.internal.runner.RunnerArgs;
import android.support.test.internal.runner.TestExecutor;
import android.support.test.internal.runner.TestRequest;
import android.support.test.internal.runner.TestRequestBuilder;
import android.support.test.runner.AndroidJUnitRunner;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;

import java.io.File;
import java.io.IOException;

/**
 * A custom AndroidJUnitRunner that supports multidex installer and list out test information.
 *
 * This class is the equivalent of BaseChromiumInstrumentationTestRunner in JUnit3. Please
 * beware that is this not a class runner. It is declared in test apk AndroidManifest.xml
 * <instrumentation>
 *
 * TODO(yolandyan): remove this class after all tests are converted to JUnit4. Use class runner
 * for test listing.
 */
public class BaseChromiumAndroidJUnitRunner extends AndroidJUnitRunner {
    private static final String LIST_ALL_TESTS_FLAG =
            "org.chromium.base.test.BaseChromiumAndroidJUnitRunner.TestList";
    private static final String LIST_TESTS_PACKAGE_FLAG =
            "org.chromium.base.test.BaseChromiumAndroidJUnitRunner.TestListPackage";
    /**
     * This flag is supported by AndroidJUnitRunner.
     *
     * See the following page for detail
     * https://developer.android.com/reference/android/support/test/runner/AndroidJUnitRunner.html
     */
    private static final String ARGUMENT_TEST_PACKAGE = "package";

    /**
     * The following arguments are corresponding to AndroidJUnitRunner command line arguments.
     * `annotation`: run with only the argument annotation
     * `notAnnotation`: run all tests except the ones with argument annotation
     * `log`: run in log only mode, do not execute tests
     *
     * For more detail, please check
     * https://developer.android.com/reference/android/support/test/runner/AndroidJUnitRunner.html
     */
    private static final String ARGUMENT_ANNOTATION = "annotation";
    private static final String ARGUMENT_NOT_ANNOTATION = "notAnnotation";
    private static final String ARGUMENT_LOG_ONLY = "log";

    private static final String TAG = "BaseJUnitRunner";

    @Override
    public Application newApplication(ClassLoader cl, String className, Context context)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        ChromiumMultiDexInstaller.install(new BaseChromiumRunnerCommon.MultiDexContextWrapper(
                getContext(), getTargetContext()));
        BaseChromiumRunnerCommon.reorderDexPathElements(cl, getContext(), getTargetContext());
        return super.newApplication(cl, className, context);
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
        Bundle arguments = InstrumentationRegistry.getArguments();
        if (arguments != null && arguments.getString(LIST_ALL_TESTS_FLAG) != null) {
            Log.w(TAG,
                    String.format("Runner will list out tests info in JSON without running tests. "
                                    + "Arguments: %s",
                            arguments.toString()));
            listTests(); // Intentionally not calling super.onStart() to avoid additional work.
        } else {
            if (arguments != null && arguments.getString(ARGUMENT_LOG_ONLY) != null) {
                Log.e(TAG,
                        String.format("Runner will log the tests without running tests."
                                        + " If this cause a test run to fail, please report to"
                                        + " crbug.com/754015. Arguments: %s",
                                arguments.toString()));
            }
            super.onStart();
        }
    }

    // TODO(yolandyan): Move this to test harness side once this class gets removed
    private void addTestListPackage(Bundle bundle) {
        PackageManager pm = getContext().getPackageManager();
        InstrumentationInfo info;
        try {
            info = pm.getInstrumentationInfo(getComponentName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            Log.e(TAG, String.format("Could not find component %s", getComponentName()));
            throw new RuntimeException(e);
        }
        Bundle metaDataBundle = info.metaData;
        if (metaDataBundle != null && metaDataBundle.getString(LIST_TESTS_PACKAGE_FLAG) != null) {
            bundle.putString(
                    ARGUMENT_TEST_PACKAGE, metaDataBundle.getString(LIST_TESTS_PACKAGE_FLAG));
        }
    }

    private void listTests() {
        Bundle results = new Bundle();
        TestListInstrumentationRunListener listener = new TestListInstrumentationRunListener();
        try {
            TestExecutor.Builder executorBuilder = new TestExecutor.Builder(this);
            executorBuilder.addRunListener(listener);
            Bundle junit3Arguments = new Bundle(InstrumentationRegistry.getArguments());
            junit3Arguments.putString(ARGUMENT_NOT_ANNOTATION, "org.junit.runner.RunWith");
            addTestListPackage(junit3Arguments);
            TestRequest listJUnit3TestRequest = createListTestRequest(junit3Arguments);
            results = executorBuilder.build().execute(listJUnit3TestRequest);

            Bundle junit4Arguments = new Bundle(InstrumentationRegistry.getArguments());
            junit4Arguments.putString(ARGUMENT_ANNOTATION, "org.junit.runner.RunWith");
            addTestListPackage(junit4Arguments);

            // Do not use Log runner from android test support.
            //
            // Test logging and execution skipping is handled by BaseJUnit4ClassRunner,
            // having ARGUMENT_LOG_ONLY in argument bundle here causes AndroidJUnitRunner
            // to use its own log-only class runner instead of BaseJUnit4ClassRunner.
            junit4Arguments.remove(ARGUMENT_LOG_ONLY);

            TestRequest listJUnit4TestRequest = createListTestRequest(junit4Arguments);
            results.putAll(executorBuilder.build().execute(listJUnit4TestRequest));
            listener.saveTestsToJson(
                    InstrumentationRegistry.getArguments().getString(LIST_ALL_TESTS_FLAG));
        } catch (IOException | RuntimeException e) {
            String msg = "Fatal exception when running tests";
            Log.e(TAG, msg, e);
            // report the exception to instrumentation out
            results.putString(Instrumentation.REPORT_KEY_STREAMRESULT,
                    msg + "\n" + Log.getStackTraceString(e));
        }
        finish(Activity.RESULT_OK, results);
    }

    @SuppressFBWarnings("REC_CATCH_EXCEPTION") // For catch (Exception)
    private TestRequest createListTestRequest(Bundle arguments) {
        RunnerArgs runnerArgs =
                new RunnerArgs.Builder().fromManifest(this).fromBundle(arguments).build();
        TestRequestBuilder builder = new TestRequestBuilder(this, arguments);
        builder.addApkToScan(getContext().getPackageCodePath());

        File[] incrementalJars = null;
        try {
            Class<?> bootstrapClass =
                    Class.forName("org.chromium.incrementalinstall.BootstrapApplication");
            incrementalJars =
                    (File[]) bootstrapClass.getDeclaredField("sIncrementalDexFiles").get(null);
        } catch (Exception e) {
            // Not an incremental build.
        }
        if (incrementalJars != null) {
            for (File f : incrementalJars) {
                builder.addApkToScan(f.getAbsolutePath());
            }
        }
        builder.addFromRunnerArgs(runnerArgs);
        return builder.build();
    }

    static boolean shouldListTests(Bundle arguments) {
        return arguments != null && arguments.getString(LIST_ALL_TESTS_FLAG) != null;
    }
}
