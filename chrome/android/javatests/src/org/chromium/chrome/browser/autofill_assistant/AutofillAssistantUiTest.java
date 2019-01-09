// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.List;

import javax.annotation.concurrent.GuardedBy;

/**
 * Instrumentation tests for autofill assistant UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantUiTest {
    private static class TestCallback<T> implements Callback<T> {
        @GuardedBy("this")
        private T mResult;

        synchronized T getResult() {
            return mResult;
        }

        @Override
        public synchronized void onResult(T result) {
            mResult = result;
        }
    };

    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    private AbstractAutofillAssistantUiController mControllerMock;
    @Captor
    private ArgumentCaptor<String> mLastSelectedScriptPathCaptor;

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestPage = mTestServer.getURL(UrlUtils.getIsolatedTestFilePath(
                "components/test/data/autofill_assistant/autofill_assistant_target_website.html"));
        PathUtils.setPrivateDataDirectorySuffix("chrome");
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        when(mControllerMock.getDetails()).thenReturn(Details.EMPTY_DETAILS);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
        mTestServer.stopAndDestroyServer();
    }

    /**
     * @see CustomTabsTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);
    }

    private View findViewByIdInMainCoordinator(int id) {
        return getActivity().findViewById(R.id.coordinator).findViewById(id);
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    public void testFirstTimeStartAccept() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        // Start autofill assistant UI. The first run screen must be shown first since the
        // preference hasn't been set.

        TestCallback<Boolean> startCallback = new TestCallback<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantFacade.startWithCallback(getActivity(), startCallback));
        View firstRunScreen = findViewByIdInMainCoordinator(R.id.init_screen);
        Assert.assertNotNull(firstRunScreen);
        Assert.assertTrue(firstRunScreen.isShown());
        Assert.assertNull(startCallback.getResult());

        // Accept on the first run screen to continue.
        View initOkButton = firstRunScreen.findViewById(R.id.button_init_ok);
        ThreadUtils.runOnUiThreadBlocking(() -> { initOkButton.performClick(); });
        Assert.assertEquals(Boolean.TRUE, startCallback.getResult());
    }

    // TODO(crbug.com/806868): Add more UI details test and check, like payment request UI,
    // highlight chips and so on.
    @Test
    @MediumTest
    public void testStartAndDismiss() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        UiDelegateHolder uiDelegateHolder = ThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantUiDelegate.start(getActivity(), mControllerMock));

        // Show and check status message.
        String testStatusMessage = "test message";
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> uiDelegateHolder.performUiOperation(
                                uiDelegate -> uiDelegate.showStatusMessage(testStatusMessage)));
        View bottomSheet = findViewByIdInMainCoordinator(R.id.autofill_assistant);
        Assert.assertTrue(bottomSheet.isShown());
        TextView statusMessageView = (TextView) bottomSheet.findViewById(R.id.status_message);
        Assert.assertEquals(statusMessageView.getText(), testStatusMessage);

        // Show overlay.
        ThreadUtils.runOnUiThreadBlocking(() -> uiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.showOverlay();
            uiDelegate.disableProgressBarPulsing();
        }));
        View overlay = bottomSheet.findViewById(R.id.touch_event_filter);
        Assert.assertTrue(overlay.isShown());

        // Show scripts.
        List<AutofillAssistantUiDelegate.ScriptHandle> scriptHandles = new ArrayList<>();
        scriptHandles.add(
                new AutofillAssistantUiDelegate.ScriptHandle("testScript1", false, "path1"));
        scriptHandles.add(
                new AutofillAssistantUiDelegate.ScriptHandle("testScript2", false, "path2"));
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> uiDelegateHolder.performUiOperation(
                                uiDelegate -> uiDelegate.updateScripts(scriptHandles)));
        ViewGroup chipsViewContainer = (ViewGroup) bottomSheet.findViewById(R.id.carousel);
        Assert.assertEquals(2, chipsViewContainer.getChildCount());

        //  choose the first script.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { chipsViewContainer.getChildAt(0).performClick(); });
        verify(mControllerMock, times(1)).onScriptSelected(mLastSelectedScriptPathCaptor.capture());
        Assert.assertEquals("path1", mLastSelectedScriptPathCaptor.getValue());

        // Show movie details.
        String movieTitle = "testTitle";
        String movieDescription = "This is a fancy test movie";
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> uiDelegateHolder.performUiOperation(uiDelegate
                                -> uiDelegate.showDetails(new Details(movieTitle, /* url = */ "",
                                        Calendar.getInstance().getTime(), movieDescription,
                                        /* mId = */ "",
                                        /* price = */ null,
                                        /* isFinal= */ true, Collections.emptySet()))));
        TextView detailsTitle = (TextView) bottomSheet.findViewById(R.id.details_title);
        TextView detailsText = (TextView) bottomSheet.findViewById(R.id.details_text);
        Assert.assertEquals(detailsTitle.getText(), movieTitle);
        Assert.assertTrue(detailsText.getText().toString().contains(movieDescription));

        // Progress bar must be shown.
        Assert.assertTrue(bottomSheet.findViewById(R.id.progress_bar).isShown());

        // Click 'X' button to graceful shutdown.
        doAnswer((Answer<Void>) invocation -> {
            uiDelegateHolder.dismiss(R.string.autofill_assistant_stopped);
            return null;
        })
                .when(mControllerMock)
                .onDismiss();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { bottomSheet.findViewById(R.id.close_button).performClick(); });
        Assert.assertFalse(bottomSheet.isShown());
    }
}
