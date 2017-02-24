// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Dialog;
import android.os.StrictMode;
import android.support.test.filters.LargeTest;
import android.view.View;

import org.json.JSONObject;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.media.RouterTestUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.util.ClickUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.StringWriter;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for MediaRouter.
 *
 * TODO(jbudorick): Remove this when media_router_integration_browsertest runs on Android.
 */
@CommandLineFlags.Add(ContentSwitches.DISABLE_GESTURE_REQUIREMENT_FOR_PRESENTATION)
public class MediaRouterIntegrationTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String TEST_PAGE =
            "/chrome/test/media_router/resources/basic_test.html?__is_android__=true";
    private static final String TEST_PAGE_RECONNECT_FAIL =
            "/chrome/test/media_router/resources/fail_reconnect_session.html?__is_android__=true";

    private static final String TEST_SINK_NAME = "test-sink-1";
    // The javascript snippets.
    private static final String UNSET_RESULT_SCRIPT = "lastExecutionResult = null";
    private static final String GET_RESULT_SCRIPT = "lastExecutionResult";
    private static final String CHECK_SESSION_SCRIPT = "checkSession();";
    private static final String CHECK_START_FAILED_SCRIPT = "checkStartFailed('%s', '%s');";
    private static final String START_SESSION_SCRIPT = "startSession();";
    private static final String TERMINATE_SESSION_SCRIPT =
            "terminateSessionAndWaitForStateChange();";
    private static final String WAIT_DEVICE_SCRIPT = "waitUntilDeviceAvailable();";
    private static final String SEND_MESSAGE_AND_EXPECT_RESPONSE_SCRIPT =
            "sendMessageAndExpectResponse('%s');";
    private static final String SEND_MESSAGE_AND_EXPECT_CONNECTION_CLOSE_ON_ERROR_SCRIPT =
            "sendMessageAndExpectConnectionCloseOnError()";

    private static final int VIEW_TIMEOUT_MS = 2000;
    private static final int VIEW_RETRY_MS = 100;
    private static final int SCRIPT_TIMEOUT_MS = 10000;
    private static final int SCRIPT_RETRY_MS = 50;

    private StrictMode.ThreadPolicy mOldPolicy;

    private EmbeddedTestServer mTestServer;

    public MediaRouterIntegrationTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        // Temporary until support library is updated, see http://crbug.com/576393.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOldPolicy = StrictMode.allowThreadDiskReads();
                StrictMode.allowThreadDiskWrites();
            }
        });
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    public void tearDown() throws Exception {
        // Temporary until support library is updated, see http://crbug.com/576393.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                StrictMode.setThreadPolicy(mOldPolicy);
            }
        });
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    // TODO(zqzhang): Move this to a util class?
    // TODO(zqzhang): This method does not handle Unicode escape sequences.
    private String unescapeString(String str) {
        assert str.charAt(0) == '\"' && str.charAt(str.length() - 1) == '\"';
        str = str.substring(1, str.length() - 1);
        StringWriter writer = new StringWriter();
        for (int i = 0; i < str.length(); i++) {
            char thisChar = str.charAt(i);
            if (thisChar != '\\') {
                writer.write(str.charAt(i));
                continue;
            }
            char nextChar = str.charAt(++i);
            switch (nextChar) {
                case 't':
                    writer.write('\t');
                    break;
                case 'b':
                    writer.write('\b');
                    break;
                case 'n':
                    writer.write('\n');
                    break;
                case 'r':
                    writer.write('\r');
                    break;
                case 'f':
                    writer.write('\f');
                    break;
                case '\'':
                    writer.write('\'');
                    break;
                case '\"':
                    writer.write('\"');
                    break;
                case '\\':
                    writer.write('\\');
                    break;
                default:
                    writer.write(nextChar);
            }
        }
        return writer.toString();
    }

    private void executeJavaScriptApi(WebContents webContents, String script) {
        executeJavaScriptApi(webContents, script, SCRIPT_TIMEOUT_MS, SCRIPT_RETRY_MS);
    }

    private void executeJavaScriptApi(
            final WebContents webContents, final String script, int maxTimeoutMs, int intervalMs) {
        try {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, UNSET_RESULT_SCRIPT);
            JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, script);
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            String result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                    webContents, GET_RESULT_SCRIPT);
                            return !result.equals("null");
                        } catch (Exception e) {
                            return false;
                        }
                    }
                }, maxTimeoutMs, intervalMs);
            String unescapedResult = unescapeString(JavaScriptUtils
                    .executeJavaScriptAndWaitForResult(webContents, GET_RESULT_SCRIPT));
            JSONObject jsonResult = new JSONObject(unescapedResult);
            assertTrue(jsonResult.getString("errorMessage"),
                    jsonResult.getBoolean("passed"));
        } catch (Exception e) {
            e.printStackTrace();
            fail("caught exception while executing javascript:" + script);
        }
    }

    String getJavaScriptVariable(WebContents webContents, String script) {
        try {
            String result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, script);
            if (result.charAt(0) == '\"' && result.charAt(result.length() - 1) == '\"') {
                result = result.substring(1, result.length() - 1);
            }
            return result;
        } catch (Exception e) {
            e.printStackTrace();
            fail();
            return null;
        }
    }

    void checkStartFailed(WebContents webContents, String errorName, String errorMessageSubstring) {
        String script = String.format(CHECK_START_FAILED_SCRIPT, errorName, errorMessageSubstring);
        executeJavaScriptApi(webContents, script);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        ChromeMediaRouter.setRouteProviderBuilderForTest(new MockMediaRouteProvider.Builder());
        startMainActivityOnBlankPage();
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testBasic() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");
        assertFalse(sessionId.length() == 0);
        String defaultRequestSessionId = getJavaScriptVariable(
                webContents, "defaultRequestSessionId");
        assertEquals(sessionId, defaultRequestSessionId);
        executeJavaScriptApi(webContents, TERMINATE_SESSION_SCRIPT);
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testSendAndOnMessage() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");
        assertFalse(sessionId.length() == 0);
        executeJavaScriptApi(webContents,
                String.format(SEND_MESSAGE_AND_EXPECT_RESPONSE_SCRIPT, "foo"));
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testOnClose() throws InterruptedException, TimeoutException {
        MockMediaRouteProvider.Builder.sProvider.setCloseRouteWithErrorOnSend(true);
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");
        assertFalse(sessionId.length() == 0);
        executeJavaScriptApi(webContents,
                SEND_MESSAGE_AND_EXPECT_CONNECTION_CLOSE_ON_ERROR_SCRIPT);
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testFailNoProvider() throws InterruptedException, TimeoutException {
        MockMediaRouteProvider.Builder.sProvider.setIsSupportsSource(false);
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(getInstrumentation(), testRouteButton);
        checkStartFailed(
                webContents, "UnknownError", "No provider supports createRoute with source");
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testFailCreateRoute() throws InterruptedException, TimeoutException {
        MockMediaRouteProvider.Builder.sProvider.setCreateRouteErrorMessage("Unknown sink");
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(getInstrumentation(), testRouteButton);
        checkStartFailed(
                webContents, "UnknownError", "Unknown sink");
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testReconnectSession() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");

        loadUrlInNewTab(mTestServer.getURL(TEST_PAGE));
        WebContents newWebContents = getActivity().getActivityTab().getWebContents();
        assertTrue(webContents != newWebContents);
        executeJavaScriptApi(newWebContents, String.format("reconnectSession(\'%s\');", sessionId));
        String reconnectedSessionId =
                getJavaScriptVariable(newWebContents, "reconnectedSession.id");
        assertEquals(sessionId, reconnectedSessionId);
        executeJavaScriptApi(webContents, TERMINATE_SESSION_SCRIPT);
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testFailReconnectSession() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");

        MockMediaRouteProvider.Builder.sProvider.setJoinRouteErrorMessage("Unknown route");
        loadUrlInNewTab(mTestServer.getURL(TEST_PAGE_RECONNECT_FAIL));
        WebContents newWebContents = getActivity().getActivityTab().getWebContents();
        assertTrue(webContents != newWebContents);
        executeJavaScriptApi(newWebContents,
                String.format("checkReconnectSessionFails('%s');", sessionId));
    }

    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testFailStartCancelled() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = getActivity().getActivityTab().getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        final Dialog routeSelectionDialog = RouterTestUtils.waitForDialog(
                getActivity(), VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        assertNotNull(routeSelectionDialog);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    routeSelectionDialog.cancel();
                }
            });
        checkStartFailed(webContents, "NotAllowedError", "Dialog closed.");
    }
}
