// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.keyboard.TextEditAction;
import org.chromium.chrome.browser.vr.mock.MockBrowserKeyboardInterface;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for interacting with HTML input elements on a webpage.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserWebInputEditingTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrBrowserTestFramework mVrBrowserTestFramework;
    private EmulatedVrController mController;

    @Before
    public void setUp() throws Exception {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
    }

    /**
     * Verifies that when a web input field is focused, the VrInputMethodManagerWrapper is asked to
     * spawn the keyboard. Moreover, we verify that an edit sent to web contents via the
     * VrInputConnection updates indices on the VrInputMethodManagerWrapper.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("enable-features=VrLaunchIntents")
    public void testWebInputFocus() throws InterruptedException {
        testWebInputFocusImpl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_web_input_editing"));
    }

    /**
     * Verifies the same thing as testWebInputFocus, but with the input box in a cross-origin
     * iframe.
     * Automation of a manual test in https://crbug.com/862153
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("enable-features=VrLaunchIntents")
    public void testWebInputFocusIframe() throws InterruptedException {
        testWebInputFocusImpl(VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                "test_web_input_editing_iframe_outer"));
    }

    private void testWebInputFocusImpl(String url) throws InterruptedException {
        mVrTestRule.loadUrl(url, PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        MockBrowserKeyboardInterface keyboard = new MockBrowserKeyboardInterface();
        vrShell.getInputMethodManageWrapperForTesting().setBrowserKeyboardInterfaceForTesting(
                keyboard);

        // The webpage reacts to the first controller click by focusing its input field. Verify that
        // focus gain spawns the keyboard by clicking in the center of the page.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CONTENT_QUAD, new PointF());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && visible;
                },
                "Keyboard did not show from focusing a web input box", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        // Add text to the input field via the input connection and verify that the keyboard
        // interface is called to update the indices.
        VrInputConnection ic = vrShell.getVrInputConnectionForTesting();
        TextEditAction[] edits = {new TextEditAction(TextEditActionType.COMMIT_TEXT, "i", 1)};
        ic.onKeyboardEdit(edits);
        // Inserting 'i' should move the cursor by one character and there should be no composition.
        MockBrowserKeyboardInterface.Indices expectedIndices =
                new MockBrowserKeyboardInterface.Indices(1, 1, -1, -1);
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    MockBrowserKeyboardInterface.Indices indices = keyboard.getLastIndices();
                    return indices == null ? false : indices.equals(expectedIndices);
                },
                "Inputting text did not move cursor the expected amount", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        // The second click should result in a focus loss and should hide the keyboard.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CONTENT_QUAD, new PointF());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && !visible;
                },
                "Keyboard did not hide from unfocusing a web input box", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Tests that interacting with a <code><select></code> tag on a webpage brings up a working
     * Android selection dialog.
     */
    @Test
    @MediumTest
    public void testSelectTag() throws TimeoutException, InterruptedException {
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_select_tag"),
                PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        // Click on the <select> tag and wait for the resulting modal dialog to appear.
        DOMUtils.clickNode(mVrBrowserTestFramework.getFirstTabWebContents(), "selectbox",
                false /* goThroughRootAndroidView */);
        NativeUiUtils.waitForModalDialogStatus(true /* shouldBeShown */, mVrTestRule.getActivity());
        // Click on whichever option is near the center of the screen. We don't care which, as long
        // as it's not the initial selection, which should be at the top. Clicking in the exact
        // center can sometimes click in the area between two options, so offset slightly to prevent
        // flakes from that.
        // Most of the time, the first click will go through. However, it's possible to send the
        // click while the dialog is technically present, but not fully ready, resulting in the
        // click not registering. Since there doesn't seem to be a good way to wait for the dialog
        // to be ready, try several times.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    if (!mVrTestRule.getActivity().getModalDialogManager().isShowing()) return true;
                    NativeUiUtils.clickElement(
                            UserFriendlyElementName.CONTENT_QUAD, new PointF(0f, 0.05f));
                    return false;
                },
                "Could not click on <select> modal dialog", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_LONG_MS);

        // Wait on JavaScript to verify that a selection was made.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        mVrBrowserTestFramework.endTest();
    }
}
