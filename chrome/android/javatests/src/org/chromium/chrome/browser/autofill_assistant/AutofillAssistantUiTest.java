// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.mockito.Mockito.inOrder;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChipType;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetails;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsModel;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.Calendar;
import java.util.List;

/**
 * Instrumentation tests for autofill assistant UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantUiTest {
    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    public AssistantCoordinator.Delegate mCoordinatorDelegateMock;

    @Mock
    public Runnable mRunnableMock;

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

    // TODO(crbug.com/806868): Add more UI details test and check, like payment request UI,
    // highlight chips and so on.
    @Test
    @MediumTest
    public void testStartAndAccept() throws Exception {
        InOrder inOrder = inOrder(mRunnableMock);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        AssistantCoordinator assistantCoordinator = ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new AssistantCoordinator(getActivity(),
                                mCustomTabActivityTestRule.getWebContents(),
                                mCoordinatorDelegateMock));

        // Bottom sheet is shown when creating the AssistantCoordinator.
        View bottomSheet = findViewByIdInMainCoordinator(R.id.autofill_assistant);
        Assert.assertTrue(bottomSheet.isShown());

        // Show onboarding.
        ThreadUtils.runOnUiThreadBlocking(() -> assistantCoordinator.showOnboarding(mRunnableMock));
        View onboardingView = bottomSheet.findViewById(R.id.assistant_onboarding);
        Assert.assertNotNull(onboardingView);
        View initOkButton = onboardingView.findViewById(R.id.button_init_ok);
        Assert.assertNotNull(initOkButton);
        ThreadUtils.runOnUiThreadBlocking(() -> { initOkButton.performClick(); });
        ThreadUtils.runOnUiThreadBlocking(() -> inOrder.verify(mRunnableMock).run());

        // Show and check status message.
        String testStatusMessage = "test message";
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getHeaderModel().set(
                                AssistantHeaderModel.STATUS_MESSAGE, testStatusMessage));
        TextView statusMessageView = bottomSheet.findViewById(R.id.status_message);
        Assert.assertEquals(statusMessageView.getText(), testStatusMessage);

        // Show overlay.
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getOverlayModel().set(
                                AssistantOverlayModel.STATE, AssistantOverlayState.FULL));
        View overlay = bottomSheet.findViewById(R.id.touch_event_filter);
        Assert.assertTrue(overlay.isShown());

        // Show chips.
        List<AssistantChip> chips = Arrays.asList(
                new AssistantChip(
                        AssistantChipType.CHIP_ASSISTIVE, "chip 0", () -> {/* do nothing */}),
                new AssistantChip(AssistantChipType.CHIP_ASSISTIVE, "chip 1", mRunnableMock));
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getCarouselModel().getChipsModel().set(
                                chips));
        RecyclerView chipsViewContainer =
                assistantCoordinator.getBottomBarCoordinator().getCarouselCoordinator().getView();
        Assert.assertEquals(2, chipsViewContainer.getAdapter().getItemCount());

        // Choose the second chip.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { chipsViewContainer.getChildAt(1).performClick(); });
        inOrder.verify(mRunnableMock).run();

        // Show movie details.
        String movieTitle = "testTitle";
        String movieDescription = "This is a fancy test movie";
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getDetailsModel().set(
                                AssistantDetailsModel.DETAILS,
                                new AssistantDetails(movieTitle, /* url = */ "",
                                        Calendar.getInstance().getTime(), movieDescription,
                                        /* mId = */ "",
                                        /* price = */ null,
                                        /* userApprovalRequired= */ false,
                                        /* highlightTitle= */ false, /* highlightDate= */
                                        false, /* showPlaceholdersForEmptyFields= */ false)));
        TextView detailsTitle = (TextView) bottomSheet.findViewById(R.id.details_title);
        TextView detailsText = (TextView) bottomSheet.findViewById(R.id.details_text);
        Assert.assertEquals(detailsTitle.getText(), movieTitle);
        Assert.assertTrue(detailsText.getText().toString().contains(movieDescription));

        // Progress bar must be shown.
        Assert.assertTrue(bottomSheet.findViewById(R.id.progress_bar).isShown());

        // Click 'X' button runs the AssistantHeaderModel.CLOSE_BUTTON_CALLBACK.
        AssistantHeaderModel headerModel = assistantCoordinator.getModel().getHeaderModel();
        headerModel.set(AssistantHeaderModel.CLOSE_BUTTON_CALLBACK, mRunnableMock);
        ThreadUtils.runOnUiThreadBlocking(
                () -> { bottomSheet.findViewById(R.id.close_button).performClick(); });
        inOrder.verify(mRunnableMock).run();
    }
}
