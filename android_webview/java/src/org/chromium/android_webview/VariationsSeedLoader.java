// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.SystemClock;

import org.chromium.android_webview.services.IVariationsSeedServer;
import org.chromium.android_webview.services.VariationsSeedServer;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.CachedMetrics.TimesHistogramSample;
import org.chromium.components.variations.LoadSeedResult;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.text.ParseException;
import java.util.Date;
import java.util.Random;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * VariationsSeedLoader asynchronously loads and updates the variations seed. VariationsSeedLoader
 * wraps a Runnable which wraps a FutureTask. The FutureTask loads the seed. The Runnable invokes
 * the FutureTask and then updates the seed if necessary, possibly requesting a new seed from
 * VariationsSeedServer. The reason for splitting the work this way is that WebView startup must
 * block on loading the seed (by using FutureTask.get()) but should not block on the other work done
 * by the Runnable.
 *
 * The Runnable and FutureTask together perform these steps:
 * 1. Pre-load the metrics client ID. This is needed to seed the EntropyProvider. If there is no
 *    client ID, variations can't be used on this run.
 * 2. Load the new seed file, if any.
 * 3. If no new seed file, load the old seed file, if any.
 * 4. Make the loaded seed available via get() (or null if there was no seed).
 * 5. If there was a new seed file, replace the old with the new (but only after making the loaded
 *    seed available, as the replace need not block startup).
 * 6. If there was no seed, or the loaded seed was expired, request a new seed (but don't request
 *    more often than MAX_REQUEST_PERIOD_MILLIS).
 *
 * VariationsSeedLoader should be used during WebView startup like so:
 * 1. Ensure ContextUtils.getApplicationContext(), AwBrowserProcess.getWebViewPackageName(),
 *    CommandLine, and PathUtils are ready to use.
 * 2. As early as possible, call startVariationsInit() to begin the task.
 * 3. Perform any WebView startup tasks which don't require variations to be initialized.
 * 4. Call finishVariationsInit() with the value returned from startVariationsInit(). This will
 *    block for up to SEED_LOAD_TIMEOUT_MILLIS if the task hasn't fininshed loading the seed. If the
 *    seed is loaded on time, variations will be initialized. finishVariationsInit() must be called
 *    before AwFieldTrialCreator::SetUpFieldTrials() runs.
 */
public class VariationsSeedLoader {
    private static final String TAG = "VariationsSeedLoader";

    // The expiration time for an app's copy of the Finch seed, after which we'll still use it,
    // but we'll request a new one from VariationsSeedService.
    private static final long SEED_EXPIRATION_MILLIS = TimeUnit.HOURS.toMillis(6);

    // After requesting a new seed, wait at least this long before making a new request.
    private static final long MAX_REQUEST_PERIOD_MILLIS = TimeUnit.HOURS.toMillis(1);

    // Block in finishVariationsInit() for at most this value waiting for the seed. If the timeout
    // is exceeded, proceed with variations disabled.
    private static final long SEED_LOAD_TIMEOUT_MILLIS = 20;

    // Must match AndroidWebViewVariationsEnableState UMA enum. Do not reorder
    private static final int DEFAULT_ENABLED = 0;
    private static final int CONTROL_DISABLED = 1;
    private static final int EXPERIMENT_ENABLED = 2;
    private static final int ENABLE_STATE_COUNT = 3;
    private static final int ENABLE_STATE_UNINITIALIZED = -1;

    // If false, then variations will be enabled if either the CMD flag or the AGSA experiment file
    // is present. If true, then variations will be enabled regardless of flag or experiment file.
    private static boolean sVariationsAlwaysEnabled = true;

    private SeedLoadAndUpdateRunnable mRunnable;

    // See Android.WebView.VariationsEnableState in histograms.xml for explanation. This setting
    // overrides sVariationsAlwaysEnabled if the latter is true.
    private int mEnableState = ENABLE_STATE_UNINITIALIZED;

    // Generate an EnableState with the following probabilities:
    // 99.8 % - DEFAULT_ENABLED
    //  0.1 % - CONTROL_DISABLED
    //  0.1 % - EXPERIMENT_ENABLED
    // It's difficult to prevent this state from changing across runs without adding IPC or file
    // I/O, which could perturb performance metrics, defeating the purpose of the experiment. So the
    // state is allowed to change on each run. This doesn't break variations, since WebView doesn't
    // support permanent consistency studies anyway.
    private static int chooseEnableState() {
        Random r = new Random();
        int i = r.nextInt(1000);
        if (i == 0) return CONTROL_DISABLED;
        if (i == 1) return EXPERIMENT_ENABLED;
        return DEFAULT_ENABLED;
    }

    private static int chooseAndLogEnableState() {
        int state = chooseEnableState();
        EnumeratedHistogramSample histogram = new EnumeratedHistogramSample(
                "Android.WebView.VariationsEnableState", ENABLE_STATE_COUNT);
        histogram.record(state);
        return state;
    }

    private static void recordLoadSeedResult(int result) {
        EnumeratedHistogramSample histogram = new EnumeratedHistogramSample(
                "Variations.SeedLoadResult", LoadSeedResult.ENUM_SIZE);
        histogram.record(result);
    }

    // AGSA will notify us to enable variations by touching this file.
    // TODO(paulmiller): Remove this after completing the experiment.
    private static boolean checkEnabledByExperiment() {
        File filesDir = ContextUtils.getApplicationContext().getFilesDir();
        File experimentFile = new File(new File(filesDir, "webview"), "finch-exp");
        return experimentFile.exists();
    }

    private static boolean isExpired(long seedFileTime) {
        long expirationTime = seedFileTime + SEED_EXPIRATION_MILLIS;
        long now = (new Date()).getTime();
        return now > expirationTime;
    }

    // Loads our local copy of the seed, if any, and then renames our local copy and/or requests a
    // new seed, if necessary.
    private class SeedLoadAndUpdateRunnable implements Runnable {
        private boolean mEnabledByCmd;

        // mLoadTask will set these to indicate what additional work to do after mLoadTask finishes:
        // - mFoundNewSeed: Is a "new" seed file present? (If so, it should be renamed to an "old"
        //   seed, replacing any existing "old" seed.)
        // - mNeedNewSeed: Should we request a new seed from the service?
        // - mCurrentSeedDate: The "date" field of our local seed, converted to milliseconds since
        //   epoch, or Long.MIN_VALUE if we have no seed.
        // - mEnabledByExperiment: Whether variations enabled by the AGSA experiment. If so, and
        //   variations is not already enabled by CommandLine, then it should be made enabled by
        //   CommandLine. This is volatile because it's set inside mLoadTask on a background thread,
        //   but read in isVariationsEnabled() on the main thread. TODO(paulmiller): Remove this
        //   after completing the experiment.
        private boolean mFoundNewSeed;
        private boolean mNeedNewSeed;
        private long mCurrentSeedDate = Long.MIN_VALUE;
        private volatile boolean mEnabledByExperiment;

        private FutureTask<SeedInfo> mLoadTask = new FutureTask<>(() -> {
            mEnabledByExperiment = checkEnabledByExperiment();
            if (!isVariationsEnabled()) return null;

            AwMetricsServiceClient.preloadClientId();

            File newSeedFile = VariationsUtils.getNewSeedFile();
            File oldSeedFile = VariationsUtils.getSeedFile();

            // The time, in milliseconds since epoch, our local copy of the seed was last written.
            // (Not to be confused with mCurrentSeedDate, the age of the seed as reported by the
            // server.)
            long seedFileTime = 0;

            // First check for a new seed.
            SeedInfo seed = VariationsUtils.readSeedFile(newSeedFile);
            if (seed != null) {
                // If a valid new seed was found, make a note to replace the old seed with
                // the new seed. (Don't do it now, to avoid delaying FutureTask.get().)
                mFoundNewSeed = true;

                seedFileTime = newSeedFile.lastModified();
            } else {
                // If there is no new seed, check for an old seed.
                seed = VariationsUtils.readSeedFile(oldSeedFile);

                if (seed != null) {
                    seedFileTime = oldSeedFile.lastModified();
                }
            }

            // Make a note to request a new seed if necessary. (Don't request it now, to
            // avoid delaying FutureTask.get().)
            if (seed == null || isExpired(seedFileTime)) {
                mNeedNewSeed = true;

                // Rate-limit the requests.
                long lastRequestTime = VariationsUtils.getStampTime();
                if (lastRequestTime != 0) {
                    long now = (new Date()).getTime();
                    if (now < lastRequestTime + MAX_REQUEST_PERIOD_MILLIS) mNeedNewSeed = false;
                }
            }

            // Note the date field of whatever seed was loaded, if any.
            if (seed != null) {
                try {
                    mCurrentSeedDate = seed.parseDate().getTime();
                } catch (ParseException e) {
                    // Should never happen, as date was already verified by readSeedFile.
                    assert false;
                    return null;
                }
            }

            return seed;
        });

        public SeedLoadAndUpdateRunnable(boolean enabledByCmd) {
            mEnabledByCmd = enabledByCmd;
        }

        @Override
        public void run() {
            mLoadTask.run();
            // The loaded seed is now available via get(). The following steps won't block startup.

            if (mFoundNewSeed) {
                // The move happens synchronously. It's not possible for the service to still be
                // writing to this file when we move it, because mFoundNewSeed means we already read
                // the seed and found it to be complete. Therefore the service must have already
                // finished writing.
                VariationsUtils.replaceOldWithNewSeed();
            }

            if (mNeedNewSeed) {
                // The new seed will arrive asynchronously; the new seed file is written by the
                // service, and may complete after this app process has died.
                requestSeedFromService(mCurrentSeedDate);
                VariationsUtils.updateStampTime();
            }

            onBackgroundWorkFinished();
        }

        public SeedInfo get(long timeout, TimeUnit unit)
                throws InterruptedException, ExecutionException, TimeoutException {
            return mLoadTask.get(timeout, unit);
        }

        // mEnabledByExperiment is set in mLoadTask, so isVariationsEnabled() should only be called
        // after run() returns.
        public boolean isVariationsEnabled() {
            assert mEnableState != ENABLE_STATE_UNINITIALIZED;
            return (sVariationsAlwaysEnabled && mEnableState != CONTROL_DISABLED)
                    || mEnabledByCmd || mEnabledByExperiment;
        }
    }

    // Connects to VariationsSeedServer service. Sends a file descriptor for our local copy of the
    // seed to the service, to which the service will write a new seed.
    private class SeedServerConnection implements ServiceConnection {
        private ParcelFileDescriptor mNewSeedFd;
        private long mOldSeedDate;

        public SeedServerConnection(ParcelFileDescriptor newSeedFd, long oldSeedDate) {
            mNewSeedFd = newSeedFd;
            mOldSeedDate = oldSeedDate;
        }

        public void start() {
            try {
                if (!ContextUtils.getApplicationContext()
                        .bindService(getServerIntent(), this, Context.BIND_AUTO_CREATE)) {
                    Log.e(TAG, "Failed to bind to WebView service");
                }
            } catch (NameNotFoundException e) {
                Log.e(TAG, "WebView provider \"" + AwBrowserProcess.getWebViewPackageName() +
                        "\" not found!");
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            try {
                IVariationsSeedServer.Stub.asInterface(service).getSeed(mNewSeedFd, mOldSeedDate);
            } catch (RemoteException e) {
                Log.e(TAG, "Faild requesting seed", e);
            } finally {
                ContextUtils.getApplicationContext().unbindService(this);
                VariationsUtils.closeSafely(mNewSeedFd);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private SeedInfo getSeedBlockingAndLog() {
        long start = SystemClock.elapsedRealtime();
        try {
            try {
                return mRunnable.get(SEED_LOAD_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
            } finally {
                if (mRunnable.isVariationsEnabled()) {
                    long end = SystemClock.elapsedRealtime();
                    TimesHistogramSample histogram = new TimesHistogramSample(
                            "Variations.SeedLoadBlockingTime", TimeUnit.MILLISECONDS);
                    histogram.record(end - start);
                }
            }
        } catch (TimeoutException e) {
            if (mRunnable.isVariationsEnabled()) {
                recordLoadSeedResult(LoadSeedResult.LOAD_TIMED_OUT);
            }
        } catch (InterruptedException e) {
            if (mRunnable.isVariationsEnabled()) {
                recordLoadSeedResult(LoadSeedResult.LOAD_INTERRUPTED);
            }
        } catch (ExecutionException e) {
            if (mRunnable.isVariationsEnabled()) {
                recordLoadSeedResult(LoadSeedResult.LOAD_OTHER_FAILURE);
            }
        }
        Log.e(TAG, "Failed loading variations seed. Variations disabled.");
        return null;
    }

    @VisibleForTesting // Overridden by tests to wait until all work is done.
    protected void onBackgroundWorkFinished() {}

    @VisibleForTesting // and non-static for overriding by tests
    protected boolean isEnabledByCmd() {
        return CommandLine.getInstance().hasSwitch(AwSwitches.ENABLE_WEBVIEW_VARIATIONS);
    }

    @VisibleForTesting // and non-static for overriding by tests
    protected Intent getServerIntent() throws NameNotFoundException {
        Context c = ContextUtils.getApplicationContext()
                .createPackageContext(AwBrowserProcess.getWebViewPackageName(), /*flags=*/0);
        return new Intent(c, VariationsSeedServer.class);
    }

    @VisibleForTesting
    protected void requestSeedFromService(long oldSeedDate) {
        File newSeedFile = VariationsUtils.getNewSeedFile();
        try {
            newSeedFile.createNewFile(); // Silently returns false if already exists.
        } catch (IOException e) {
            Log.e(TAG, "Failed to create seed file " + newSeedFile);
            return;
        }
        ParcelFileDescriptor newSeedFd = null;
        try {
            newSeedFd = ParcelFileDescriptor.open(
                    newSeedFile, ParcelFileDescriptor.MODE_WRITE_ONLY);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Failed to open seed file " + newSeedFile);
            return;
        }

        SeedServerConnection connection = new SeedServerConnection(newSeedFd, oldSeedDate);
        connection.start();
    }

    // Begin asynchronously loading the variations seed. ContextUtils.getApplicationContext() and
    // AwBrowserProcess.getWebViewPackageName() must be ready to use before calling this.
    public void startVariationsInit() {
        assert mEnableState == ENABLE_STATE_UNINITIALIZED;
        mEnableState = chooseAndLogEnableState();
        mRunnable = new SeedLoadAndUpdateRunnable(isEnabledByCmd());
        (new Thread(mRunnable)).start();
    }

    // Block on loading the seed with a timeout. Then if a seed was successfully loaded, initialize
    // variations.
    public void finishVariationsInit() {
        SeedInfo seed = getSeedBlockingAndLog();
        if (seed != null) {
            if (!isEnabledByCmd()) {
                CommandLine.getInstance().appendSwitch(AwSwitches.ENABLE_WEBVIEW_VARIATIONS);
            }
            AwVariationsSeedBridge.setSeed(seed);
        }
    }
}
