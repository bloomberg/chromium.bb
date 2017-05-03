// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.test.filters.SmallTest;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;


/**
 * Tests for launching Chrome.
 */
@RetryOnFailure
public class LauncherActivityTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private Context mContext;
    private static final long DEVICE_STARTUP_TIMEOUT_MS = scaleTimeout(15000);

    public LauncherActivityTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
    }

    @SmallTest
    public void testLaunchWithUrlNoScheme() {
        // Prepare intent
        final String intentUrl = "www.google.com";
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentUrl));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        final Activity startedActivity = tryLaunchingChrome(intent);
        final Intent activityIntent = startedActivity.getIntent();
        assertEquals(intentUrl, activityIntent.getDataString());
    }

    @SmallTest
    public void testDoesNotCrashWithBadParcel() {
        // Prepare bad intent
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.google.com"));
        final Parcel parcel = Parcel.obtain();
        // Force unparcelling within ChromeLauncherActivity. Writing and reading from a parcel will
        // simulate being parcelled by another application, and thus cause unmarshalling when
        // Chrome tries reading an extra the next time.
        intent.writeToParcel(parcel, 0);
        intent.readFromParcel(parcel);
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra("BadExtra", new InvalidParcelable());

        final Activity startedActivity = tryLaunchingChrome(intent);
        final Intent activityIntent = startedActivity.getIntent();
        assertEquals("Data was not preserved", intent.getData(), activityIntent.getData());
        assertEquals("Action was not preserved", intent.getAction(), activityIntent.getAction());
    }

    private Activity tryLaunchingChrome(final Intent intent) {
        mContext.startActivity(intent);

        // Check that ChromeLauncher Activity successfully launched
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(ApplicationState.HAS_RUNNING_ACTIVITIES, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return ApplicationStatus.getStateForApplication();
                    }
                }));

        // Check that Chrome proper was successfully launched as a follow-up
        final AtomicReference<Activity> launchedActivity = new AtomicReference<>();
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("ChromeLauncherActivity did not start Chrome") {
                    @Override
                    public boolean isSatisfied() {
                        final List<WeakReference<Activity>> references =
                                ApplicationStatus.getRunningActivities();
                        if (references.size() != 1) return false;
                        launchedActivity.set(references.get(0).get());
                        return launchedActivity.get() instanceof ChromeActivity;
                    }
                }, DEVICE_STARTUP_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return launchedActivity.get();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
    }

    /**
     * This Parcelable does not adhere to the form standards of a well formed Parcelable and will
     * thus cause a BadParcelableException.  The lint suppression is needed since it detects that
     * this will throw a BadParcelableException.
     */
    @SuppressLint("ParcelCreator")
    private static class InvalidParcelable implements Parcelable {
        @Override
        public void writeToParcel(Parcel parcel, int params) {
        }

        @Override
        public int describeContents() {
            return 0;
        }
    }
}
