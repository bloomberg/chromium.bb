// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import static org.chromium.chrome.test.omaha.MockRequestGenerator.DeviceType;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.omaha.AttributeFinder;
import org.chromium.chrome.test.omaha.MockRequestGenerator;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.util.HashSet;
import java.util.LinkedList;

/**
 * Tests for the {@link OmahaClient}.
 * Tests often override the original OmahaClient's functions with the HookedOmahaClient, which
 * provides a way to hook into functions to return values that would normally be provided by the
 * system, such as whether Chrome was installed through the system image.
 */
public class OmahaClientTest extends InstrumentationTestCase {
    private enum ServerResponse {
        SUCCESS, FAILURE
    }

    private enum ConnectionStatus {
        RESPONDS, TIMES_OUT
    }

    private enum InstallEvent {
        SEND, DONT_SEND
    }

    private enum PostStatus {
        DUE, NOT_DUE
    }

    private MockOmahaContext mContext;
    private HookedOmahaClient mOmahaClient;

    @Override
    protected void setUp() {
        Context targetContext = getInstrumentation().getTargetContext();
        mContext = new MockOmahaContext(targetContext);
        mOmahaClient = HookedOmahaClient.create(mContext);
    }

    /**
     * If a request exists during handleInitialize(), a POST Intent should be fired immediately.
     */
    @SmallTest
    @Feature({"Omaha"})
    public void testInitializeWithRequest() {
        mOmahaClient.registerNewRequest(10);

        Intent intent = OmahaClient.createInitializeIntent(mContext);
        mOmahaClient.onHandleIntent(intent);
        assertTrue("OmahaClient has no registered request", mOmahaClient.hasRequest());
        assertTrue("Alarm does not have the correct state", mOmahaClient.getRequestAlarmWasSet());
        assertTrue("OmahaClient wasn't restarted.", mContext.mOmahaClientRestarted);
        assertNotNull(mContext.mIntentFired);
        assertEquals("POST intent not fired.",
                OmahaClient.createPostRequestIntent(mContext).getAction(),
                mContext.mIntentFired.getAction());
    }

    /**
     * If a request doesn't exist during handleInitialize(), no intent should be fired.
     */
    @SmallTest
    @Feature({"Omaha"})
    public void testInitializeWithoutRequest() {
        Intent intent = OmahaClient.createInitializeIntent(mContext);
        mOmahaClient.onHandleIntent(intent);
        assertFalse("OmahaClient has a registered request", mOmahaClient.hasRequest());
        assertTrue("Alarm does not have the correct state", mOmahaClient.getRequestAlarmWasSet());
        assertFalse("OmahaClient was restarted.", mContext.mOmahaClientRestarted);
        assertNull(mContext.mIntentFired);
    }

    /**
     * Catch situations where the install source isn't set prior to restoring a saved request.
     */
    @SmallTest
    @Feature({"Omaha"})
    public void testInstallSourceSetBeforeRestoringRequest() {
        // Plant a failed request.
        Context targetContext = getInstrumentation().getTargetContext();
        MockOmahaContext mockContext = new MockOmahaContext(targetContext);
        SharedPreferences prefs =
                mockContext.getSharedPreferences(OmahaClient.PREF_PACKAGE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putLong(OmahaClient.PREF_TIMESTAMP_OF_REQUEST, 0);
        editor.putString(OmahaClient.PREF_PERSISTED_REQUEST_ID, "persisted_id");
        editor.apply();

        // Send it off and don't crash when the state is restored and the XML is generated.
        HookedOmahaClient omahaClient = HookedOmahaClient.create(mockContext);
        omahaClient.mMockScheduler.setCurrentTime(1000);
        Intent postIntent = OmahaClient.createPostRequestIntent(mockContext);
        omahaClient.onHandleIntent(postIntent);

        // Check that the request was actually generated and tried to be sent.
        MockConnection connection = omahaClient.getLastConnection();
        assertTrue("Didn't try to make a connection.", connection.getSentRequest());
        assertFalse("OmahaClient still has a registered request", omahaClient.hasRequest());
        assertTrue("Failed to send request", omahaClient.getCumulativeFailedAttempts() == 0);
    }

    /**
     * Makes sure that we don't generate a request if we don't have to.
     */
    @SmallTest
    @Feature({"Omaha", "Main"})
    public void testOmahaClientDoesNotGenerateRequest() {
        // Change the time so the OmahaClient thinks no request is necessary.
        mOmahaClient.mMockScheduler.setCurrentTime(-1000);
        Intent intent = OmahaClient.createRegisterRequestIntent(mContext);
        mOmahaClient.onHandleIntent(intent);
        assertFalse("OmahaClient has a registered request", mOmahaClient.hasRequest());
        assertFalse("OmahaClient was relaunched", mContext.mOmahaClientRestarted);
    }

    /**
     * Makes sure that firing a XML request triggers a post intent.
     */
    @SmallTest
    @Feature({"Omaha", "Main"})
    public void testOmahaClientRequestToPost() {
        // Change the time so the OmahaClient thinks a request is overdue.
        mOmahaClient.mMockScheduler.setCurrentTime(1000);
        Intent intent = OmahaClient.createRegisterRequestIntent(mContext);
        mOmahaClient.onHandleIntent(intent);
        assertTrue("OmahaClient has no registered request", mOmahaClient.hasRequest());
        assertTrue("OmahaClient wasn't relaunched", mContext.mOmahaClientRestarted);
    }

    /**
     * Makes sure that incorrect timestamps are caught.
     */
    @SmallTest
    @Feature({"Omaha"})
    public void testIncorrectDelays() {
        // Set the time to be 2 days past epoch, then generate a request.
        final long millisecondsPerDay = 86400000;
        long currentTimestamp = millisecondsPerDay * 2;
        mOmahaClient.mMockScheduler.setCurrentTime(currentTimestamp);
        mOmahaClient.registerNewRequest(currentTimestamp);

        // Rewind the clock 2 days.
        currentTimestamp -= millisecondsPerDay * 2;
        mOmahaClient.mMockScheduler.setCurrentTime(currentTimestamp);
        mOmahaClient.restoreState();

        // Confirm that the post timestamp was reset, since it's larger than the exponential
        // backoff delay.
        assertEquals("Post timestamp was not cleared.",
                mOmahaClient.getTimestampForNextPostAttempt(), 0);

        // Confirm that the request timestamp was reset, since the next timestamp is more than
        // a day away.
        assertEquals("Request timestamp was not cleared.",
                mOmahaClient.getTimestampForNewRequest(), 0);
    }

    /**
     * Checks that reading and writing out the preferences works.
     */
    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientFileIO() {
        // Register a request and save it to disk.
        mOmahaClient.registerNewRequest(0);

        // The second OmahaCLient should retrieve the request that the first one saved.
        HookedOmahaClient secondClient = HookedOmahaClient.create(mContext);
        assertTrue("The request from the first OmahaClient wasn't written",
                secondClient.hasRequest());
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostHandsetFailure() {
        postRequestToServer(DeviceType.HANDSET, ServerResponse.FAILURE, ConnectionStatus.RESPONDS,
                            InstallEvent.DONT_SEND, PostStatus.DUE);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostHandsetSuccess() {
        postRequestToServer(DeviceType.HANDSET, ServerResponse.SUCCESS, ConnectionStatus.RESPONDS,
                            InstallEvent.DONT_SEND, PostStatus.DUE);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostTabletFailure() {
        postRequestToServer(DeviceType.TABLET, ServerResponse.FAILURE, ConnectionStatus.RESPONDS,
                            InstallEvent.DONT_SEND, PostStatus.DUE);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostTabletSuccess() {
        postRequestToServer(DeviceType.TABLET, ServerResponse.SUCCESS, ConnectionStatus.RESPONDS,
                            InstallEvent.DONT_SEND, PostStatus.DUE);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostHandsetTimeout() {
        postRequestToServer(DeviceType.HANDSET, ServerResponse.FAILURE, ConnectionStatus.TIMES_OUT,
                            InstallEvent.DONT_SEND, PostStatus.DUE);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostInstallEventSuccess() {
        postRequestToServer(DeviceType.HANDSET, ServerResponse.SUCCESS, ConnectionStatus.RESPONDS,
                            InstallEvent.SEND, PostStatus.DUE);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostInstallEventFailure() {
        postRequestToServer(DeviceType.HANDSET, ServerResponse.FAILURE, ConnectionStatus.RESPONDS,
                            InstallEvent.SEND, PostStatus.DUE);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testOmahaClientPostWhenNotDue() {
        postRequestToServer(DeviceType.HANDSET, ServerResponse.FAILURE, ConnectionStatus.RESPONDS,
                            InstallEvent.DONT_SEND, PostStatus.NOT_DUE);
    }

    /**
     * Pretends to post a request to the Omaha server.
     * @param deviceType Whether or not to use an app-ID indicating a tablet.
     * @param response Whether the server acknowledged the request correctly.
     * @param connectionStatus Whether the connection times out when it tries to contact the
     *                         Omaha server.
     * @param installType Whether we're sending an install event or not.
     * @param postStatus Whether we're due for a POST or not.
     */
    public void postRequestToServer(DeviceType deviceType, ServerResponse response,
            ConnectionStatus connectionStatus, InstallEvent installType, PostStatus postStatus) {
        final boolean succeeded = response == ServerResponse.SUCCESS;
        final boolean sentInstallEvent = installType == InstallEvent.SEND;

        HookedOmahaClient omahaClient = new HookedOmahaClient(mContext, deviceType, response,
                connectionStatus, false);
        omahaClient.onCreate();
        omahaClient.restoreState();

        // Set whether or not we're sending the install event.
        assertTrue("Should default to sending install event.", omahaClient.isSendInstallEvent());
        omahaClient.setSendInstallEvent(installType == InstallEvent.SEND);
        omahaClient.registerNewRequest(0);

        // Set up the POST request.
        if (postStatus == PostStatus.NOT_DUE) {
            // Rewind the clock so that we don't send the request yet.
            omahaClient.mMockScheduler.setCurrentTime(-1000);
        }
        Intent postIntent = OmahaClient.createPostRequestIntent(mContext);
        omahaClient.onHandleIntent(postIntent);

        assertTrue("hasRequest() returned wrong value", succeeded != omahaClient.hasRequest());
        if (postStatus == PostStatus.NOT_DUE) {
            // No POST attempt was made.
            assertTrue("POST was attempted and failed.",
                    omahaClient.getCumulativeFailedAttempts() == 0);
            assertTrue("POST alarm wasn't set for reattempt", omahaClient.getPOSTAlarmWasSet());
        } else {
            // Since we start with no failures, the counter incrementing should indicate whether it
            // succeeded or not.
            assertEquals("Expected different outcome", succeeded,
                    omahaClient.getCumulativeFailedAttempts() == 0);
            assertTrue("Alarm state was changed when it shouldn't have been",
                    succeeded != omahaClient.getPOSTAlarmWasSet());

            // If we're sending an install event, we will immediately attempt to send a ping in a
            // follow-up request.
            int numExpectedRequests = succeeded && sentInstallEvent ? 2 : 1;
            assertEquals("Didn't send the correct number of XML requests.", numExpectedRequests,
                            omahaClient.getNumConnectionsMade());

            MockConnection connection = omahaClient.getLastConnection();
            assertEquals("Didn't try to make a connection.", true, connection.getSentRequest());

            if (connectionStatus == ConnectionStatus.TIMES_OUT) {
                // Several events shouldn't happen if the connection times out.
                assertEquals("Retrieved response code when it should have bailed earlier.",
                        0, connection.getNumTimesResponseCodeRetrieved());
                assertFalse("Grabbed input stream when it should have bailed earlier.",
                        connection.getGotInputStream());
            }
        }

        // Check that the latest version and market URLs were saved correctly.
        String expectedVersion = succeeded ? MockConnection.UPDATE_VERSION : "";
        String expectedURL = succeeded ? MockConnection.STRIPPED_MARKET_URL : "";

        // Make sure we properly parsed out the server's response.
        assertEquals("Latest version numbers didn't match", expectedVersion,
                OmahaClient.getVersionNumberGetter().getLatestKnownVersion(
                        mContext, OmahaClient.PREF_PACKAGE, OmahaClient.PREF_LATEST_VERSION));
        assertEquals("Market URL didn't match", expectedURL, OmahaClient.getMarketURL(mContext));

        // Check that the install event was sent properly.
        if (sentInstallEvent) {
            assertFalse("OmahaPingService is going to send another install <event>.",
                    succeeded == omahaClient.isSendInstallEvent());
        }
    }

    /**
     * Test whether we're using request and session IDs properly for POSTs.
     */
    @SmallTest
    @Feature({"Omaha"})
    public void testRequestAndSessionIDs() {
        assertTrue("Should default to sending install event.", mOmahaClient.isSendInstallEvent());

        // Send the POST request.
        mOmahaClient.registerNewRequest(0);
        Intent postIntent = OmahaClient.createPostRequestIntent(mContext);
        mOmahaClient.onHandleIntent(postIntent);

        // If we're sending an install event, we will immediately attempt to send a ping in a
        // follow-up request.  These should have the same session ID, but different request IDs.
        int numRequests = mOmahaClient.getNumConnectionsMade();

        HashSet<String> sessionIDs = new HashSet<String>();
        HashSet<String> requestIDs = new HashSet<String>();
        for (int i = 0; i < numRequests; ++i) {
            String request = mOmahaClient.getConnection(i).getOutputStreamContents();

            String sessionID =
                    new AttributeFinder(request, "request", "sessionid").getValue();
            assertNotNull(sessionID);
            sessionIDs.add(sessionID);

            String requestID =
                    new AttributeFinder(request, "request", "requestid").getValue();
            assertNotNull(requestID);
            requestIDs.add(requestID);
        }
        assertEquals("Session ID was not the same across all requests", 1,
                sessionIDs.size());
        assertEquals("Request ID was duplicated", numRequests, requestIDs.size());

        // Send another XML request and make sure the IDs are all different.
        assertFalse("OmahaPingService is going to send another install <event>.",
                mOmahaClient.isSendInstallEvent());
        mOmahaClient.registerNewRequest(0);
        postIntent = OmahaClient.createPostRequestIntent(mContext);
        mOmahaClient.onHandleIntent(postIntent);

        assertEquals("Didn't send the correct number of XML requests.", numRequests + 1,
                        mOmahaClient.getNumConnectionsMade());
        String newRequest = mOmahaClient.getConnection(numRequests).getOutputStreamContents();

        String newSessionID = new AttributeFinder(newRequest, "request", "sessionid").getValue();
        assertNotNull(newSessionID);
        assertFalse("Session ID was reused.", sessionIDs.contains(newSessionID));

        String newRequestID = new AttributeFinder(newRequest, "request", "requestid").getValue();
        assertNotNull(newRequestID);
        assertFalse("Request ID was reused.", requestIDs.contains(newRequestID));
    }

    /**
     * Checks to see that the header is added only for persisted XML requests.
     */
    @SmallTest
    @Feature({"Omaha"})
    public void testHTTPHeaderForPersistedXMLRequest() {
        final String xml = "<lorem ipsum=\"dolor\" />";
        final long requestTimestamp = 0;
        final long currentTimestamp = 11684;
        final long currentTimestampInSeconds = currentTimestamp / 1000;

        mOmahaClient.registerNewRequest(requestTimestamp);

        assertTrue("Should default to sending install event.", mOmahaClient.isSendInstallEvent());
        assertEquals("Shouldn't have any failed attempts.", 0,
                mOmahaClient.getCumulativeFailedAttempts());

        MockConnection connection = null;
        try {
            mOmahaClient.postRequest(currentTimestamp, xml);
            connection = mOmahaClient.getLastConnection();
        } catch (RequestFailureException e) {
            fail();
        }
        assertEquals("Age property field was unexpectedly added.", null,
                connection.getRequestPropertyField());
        assertEquals("Age property value was unexpectedly set.", null,
                connection.getRequestPropertyValue());

        // Fail once, then check that the header is added.
        mOmahaClient.getBackoffScheduler().increaseFailedAttempts();
        try {
            mOmahaClient.postRequest(currentTimestamp, xml);
            connection = mOmahaClient.getLastConnection();
        } catch (RequestFailureException e) {
            fail();
        }
        assertEquals("Age property field was not added.", "X-RequestAge",
                connection.getRequestPropertyField());
        assertEquals("Age property value was incorrectly set.", currentTimestampInSeconds,
                Long.parseLong(connection.getRequestPropertyValue()));

        // Make sure the header isn't added if we're not sending an install ping.
        mOmahaClient.setSendInstallEvent(false);
        mOmahaClient.registerNewRequest(requestTimestamp);
        mOmahaClient.getBackoffScheduler().increaseFailedAttempts();
        try {
            mOmahaClient.postRequest(currentTimestamp, xml);
            connection = mOmahaClient.getLastConnection();
        } catch (RequestFailureException e) {
            fail();
        }
        assertEquals("Age property field was unexpectedly added.", null,
                connection.getRequestPropertyField());
        assertEquals("Age property value was unexpectedly set.", null,
                connection.getRequestPropertyValue());
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testInstallSource() {
        HookedOmahaClient organicClient = new HookedOmahaClient(mContext, DeviceType.TABLET,
                ServerResponse.SUCCESS, ConnectionStatus.RESPONDS, false);
        String organicInstallSource = organicClient.determineInstallSource();
        assertEquals("Install source should have been treated as organic.",
                OmahaClient.INSTALL_SOURCE_ORGANIC, organicInstallSource);

        HookedOmahaClient systemImageClient = new HookedOmahaClient(mContext, DeviceType.TABLET,
                ServerResponse.SUCCESS, ConnectionStatus.RESPONDS, true);
        String systemImageInstallSource = systemImageClient.determineInstallSource();
        assertEquals("Install source should have been treated as system image.",
                OmahaClient.INSTALL_SOURCE_SYSTEM, systemImageInstallSource);
    }

    /**
     * OmahaClient that overrides simple methods for testing.
     */
    private static class HookedOmahaClient extends OmahaClient {
        private final boolean mIsOnTablet;
        private final boolean mSendValidResponse;
        private final boolean mIsInForeground;
        private final boolean mConnectionTimesOut;
        private final boolean mInstalledOnSystemImage;

        private MockExponentialBackoffScheduler mMockScheduler;
        private RequestGenerator mMockGenerator;
        private final LinkedList<MockConnection> mMockConnections;

        private boolean mRequestAlarmWasSet;
        private int mNumUUIDsGenerated;

        public static HookedOmahaClient create(Context context) {
            HookedOmahaClient omahaClient = new HookedOmahaClient(context, DeviceType.TABLET,
                    ServerResponse.SUCCESS, ConnectionStatus.RESPONDS, false);
            omahaClient.onCreate();
            omahaClient.restoreState();
            return omahaClient;
        }

        public HookedOmahaClient(Context context, DeviceType deviceType,
                    ServerResponse serverResponse, ConnectionStatus connectionStatus,
                    boolean installedOnSystemImage) {
            attachBaseContext(context);
            mIsOnTablet = deviceType == DeviceType.TABLET;
            mSendValidResponse = serverResponse == ServerResponse.SUCCESS;
            mIsInForeground = true;
            mConnectionTimesOut = connectionStatus == ConnectionStatus.TIMES_OUT;
            mMockConnections = new LinkedList<MockConnection>();
            mInstalledOnSystemImage = installedOnSystemImage;
        }

        @Override
        protected boolean isChromeBeingUsed() {
            return mIsInForeground;
        }

        @Override
        public int getApplicationFlags() {
            return mInstalledOnSystemImage ? ApplicationInfo.FLAG_SYSTEM : 0;
        }

        /**
         * Checks if an alarm was set by the backoff scheduler.
         */
        public boolean getPOSTAlarmWasSet() {
            return mMockScheduler.getAlarmWasSet();
        }

        public boolean getRequestAlarmWasSet() {
            return mRequestAlarmWasSet;
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

        /**
         * Mocks out the scheduler so that no alarms are really created.
         */
        @Override
        ExponentialBackoffScheduler createBackoffScheduler(
                String prefPackage, Context context, long base, long max) {
            mMockScheduler =
                    new MockExponentialBackoffScheduler(prefPackage, context, base, max);
            return mMockScheduler;
        }

        @Override
        RequestGenerator createRequestGenerator(Context context) {
            mMockGenerator = new MockRequestGenerator(
                    context, mIsOnTablet ? DeviceType.TABLET : DeviceType.HANDSET);
            return mMockGenerator;
        }

        @Override
        protected HttpURLConnection createConnection() throws RequestFailureException {
            MockConnection connection = null;
            try {
                URL url = new URL(getRequestGenerator().getServerUrl());
                connection = new MockConnection(url, mIsOnTablet, mSendValidResponse,
                        mSendInstallEvent, mConnectionTimesOut);
                mMockConnections.addLast(connection);
            } catch (MalformedURLException e) {
                fail("Caught a malformed URL exception: " + e);
            }
            return connection;
        }

        @Override
        protected void setAlarm(AlarmManager am, PendingIntent operation, int alarmType,
                long triggerAtTime) {
            mRequestAlarmWasSet = true;
        }

        @Override
        protected String generateRandomUUID() {
            mNumUUIDsGenerated += 1;
            return "UUID" + mNumUUIDsGenerated;
        }
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
            response += (isOnTablet
                    ? MockRequestGenerator.UUID_TABLET : MockRequestGenerator.UUID_PHONE);
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
                assertEquals("Expected OmahaClient to write out certain number of bytes",
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

    private static class MockOmahaContext extends AdvancedMockContext {
        private boolean mOmahaClientRestarted;
        private Intent mIntentFired;

        public MockOmahaContext(Context targetContext) {
            super(targetContext);
        }

        @Override
        public ComponentName startService(Intent intent) {
            assertEquals(OmahaClient.class.getCanonicalName(),
                    intent.getComponent().getClassName());
            mOmahaClientRestarted = true;
            mIntentFired = intent;
            return intent.getComponent();
        }
    }
}
