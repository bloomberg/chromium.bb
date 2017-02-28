// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.omaha.MockRequestGenerator;
import org.chromium.chrome.test.omaha.MockRequestGenerator.DeviceType;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

/**
 * Tests for the {@link OmahaClient}.
 * Tests override the original OmahaClient's functions with the MockOmahaClient, which
 * provides a way to hook into functions to return values that would normally be provided by the
 * system, such as whether Chrome was installed through the system image.
 */
public class OmahaBaseTest extends InstrumentationTestCase {
    private static class TimestampPair {
        public long timestampNextRequest;
        public long timestampNextPost;

        public TimestampPair(long timestampNextRequest, long timestampNextPost) {
            this.timestampNextRequest = timestampNextRequest;
            this.timestampNextPost = timestampNextPost;
        }
    }

    private static class MockOmahaDelegate extends OmahaDelegate {
        private final List<Integer> mPostResults = new ArrayList<Integer>();
        private final List<Boolean> mGenerateAndPostRequestResults = new ArrayList<Boolean>();

        private final Context mContext;
        private final boolean mIsOnTablet;
        private final boolean mIsInForeground;
        private final boolean mIsInSystemImage;
        private final MockExponentialBackoffScheduler mMockScheduler;
        private MockRequestGenerator mMockGenerator;

        private int mNumUUIDsGenerated;
        private long mNextScheduledTimestamp = -1;

        private boolean mInstallEventWasSent;
        private TimestampPair mTimestampsOnRegisterNewRequest;
        private TimestampPair mTimestampsOnSaveState;

        MockOmahaDelegate(Context context, DeviceType deviceType, InstallSource installSource) {
            mContext = context;
            mIsOnTablet = deviceType == DeviceType.TABLET;
            mIsInForeground = true;
            mIsInSystemImage = installSource == InstallSource.SYSTEM_IMAGE;

            mMockScheduler = new MockExponentialBackoffScheduler(OmahaBase.PREF_PACKAGE, context,
                    OmahaBase.MS_POST_BASE_DELAY, OmahaBase.MS_POST_MAX_DELAY);
        }

        @Override
        protected RequestGenerator createRequestGenerator(Context context) {
            mMockGenerator = new MockRequestGenerator(
                    context, mIsOnTablet ? DeviceType.TABLET : DeviceType.HANDSET);
            return mMockGenerator;
        }

        @Override
        public boolean isInSystemImage() {
            return mIsInSystemImage;
        }

        @Override
        MockExponentialBackoffScheduler getScheduler() {
            return mMockScheduler;
        }

        @Override
        protected String generateUUID() {
            mNumUUIDsGenerated += 1;
            return "UUID" + mNumUUIDsGenerated;
        }

        @Override
        protected boolean isChromeBeingUsed() {
            return mIsInForeground;
        }

        @Override
        void scheduleService(long currentTimestampMs, long nextTimestampMs) {
            mNextScheduledTimestamp = nextTimestampMs;
        }

        @Override
        void onHandlePostRequestDone(int result, boolean installEventWasSent) {
            mPostResults.add(result);
            mInstallEventWasSent = installEventWasSent;
        }

        @Override
        void onRegisterNewRequestDone(long nextRequestTimestamp, long nextPostTimestamp) {
            mTimestampsOnRegisterNewRequest =
                    new TimestampPair(nextRequestTimestamp, nextPostTimestamp);
        }

        @Override
        void onGenerateAndPostRequestDone(boolean result) {
            mGenerateAndPostRequestResults.add(result);
        }

        @Override
        void onSaveStateDone(long nextRequestTimestamp, long nextPostTimestamp) {
            mTimestampsOnSaveState = new TimestampPair(nextRequestTimestamp, nextPostTimestamp);
        }

        @Override
        Context getContext() {
            return mContext;
        }
    }

    private enum InstallSource { SYSTEM_IMAGE, ORGANIC }
    private enum ServerResponse { SUCCESS, FAILURE }
    private enum ConnectionStatus { RESPONDS, TIMES_OUT }
    private enum InstallEvent { SEND, DONT_SEND }
    private enum PostStatus { DUE, NOT_DUE }

    private AdvancedMockContext mContext;
    private MockOmahaDelegate mDelegate;
    private MockOmahaBase mOmahaBase;

    private MockOmahaBase createOmahaBase() {
        return createOmahaBase(
                ServerResponse.SUCCESS, ConnectionStatus.RESPONDS, DeviceType.HANDSET);
    }

    private MockOmahaBase createOmahaBase(
            ServerResponse response, ConnectionStatus status, DeviceType deviceType) {
        MockOmahaBase omahaClient = new MockOmahaBase(mDelegate, response, status, deviceType);
        return omahaClient;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context targetContext = getInstrumentation().getTargetContext();
        OmahaBase.setIsDisabledForTesting(false);
        mContext = new AdvancedMockContext(targetContext);
    }

    @Override
    public void tearDown() throws Exception {
        OmahaBase.setIsDisabledForTesting(true);
        super.tearDown();
    }

    private class MockOmahaBase extends OmahaBase {
        private final LinkedList<MockConnection> mMockConnections = new LinkedList<>();

        private final boolean mSendValidResponse;
        private final boolean mConnectionTimesOut;
        private final boolean mIsOnTablet;

        public MockOmahaBase(OmahaDelegate delegate, ServerResponse serverResponse,
                ConnectionStatus connectionStatus, DeviceType deviceType) {
            super(delegate);
            mSendValidResponse = serverResponse == ServerResponse.SUCCESS;
            mConnectionTimesOut = connectionStatus == ConnectionStatus.TIMES_OUT;
            mIsOnTablet = deviceType == DeviceType.TABLET;
        }

        /**
         * Gets the number of MockConnections created.
         */
        public int getNumConnectionsMade() {
            return mMockConnections.size();
        }

        /**
         * Returns a particular connection.
         */
        public MockConnection getConnection(int index) {
            return mMockConnections.get(index);
        }

        /**
         * Returns the last MockPingConection used to simulate communication with the server.
         */
        public MockConnection getLastConnection() {
            return mMockConnections.getLast();
        }

        public boolean isSendInstallEvent() {
            return mSendInstallEvent;
        }

        public void setSendInstallEvent(boolean state) {
            mSendInstallEvent = state;
        }

        @Override
        protected HttpURLConnection createConnection() throws RequestFailureException {
            MockConnection connection = null;
            try {
                URL url = new URL(mDelegate.getRequestGenerator().getServerUrl());
                connection = new MockConnection(url, mIsOnTablet, mSendValidResponse,
                        mSendInstallEvent, mConnectionTimesOut);
                mMockConnections.addLast(connection);
            } catch (MalformedURLException e) {
                fail("Caught a malformed URL exception: " + e);
            }
            return connection;
        }
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testPipelineFreshInstall() {
        final long now = 11684;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        // Trigger Omaha.
        mOmahaBase = createOmahaBase();
        mOmahaBase.run();

        // A fresh install results in two requests to the Omaha server: one for the install request
        // and one for the ping request.
        assertTrue(mDelegate.mInstallEventWasSent);
        assertEquals(1, mDelegate.mPostResults.size());
        assertEquals(OmahaBase.POST_RESULT_SENT, mDelegate.mPostResults.get(0).intValue());
        assertEquals(2, mDelegate.mGenerateAndPostRequestResults.size());
        assertTrue(mDelegate.mGenerateAndPostRequestResults.get(0));
        assertTrue(mDelegate.mGenerateAndPostRequestResults.get(1));

        // Successful requests mean that the next scheduled event should be checking for when the
        // user is active.
        assertEquals(now + OmahaBase.MS_BETWEEN_REQUESTS, mDelegate.mNextScheduledTimestamp);
        checkTimestamps(now + OmahaBase.MS_BETWEEN_REQUESTS, now + OmahaBase.MS_POST_BASE_DELAY,
                mDelegate.mTimestampsOnSaveState);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testPipelineRegularPing() {
        final long now = 11684;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        // Record that an install event has already been sent and that we're due for a new request.
        SharedPreferences.Editor editor = OmahaBase.getSharedPreferences(mContext).edit();
        editor.putBoolean(OmahaBase.PREF_SEND_INSTALL_EVENT, false);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, now);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, now);
        editor.apply();

        // Trigger Omaha.
        mOmahaBase = createOmahaBase();
        mOmahaBase.run();

        // Only the regular ping should have been sent.
        assertFalse(mDelegate.mInstallEventWasSent);
        assertEquals(1, mDelegate.mPostResults.size());
        assertEquals(OmahaBase.POST_RESULT_SENT, mDelegate.mPostResults.get(0).intValue());
        assertEquals(1, mDelegate.mGenerateAndPostRequestResults.size());
        assertTrue(mDelegate.mGenerateAndPostRequestResults.get(0));

        // Successful requests mean that the next scheduled event should be checking for when the
        // user is active.
        assertEquals(now + OmahaBase.MS_BETWEEN_REQUESTS, mDelegate.mNextScheduledTimestamp);
        checkTimestamps(now + OmahaBase.MS_BETWEEN_REQUESTS, now + OmahaBase.MS_POST_BASE_DELAY,
                mDelegate.mTimestampsOnSaveState);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testTooEarlyToPing() {
        final long now = 0;
        final long later = 10000;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        // Put the time for the next request in the future.
        SharedPreferences prefs = OmahaBase.getSharedPreferences(mContext);
        prefs.edit().putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, later).apply();

        // Trigger Omaha.
        mOmahaBase = createOmahaBase();
        mOmahaBase.run();

        // Nothing should have been POSTed.
        assertEquals(0, mDelegate.mPostResults.size());
        assertEquals(0, mDelegate.mGenerateAndPostRequestResults.size());

        // The next scheduled event is the request generation.  Because there was nothing to POST,
        // its timestamp should have remained unchanged and shouldn't have been considered when the
        // new alarm was scheduled.
        assertEquals(later, mDelegate.mNextScheduledTimestamp);
        checkTimestamps(later, now, mDelegate.mTimestampsOnSaveState);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testTooEarlyToPostExistingRequest() {
        final long timeGeneratedRequest = 0L;
        final long now = 10000L;
        final long timeSendNewPost = 20000L;
        final long timeSendNewRequest = 50000L;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        SharedPreferences prefs = OmahaBase.getSharedPreferences(mContext);
        SharedPreferences.Editor editor = prefs.edit();

        // Make it so that a request was generated and is just waiting to be sent.
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, timeSendNewRequest);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_OF_REQUEST, timeGeneratedRequest);
        editor.putString(OmahaBase.PREF_PERSISTED_REQUEST_ID, "persisted_id");

        // Put the time for the next post in the future.
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, timeSendNewPost);
        editor.apply();

        // Trigger Omaha.
        mOmahaBase = createOmahaBase();
        mOmahaBase.run();

        // Request generation code should be skipped.
        assertNull(mDelegate.mTimestampsOnRegisterNewRequest);

        // Should be too early to post, causing it to be rescheduled.
        assertEquals(1, mDelegate.mPostResults.size());
        assertEquals(OmahaBase.POST_RESULT_SCHEDULED, mDelegate.mPostResults.get(0).intValue());
        assertEquals(0, mDelegate.mGenerateAndPostRequestResults.size());

        // The next scheduled event is the POST.  Because request generation code wasn't run, the
        // timestamp for it shouldn't have changed.
        assertEquals(timeSendNewPost, mDelegate.mNextScheduledTimestamp);
        checkTimestamps(timeSendNewRequest, timeSendNewPost, mDelegate.mTimestampsOnSaveState);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testPostExistingRequestSuccessfully() {
        final long timeGeneratedRequest = 0L;
        final long now = 10000L;
        final long timeSendNewPost = now;
        final long timeRegisterNewRequest = 20000L;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        SharedPreferences prefs = OmahaBase.getSharedPreferences(mContext);
        SharedPreferences.Editor editor = prefs.edit();

        // Make it so that a regular <ping> was generated and is just waiting to be sent.
        editor.putBoolean(OmahaBase.PREF_SEND_INSTALL_EVENT, false);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, timeRegisterNewRequest);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_OF_REQUEST, timeGeneratedRequest);
        editor.putString(OmahaBase.PREF_PERSISTED_REQUEST_ID, "persisted_id");

        // Send the POST now.
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, timeSendNewPost);
        editor.apply();

        // Trigger Omaha.
        mOmahaBase = createOmahaBase();
        mOmahaBase.run();

        // Registering code shouldn't have fired.
        assertNull(mDelegate.mTimestampsOnRegisterNewRequest);

        // Because we didn't send an install event, only one POST should have occurred.
        assertEquals(1, mDelegate.mPostResults.size());
        assertEquals(OmahaBase.POST_RESULT_SENT, mDelegate.mPostResults.get(0).intValue());
        assertEquals(1, mDelegate.mGenerateAndPostRequestResults.size());
        assertTrue(mDelegate.mGenerateAndPostRequestResults.get(0));

        // The next scheduled event is the request generation because there is nothing to POST.
        // A successful POST adjusts all timestamps for the current time.
        assertEquals(timeRegisterNewRequest, mDelegate.mNextScheduledTimestamp);
        checkTimestamps(now + OmahaBase.MS_BETWEEN_REQUESTS, now + OmahaBase.MS_POST_BASE_DELAY,
                mDelegate.mTimestampsOnSaveState);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testPostExistingButFails() {
        final long timeGeneratedRequest = 0L;
        final long now = 10000L;
        final long timeSendNewPost = now;
        final long timeRegisterNewRequest = timeGeneratedRequest + OmahaBase.MS_BETWEEN_REQUESTS;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        SharedPreferences prefs = OmahaBase.getSharedPreferences(mContext);
        SharedPreferences.Editor editor = prefs.edit();

        // Make it so that a regular <ping> was generated and is just waiting to be sent.
        editor.putBoolean(OmahaBase.PREF_SEND_INSTALL_EVENT, false);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, timeRegisterNewRequest);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_OF_REQUEST, timeGeneratedRequest);
        editor.putString(OmahaBase.PREF_PERSISTED_REQUEST_ID, "persisted_id");

        // Send the POST now.
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, timeSendNewPost);
        editor.apply();

        // Trigger Omaha.
        mOmahaBase = createOmahaBase(
                ServerResponse.FAILURE, ConnectionStatus.RESPONDS, DeviceType.HANDSET);
        mOmahaBase.run();

        // Registering code shouldn't have fired.
        assertNull(mDelegate.mTimestampsOnRegisterNewRequest);

        // Because we didn't send an install event, only one POST should have occurred.
        assertEquals(1, mDelegate.mPostResults.size());
        assertEquals(OmahaBase.POST_RESULT_FAILED, mDelegate.mPostResults.get(0).intValue());
        assertEquals(1, mDelegate.mGenerateAndPostRequestResults.size());
        assertFalse(mDelegate.mGenerateAndPostRequestResults.get(0));

        // The next scheduled event should be the POST event, which is delayed by the base delay
        // because no failures have happened yet.
        assertEquals(mDelegate.mTimestampsOnSaveState.timestampNextPost,
                mDelegate.mNextScheduledTimestamp);
        checkTimestamps(timeRegisterNewRequest, now + OmahaBase.MS_POST_BASE_DELAY,
                mDelegate.mTimestampsOnSaveState);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testTimestampWithinBounds() {
        final long now = 0L;
        final long timeRegisterNewRequest = OmahaBase.MS_BETWEEN_REQUESTS + 1;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        SharedPreferences prefs = OmahaBase.getSharedPreferences(mContext);
        SharedPreferences.Editor editor = prefs.edit();

        // Indicate that the next request should be generated way past an expected timeframe.
        editor.putBoolean(OmahaBase.PREF_SEND_INSTALL_EVENT, false);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, timeRegisterNewRequest);
        editor.apply();

        // Trigger Omaha.
        mOmahaBase = createOmahaBase();
        mOmahaBase.run();

        // Request generation code should fire.
        assertNotNull(mDelegate.mTimestampsOnRegisterNewRequest);

        // Because we didn't send an install event, only one POST should have occurred.
        assertEquals(1, mDelegate.mPostResults.size());
        assertEquals(OmahaBase.POST_RESULT_SENT, mDelegate.mPostResults.get(0).intValue());
        assertEquals(1, mDelegate.mGenerateAndPostRequestResults.size());
        assertTrue(mDelegate.mGenerateAndPostRequestResults.get(0));

        // The next scheduled event should be the timestamp for a new request generation.
        assertEquals(mDelegate.mTimestampsOnSaveState.timestampNextRequest,
                mDelegate.mNextScheduledTimestamp);
        checkTimestamps(now + OmahaBase.MS_BETWEEN_REQUESTS, now + OmahaBase.MS_POST_BASE_DELAY,
                mDelegate.mTimestampsOnSaveState);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOverdueRequestCausesNewRegistration() {
        final long timeGeneratedRequest = 0L;
        final long now = 10000L;
        final long timeSendNewPost = now;
        final long timeRegisterNewRequest =
                timeGeneratedRequest + OmahaBase.MS_BETWEEN_REQUESTS * 5;

        mDelegate = new MockOmahaDelegate(mContext, DeviceType.HANDSET, InstallSource.ORGANIC);
        mDelegate.getScheduler().setCurrentTime(now);

        // Record that a regular <ping> was generated, but not sent, then assign it an invalid
        // timestamp and try to send it now.
        SharedPreferences prefs = OmahaBase.getSharedPreferences(mContext);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(OmahaBase.PREF_SEND_INSTALL_EVENT, false);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEW_REQUEST, timeRegisterNewRequest);
        editor.putLong(OmahaBase.PREF_TIMESTAMP_OF_REQUEST, timeGeneratedRequest);
        editor.putString(OmahaBase.PREF_PERSISTED_REQUEST_ID, "persisted_id");
        editor.putLong(OmahaBase.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, timeSendNewPost);
        editor.apply();

        // Trigger Omaha.
        mOmahaBase = createOmahaBase();
        mOmahaBase.run();

        // Registering code shouldn't have fired.
        checkTimestamps(now + OmahaBase.MS_BETWEEN_REQUESTS, now,
                mDelegate.mTimestampsOnRegisterNewRequest);

        // Because we didn't send an install event, only one POST should have occurred.
        assertEquals(1, mDelegate.mPostResults.size());
        assertEquals(OmahaBase.POST_RESULT_SENT, mDelegate.mPostResults.get(0).intValue());
        assertEquals(1, mDelegate.mGenerateAndPostRequestResults.size());
        assertTrue(mDelegate.mGenerateAndPostRequestResults.get(0));

        // The next scheduled event should be the registration event.
        assertEquals(mDelegate.mTimestampsOnSaveState.timestampNextRequest,
                mDelegate.mNextScheduledTimestamp);
        checkTimestamps(now + OmahaBase.MS_BETWEEN_REQUESTS, now + OmahaBase.MS_POST_BASE_DELAY,
                mDelegate.mTimestampsOnSaveState);
    }

    private void checkTimestamps(
            long expectedRequestTimestamp, long expectedPostTimestamp, TimestampPair timestamps) {
        assertEquals(expectedRequestTimestamp, timestamps.timestampNextRequest);
        assertEquals(expectedPostTimestamp, timestamps.timestampNextPost);
    }

    /**
     * Simulates communication with the actual Omaha server.
     */
    private static class MockConnection extends HttpURLConnection {
        // Omaha appends a "/" to the URL.
        private static final String STRIPPED_MARKET_URL =
                "https://market.android.com/details?id=com.google.android.apps.chrome";
        private static final String MARKET_URL = STRIPPED_MARKET_URL + "/";

        private static final String UPDATE_VERSION = "1.2.3.4";

        // Parameters.
        private final boolean mConnectionTimesOut;
        private final ByteArrayInputStream mServerResponse;
        private final ByteArrayOutputStream mOutputStream;
        private final int mHTTPResponseCode;

        // Result variables.
        private int mContentLength;
        private int mNumTimesResponseCodeRetrieved;
        private boolean mSentRequest;
        private boolean mGotInputStream;
        private String mRequestPropertyField;
        private String mRequestPropertyValue;

        MockConnection(URL url, boolean usingTablet, boolean sendValidResponse,
                boolean sendInstallEvent, boolean connectionTimesOut) {
            super(url);
            assertEquals(MockRequestGenerator.SERVER_URL, url.toString());

            String mockResponse = buildServerResponseString(usingTablet, sendInstallEvent);
            mOutputStream = new ByteArrayOutputStream();
            mServerResponse = new ByteArrayInputStream(mockResponse.getBytes());
            mConnectionTimesOut = connectionTimesOut;

            if (sendValidResponse) {
                mHTTPResponseCode = 200;
            } else {
                mHTTPResponseCode = 404;
            }
        }

        /**
         * Build a simulated response from the Omaha server indicating an update is available.
         * The response changes based on the device type.
         */
        private String buildServerResponseString(boolean isOnTablet, boolean sendInstallEvent) {
            String response = "";
            response += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
            response += "<response protocol=\"3.0\" server=\"prod\">";
            response += "<daystart elapsed_seconds=\"12345\"/>";
            response += "<app appid=\"";
            response += (isOnTablet ? MockRequestGenerator.UUID_TABLET
                                    : MockRequestGenerator.UUID_PHONE);
            response += "\" status=\"ok\">";
            if (sendInstallEvent) {
                response += "<event status=\"ok\"/>";
            } else {
                response += "<updatecheck status=\"ok\">";
                response += "<urls><url codebase=\"" + MARKET_URL + "\"/></urls>";
                response += "<manifest version=\"" + UPDATE_VERSION + "\">";
                response += "<packages>";
                response += "<package hash=\"0\" name=\"dummy.apk\" required=\"true\" size=\"0\"/>";
                response += "</packages>";
                response += "<actions>";
                response += "<action event=\"install\" run=\"dummy.apk\"/>";
                response += "<action event=\"postinstall\"/>";
                response += "</actions>";
                response += "</manifest>";
                response += "</updatecheck>";
                response += "<ping status=\"ok\"/>";
            }
            response += "</app>";
            response += "</response>";
            return response;
        }

        @Override
        public boolean usingProxy() {
            return false;
        }

        @Override
        public void connect() throws SocketTimeoutException {
            if (mConnectionTimesOut) {
                throw new SocketTimeoutException("Connection timed out.");
            }
        }

        @Override
        public void disconnect() {}

        @Override
        public void setDoOutput(boolean value) throws IllegalAccessError {
            assertTrue("Told the HTTPUrlConnection to send no request.", value);
        }

        @Override
        public void setFixedLengthStreamingMode(int contentLength) {
            mContentLength = contentLength;
        }

        @Override
        public int getResponseCode() {
            if (mNumTimesResponseCodeRetrieved == 0) {
                // The output stream should now have the generated XML for the request.
                // Check if its length is correct.
                assertEquals("Expected OmahaBase to write out certain number of bytes",
                        mContentLength, mOutputStream.toByteArray().length);
            }
            assertTrue("Tried to retrieve response code more than twice",
                    mNumTimesResponseCodeRetrieved < 2);
            mNumTimesResponseCodeRetrieved++;
            return mHTTPResponseCode;
        }

        @Override
        public OutputStream getOutputStream() throws IOException {
            mSentRequest = true;
            connect();
            return mOutputStream;
        }

        public String getOutputStreamContents() {
            return mOutputStream.toString();
        }

        @Override
        public InputStream getInputStream() {
            assertTrue("Tried to read server response without sending request.", mSentRequest);
            mGotInputStream = true;
            return mServerResponse;
        }

        @Override
        public void addRequestProperty(String field, String newValue) {
            mRequestPropertyField = field;
            mRequestPropertyValue = newValue;
        }

        public int getNumTimesResponseCodeRetrieved() {
            return mNumTimesResponseCodeRetrieved;
        }

        public boolean getGotInputStream() {
            return mGotInputStream;
        }

        public boolean getSentRequest() {
            return mSentRequest;
        }

        public String getRequestPropertyField() {
            return mRequestPropertyField;
        }

        public String getRequestPropertyValue() {
            return mRequestPropertyValue;
        }
    }
}
