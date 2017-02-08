// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.app.AlarmManager;
import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Looper;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeApplication;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

/**
 * Keeps tabs on the current state of Chrome, tracking if and when a request should be sent to the
 * Omaha Server.
 *
 * A hook in ChromeActivity's doDeferredResume() initializes the service.  Further attempts to
 * reschedule events will be scheduled by the class itself.
 *
 * Each request to the server will perform an update check and ping the server.
 * We use a repeating alarm to schedule the XML requests to be generated 5 hours apart.
 * If Chrome isn't running when the alarm is fired, the request generation will be stalled until
 * the next time Chrome runs.
 *
 * mevissen suggested being conservative with our timers for sending requests.
 * POST attempts that fail to be acknowledged by the server are re-attempted, with at least
 * one hour between each attempt.
 *
 * Status is saved directly to the the disk after every operation.  Unit tests testing the code
 * paths without using Intents may need to call restoreState() manually as it is not automatically
 * handled in onCreate().
 *
 * Implementation notes:
 * http://docs.google.com/a/google.com/document/d/1scTCovqASf5ktkOeVj8wFRkWTCeDYw2LrOBNn05CDB0/edit
 *
 * NOTE: This class can never be renamed because the user may have Intents floating around that
 *       reference this class specifically.
 */
public class OmahaClient extends IntentService {
    // Results of {@link #handlePostRequest()}.
    static final int POST_RESULT_NO_REQUEST = 0;
    static final int POST_RESULT_SENT = 1;
    static final int POST_RESULT_FAILED = 2;
    static final int POST_RESULT_SCHEDULED = 3;

    private static final String TAG = "omaha";

    // Intent actions.
    private static final String ACTION_INITIALIZE =
            "org.chromium.chrome.browser.omaha.ACTION_INITIALIZE";
    private static final String ACTION_REGISTER_REQUEST =
            "org.chromium.chrome.browser.omaha.ACTION_REGISTER_REQUEST";
    private static final String ACTION_POST_REQUEST =
            "org.chromium.chrome.browser.omaha.ACTION_POST_REQUEST";

    // Delays between events.
    private static final long MS_POST_BASE_DELAY = TimeUnit.HOURS.toMillis(1);
    private static final long MS_POST_MAX_DELAY = TimeUnit.HOURS.toMillis(5);
    static final long MS_BETWEEN_REQUESTS = TimeUnit.HOURS.toMillis(5);
    private static final int MS_CONNECTION_TIMEOUT = (int) TimeUnit.MINUTES.toMillis(1);

    // Strings indicating how the Chrome APK arrived on the user's device. These values MUST NOT
    // be changed without updating the corresponding Omaha server strings.
    static final String INSTALL_SOURCE_SYSTEM = "system_image";
    static final String INSTALL_SOURCE_ORGANIC = "organic";

    private static final long INVALID_TIMESTAMP = -1;
    @VisibleForTesting
    static final String INVALID_REQUEST_ID = "invalid";

    // Member fields not persisted to disk.
    private boolean mStateHasBeenRestored;
    private ExponentialBackoffScheduler mBackoffScheduler;
    private RequestGenerator mGenerator;

    // State saved written to and read from disk.
    private RequestData mCurrentRequest;
    private long mTimestampOfInstall;
    private long mTimestampForNextPostAttempt;
    private long mTimestampForNewRequest;
    private String mLatestVersion;
    private String mMarketURL;
    private String mInstallSource;
    protected boolean mSendInstallEvent;

    public OmahaClient() {
        super(TAG);
        setIntentRedelivery(true);
    }

    @VisibleForTesting
    long getTimestampForNextPostAttempt() {
        return mTimestampForNextPostAttempt;
    }

    @VisibleForTesting
    long getTimestampForNewRequest() {
        return mTimestampForNewRequest;
    }

    @VisibleForTesting
    int getCumulativeFailedAttempts() {
        return getBackoffScheduler().getNumFailedAttempts();
    }

    /**
     * Creates the scheduler used to space out POST attempts.
     */
    @VisibleForTesting
    ExponentialBackoffScheduler createBackoffScheduler(String prefPackage, Context context,
            long base, long max) {
        return new ExponentialBackoffScheduler(prefPackage, context, base, max);
    }

    /**
     * Creates the request generator used to create Omaha XML.
     */
    @VisibleForTesting
    RequestGenerator createRequestGenerator(Context context) {
        return ((ChromeApplication) context.getApplicationContext()).createOmahaRequestGenerator();
    }

    /**
     * Handles an action on a thread separate from the UI thread.
     * @param intent Intent fired by some part of Chrome.
     */
    @Override
    public void onHandleIntent(Intent intent) {
        assert Looper.myLooper() != Looper.getMainLooper();

        if (OmahaBase.isDisabled() || Build.VERSION.SDK_INT > Build.VERSION_CODES.N
                || getRequestGenerator() == null) {
            Log.v(TAG, "Disabled.  Ignoring intent.");
            return;
        }

        restoreState(this);

        if (ACTION_INITIALIZE.equals(intent.getAction())) {
            handleInitialize();
        } else if (ACTION_REGISTER_REQUEST.equals(intent.getAction())) {
            handleRegisterRequest();
        } else if (ACTION_POST_REQUEST.equals(intent.getAction())) {
            handlePostRequest();
        } else {
            Log.e(TAG, "Got unknown action from intent: " + intent.getAction());
        }

        saveState(this);
    }

    /**
     * Begin communicating with the Omaha Update Server.
     */
    static void startService(Context context) {
        Intent omahaIntent = createInitializeIntent(context);
        context.startService(omahaIntent);
    }

    static Intent createInitializeIntent(Context context) {
        Intent intent = new Intent(context, OmahaClient.class);
        intent.setAction(ACTION_INITIALIZE);
        return intent;
    }

    /**
     * Start a recurring alarm to fire request generation intents.
     */
    private void handleInitialize() {
        scheduleActiveUserCheck();

        // If a request exists, kick off POSTing it to the server immediately.
        if (hasRequest()) handlePostRequest();
    }

    /**
     * Returns an Intent for registering a new request to send to the server.
     */
    static Intent createRegisterRequestIntent(Context context) {
        Intent intent = new Intent(context, OmahaClient.class);
        intent.setAction(ACTION_REGISTER_REQUEST);
        return intent;
    }

    /**
     * Determines if a new request should be generated.  New requests are only generated if enough
     * time has passed between now and the last time a request was generated.
     */
    private void handleRegisterRequest() {
        if (!isChromeBeingUsed()) {
            getBackoffScheduler().cancelAlarm(createRegisterRequestIntent(this));
            return;
        }

        // If the current request is too old, generate a new one.
        long currentTimestamp = getBackoffScheduler().getCurrentTime();
        boolean isTooOld = hasRequest()
                && mCurrentRequest.getAgeInMilliseconds(currentTimestamp) >= MS_BETWEEN_REQUESTS;
        boolean isOverdue = !hasRequest() && currentTimestamp >= mTimestampForNewRequest;
        if (isTooOld || isOverdue) {
            registerNewRequest(currentTimestamp);
        }

        // Send the request.
        if (hasRequest()) {
            handlePostRequest();
        }
    }

    /**
     * Returns an Intent for POSTing the current request to the Omaha server.
     */
    static Intent createPostRequestIntent(Context context) {
        Intent intent = new Intent(context, OmahaClient.class);
        intent.setAction(ACTION_POST_REQUEST);
        return intent;
    }

    /**
     * Sends the request it is holding.
     */
    @VisibleForTesting
    protected int handlePostRequest() {
        if (!hasRequest()) return POST_RESULT_NO_REQUEST;

        // If enough time has passed since the last attempt, try sending a request.
        int result;
        long currentTimestamp = getBackoffScheduler().getCurrentTime();
        if (currentTimestamp >= mTimestampForNextPostAttempt) {
            // All requests made during the same session should have the same ID.
            String sessionID = generateRandomUUID();
            boolean sendingInstallRequest = mSendInstallEvent;
            boolean succeeded = generateAndPostRequest(currentTimestamp, sessionID);

            if (succeeded && sendingInstallRequest) {
                // Only the first request ever generated should contain an install event.
                mSendInstallEvent = false;

                // Create and immediately send another request for a ping and update check.
                registerNewRequest(currentTimestamp);
                generateAndPostRequest(currentTimestamp, sessionID);
            }

            result = succeeded ? POST_RESULT_SENT : POST_RESULT_FAILED;
        } else {
            schedulePost();
            result = POST_RESULT_SCHEDULED;
        }

        return result;
    }

    /** Set an alarm to POST at the proper time.  Previous alarms are destroyed. */
    private void schedulePost() {
        Intent postIntent = createPostRequestIntent(this);
        getBackoffScheduler().createAlarm(postIntent, mTimestampForNextPostAttempt);
    }

    private boolean generateAndPostRequest(long currentTimestamp, String sessionID) {
        ExponentialBackoffScheduler scheduler = getBackoffScheduler();
        try {
            // Generate the XML for the current request.
            long installAgeInDays = RequestGenerator.installAge(currentTimestamp,
                    mTimestampOfInstall, mCurrentRequest.isSendInstallEvent());
            String version = VersionNumberGetter.getInstance().getCurrentlyUsedVersion(this);
            String xml = getRequestGenerator().generateXML(
                    sessionID, version, installAgeInDays, mCurrentRequest);

            // Send the request to the server & wait for a response.
            String response = postRequest(currentTimestamp, xml);

            // Parse out the response.
            String appId = getRequestGenerator().getAppId();
            boolean sentPingAndUpdate = !mSendInstallEvent;
            ResponseParser parser = new ResponseParser(
                    appId, mSendInstallEvent, sentPingAndUpdate, sentPingAndUpdate);
            parser.parseResponse(response);
            mLatestVersion = parser.getNewVersion();
            mMarketURL = parser.getURL();

            // If we've gotten this far, we've successfully sent a request.
            mCurrentRequest = null;

            mTimestampForNewRequest = getBackoffScheduler().getCurrentTime() + MS_BETWEEN_REQUESTS;
            scheduleActiveUserCheck();

            scheduler.resetFailedAttempts();
            mTimestampForNextPostAttempt = scheduler.calculateNextTimestamp();
            Log.i(TAG, "Request to Server Successful. Timestamp for next request:"
                    + String.valueOf(mTimestampForNextPostAttempt));

            return true;
        } catch (RequestFailureException e) {
            // Set the alarm to try again later.
            Log.e(TAG, "Failed to contact server: ", e);
            scheduler.increaseFailedAttempts();
            mTimestampForNextPostAttempt = scheduler.calculateNextTimestamp();
            scheduler.createAlarm(createPostRequestIntent(this), mTimestampForNextPostAttempt);
            return false;
        }
    }

    /**
     * Sets a repeating alarm that fires request registration Intents.
     * Setting the alarm overwrites whatever alarm is already there, and rebooting
     * clears whatever alarms are currently set.
     */
    private void scheduleActiveUserCheck() {
        Intent registerIntent = createRegisterRequestIntent(this);
        PendingIntent pIntent = PendingIntent.getService(this, 0, registerIntent, 0);
        AlarmManager am = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
        setAlarm(am, pIntent, mTimestampForNewRequest);
    }

    /**
     * Sets up a timer to fire after each interval.
     * Override to prevent a real alarm from being set.
     */
    @VisibleForTesting
    protected void setAlarm(AlarmManager am, PendingIntent operation, long triggerAtTime) {
        try {
            am.setRepeating(AlarmManager.RTC, triggerAtTime, MS_BETWEEN_REQUESTS, operation);
        } catch (SecurityException e) {
            Log.e(TAG, "Failed to set repeating alarm.");
        }
    }

    /**
     * Determine whether or not Chrome is currently being used actively.
     */
    @VisibleForTesting
    protected boolean isChromeBeingUsed() {
        boolean isChromeVisible = ApplicationStatus.hasVisibleActivities();
        boolean isScreenOn = ApiCompatibilityUtils.isInteractive(this);
        return isChromeVisible && isScreenOn;
    }

    /**
     * Registers a new request with the current timestamp.  Internal timestamps are reset to start
     * fresh.
     * @param currentTimestamp Current time.
     */
    @VisibleForTesting
    void registerNewRequest(long currentTimestamp) {
        mCurrentRequest = createRequestData(currentTimestamp, null);
        getBackoffScheduler().resetFailedAttempts();
        mTimestampForNextPostAttempt = currentTimestamp;

        // Tentatively set the timestamp for a new request.  This will be updated when the server
        // is successfully contacted.
        mTimestampForNewRequest = currentTimestamp + MS_BETWEEN_REQUESTS;
        scheduleActiveUserCheck();
    }

    private RequestData createRequestData(long currentTimestamp, String persistedID) {
        // If we're sending a persisted event, keep trying to send the same request ID.
        String requestID;
        if (persistedID == null || INVALID_REQUEST_ID.equals(persistedID)) {
            requestID = generateRandomUUID();
        } else {
            requestID = persistedID;
        }
        return new RequestData(mSendInstallEvent, currentTimestamp, requestID, mInstallSource);
    }

    @VisibleForTesting
    boolean hasRequest() {
        return mCurrentRequest != null;
    }

    /**
     * Posts the request to the Omaha server.
     * @return the XML response as a String.
     * @throws RequestFailureException if the request fails.
     */
    @VisibleForTesting
    String postRequest(long timestamp, String xml) throws RequestFailureException {
        String response = null;

        HttpURLConnection urlConnection = null;
        try {
            urlConnection = createConnection();

            // Prepare the HTTP header.
            urlConnection.setDoOutput(true);
            urlConnection.setFixedLengthStreamingMode(xml.getBytes().length);
            if (mSendInstallEvent && getCumulativeFailedAttempts() > 0) {
                String age = Long.toString(mCurrentRequest.getAgeInSeconds(timestamp));
                urlConnection.addRequestProperty("X-RequestAge", age);
            }

            response = OmahaBase.sendRequestToServer(urlConnection, xml);
        } catch (IllegalAccessError e) {
            throw new RequestFailureException("Caught an IllegalAccessError:", e);
        } catch (IllegalArgumentException e) {
            throw new RequestFailureException("Caught an IllegalArgumentException:", e);
        } catch (IllegalStateException e) {
            throw new RequestFailureException("Caught an IllegalStateException:", e);
        } finally {
            if (urlConnection != null) {
                urlConnection.disconnect();
            }
        }

        return response;
    }

    /**
     * Returns a HttpURLConnection to the server.
     */
    @VisibleForTesting
    protected HttpURLConnection createConnection() throws RequestFailureException {
        try {
            URL url = new URL(getRequestGenerator().getServerUrl());
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(MS_CONNECTION_TIMEOUT);
            connection.setReadTimeout(MS_CONNECTION_TIMEOUT);
            return connection;
        } catch (MalformedURLException e) {
            throw new RequestFailureException("Caught a malformed URL exception.", e);
        } catch (IOException e) {
            throw new RequestFailureException("Failed to open connection to URL", e);
        }
    }

    /**
     * Determine how the Chrome APK arrived on the device.
     * @param context Context to pull resources from.
     * @return A String indicating the install source.
     */
    String determineInstallSource() {
        boolean isInSystemImage = (getApplicationFlags() & ApplicationInfo.FLAG_SYSTEM) != 0;
        return isInSystemImage ? INSTALL_SOURCE_SYSTEM : INSTALL_SOURCE_ORGANIC;
    }

    /**
     * Returns the Application's flags, used to determine if Chrome was installed as part of the
     * system image.
     * @return The Application's flags.
     */
    @VisibleForTesting
    int getApplicationFlags() {
        return getApplicationInfo().flags;
    }

    /**
     * Reads the data back from the file it was saved to.  Uses SharedPreferences to handle I/O.
     * Sanity checks are performed on the timestamps to guard against clock changing.
     */
    @VisibleForTesting
    void restoreState(Context context) {
        if (mStateHasBeenRestored) return;
        long currentTime = getBackoffScheduler().getCurrentTime();

        SharedPreferences preferences = OmahaBase.getSharedPreferences(context);
        mTimestampForNewRequest =
                preferences.getLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, currentTime);
        mTimestampForNextPostAttempt =
                preferences.getLong(OmahaBase.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, currentTime);
        mTimestampOfInstall = preferences.getLong(OmahaBase.PREF_TIMESTAMP_OF_INSTALL, currentTime);
        mSendInstallEvent = preferences.getBoolean(OmahaBase.PREF_SEND_INSTALL_EVENT, true);
        mInstallSource =
                preferences.getString(OmahaBase.PREF_INSTALL_SOURCE, determineInstallSource());
        mLatestVersion = preferences.getString(OmahaBase.PREF_LATEST_VERSION, "");
        mMarketURL = preferences.getString(OmahaBase.PREF_MARKET_URL, "");

        // If we're not sending an install event, don't bother restoring the request ID:
        // the server does not expect to have persisted request IDs for pings or update checks.
        String persistedRequestId = mSendInstallEvent
                ? preferences.getString(OmahaBase.PREF_PERSISTED_REQUEST_ID, INVALID_REQUEST_ID)
                : INVALID_REQUEST_ID;
        long requestTimestamp =
                preferences.getLong(OmahaBase.PREF_TIMESTAMP_OF_REQUEST, INVALID_TIMESTAMP);
        mCurrentRequest = requestTimestamp == INVALID_TIMESTAMP
                ? null : createRequestData(requestTimestamp, persistedRequestId);

        // Confirm that the timestamp for the next request is less than the base delay.
        long delayToNewRequest = mTimestampForNewRequest - currentTime;
        if (delayToNewRequest > MS_BETWEEN_REQUESTS) {
            Log.w(TAG, "Delay to next request (" + delayToNewRequest
                    + ") is longer than expected.  Resetting to now.");
            mTimestampForNewRequest = currentTime;
        }

        // Confirm that the timestamp for the next POST is less than the current delay.
        long delayToNextPost = mTimestampForNextPostAttempt - currentTime;
        if (delayToNextPost > getBackoffScheduler().getGeneratedDelay()) {
            Log.w(TAG, "Delay to next post attempt (" + delayToNextPost
                    + ") is greater than expected (" + getBackoffScheduler().getGeneratedDelay()
                    + ").  Resetting to now.");
            mTimestampForNextPostAttempt = currentTime;
        }

        mStateHasBeenRestored = true;
    }

    /**
     * Writes out the current state to a file.
     */
    private void saveState(Context context) {
        SharedPreferences prefs = OmahaBase.getSharedPreferences(context);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(OmahaBase.PREF_SEND_INSTALL_EVENT, mSendInstallEvent);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_OF_INSTALL, mTimestampOfInstall);
        editor.putLong(
                OmahaBase.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, mTimestampForNextPostAttempt);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, mTimestampForNewRequest);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_OF_REQUEST,
                hasRequest() ? mCurrentRequest.getCreationTimestamp() : INVALID_TIMESTAMP);
        editor.putString(OmahaBase.PREF_PERSISTED_REQUEST_ID,
                hasRequest() ? mCurrentRequest.getRequestID() : INVALID_REQUEST_ID);
        editor.putString(
                OmahaBase.PREF_LATEST_VERSION, mLatestVersion == null ? "" : mLatestVersion);
        editor.putString(OmahaBase.PREF_MARKET_URL, mMarketURL == null ? "" : mMarketURL);
        editor.putString(OmahaBase.PREF_INSTALL_SOURCE, mInstallSource);
        editor.apply();
    }

    /**
     * Generates a random UUID.
     */
    @VisibleForTesting
    protected String generateRandomUUID() {
        return UUID.randomUUID().toString();
    }

    protected final RequestGenerator getRequestGenerator() {
        if (mGenerator == null) mGenerator = createRequestGenerator(this);
        return mGenerator;
    }

    protected final ExponentialBackoffScheduler getBackoffScheduler() {
        if (mBackoffScheduler == null) {
            mBackoffScheduler = createBackoffScheduler(
                    OmahaBase.PREF_PACKAGE, this, MS_POST_BASE_DELAY, MS_POST_MAX_DELAY);
        }
        return mBackoffScheduler;
    }
}
