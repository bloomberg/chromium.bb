// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.JavascriptAppModalDialog;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for displaying and functioning of modal dialogs.
 */
public class ModalDialogTest extends ChromeShellTestBase {
    private static final String TAG = "ModalDialogTest";
    private static final String EMPTY_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html><title>Modal Dialog Test</title><p>Testcase.</p></title></html>");
    private static final String BEFORE_UNLOAD_URL = UrlUtils.encodeHtmlDataUri(
            "<html>" +
            "<head><script>window.onbeforeunload=function() {" +
            "return 'Are you sure?';" +
            "};</script></head></html>");

    @Override
    public void setUp() throws Exception {
        super.setUp();
        launchChromeShellWithUrl(EMPTY_PAGE);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
    }

    /**
     * Verifies modal alert-dialog appearance and that JavaScript execution is
     * able to continue after dismissal.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testAlertModalDialog()
            throws InterruptedException, TimeoutException, ExecutionException {
        final OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("alert('Hello Android!');");

        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);

        clickOk(jsDialog);
        assertTrue("JavaScript execution should continue after closing prompt.",
                scriptEvent.waitUntilHasValue());
    }

    /**
     * Verifies that clicking on a button twice doesn't crash.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testAlertModalDialogWithTwoClicks()
            throws InterruptedException, TimeoutException, ExecutionException {
        OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("alert('Hello Android');");
        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);

        clickOk(jsDialog);
        clickOk(jsDialog);

        assertTrue("JavaScript execution should continue after closing prompt.",
                scriptEvent.waitUntilHasValue());
    }

    /**
     * Verifies that modal confirm-dialogs display, two buttons are visible and
     * the return value of [Ok] equals true, [Cancel] equals false.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testConfirmModalDialog()
            throws InterruptedException, TimeoutException, ExecutionException {
        OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("confirm('Android');");

        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);

        Button[] buttons = getAlertDialogButtons(jsDialog.getDialogForTest());
        assertNotNull("No cancel button in confirm dialog.", buttons[0]);
        assertEquals("Cancel button is not visible.", View.VISIBLE, buttons[0].getVisibility());
        if (buttons[1] != null) {
            assertNotSame("Neutral button visible when it should not.",
                    View.VISIBLE, buttons[1].getVisibility());
        }
        assertNotNull("No OK button in confirm dialog.", buttons[2]);
        assertEquals("OK button is not visible.", View.VISIBLE, buttons[2].getVisibility());

        clickOk(jsDialog);
        assertTrue("JavaScript execution should continue after closing dialog.",
                scriptEvent.waitUntilHasValue());

        String resultString = scriptEvent.getJsonResultAndClear();
        assertEquals("Invalid return value.", "true", resultString);

        // Try again, pressing cancel this time.
        scriptEvent = executeJavaScriptAndWaitForDialog("confirm('Android');");
        jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);

        clickCancel(jsDialog);
        assertTrue("JavaScript execution should continue after closing dialog.",
                scriptEvent.waitUntilHasValue());

        resultString = scriptEvent.getJsonResultAndClear();
        assertEquals("Invalid return value.", "false", resultString);
    }

    /**
     * Verifies that modal prompt-dialogs display and the result is returned.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testPromptModalDialog()
            throws InterruptedException, TimeoutException, ExecutionException {
        final String promptText = "Hello Android!";
        final OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("prompt('Android', 'default');");

        final JavascriptAppModalDialog jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);

        // Set the text in the prompt field of the dialog.
        boolean result = ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                EditText prompt = (EditText) jsDialog.getDialogForTest().findViewById(
                        org.chromium.chrome.R.id.js_modal_dialog_prompt);
                if (prompt == null) return false;
                prompt.setText(promptText);
                return true;
            }
        });
        assertTrue("Failed to find prompt view in prompt dialog.", result);

        clickOk(jsDialog);
        assertTrue("JavaScript execution should continue after closing prompt.",
                scriptEvent.waitUntilHasValue());

        String resultString = scriptEvent.getJsonResultAndClear();
        assertEquals("Invalid return value.", '"' + promptText + '"', resultString);
    }

    /**
     * Verifies beforeunload dialogs are shown and they block/allow navigation
     * as appropriate.
     */
    //@MediumTest
    //@Feature({"Browser", "Main"})
    @DisabledTest //crbug/270593
    public void testBeforeUnloadDialog()
            throws InterruptedException, TimeoutException, ExecutionException {
        loadUrlWithSanitization(BEFORE_UNLOAD_URL);
        executeJavaScriptAndWaitForDialog("history.back();");

        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);
        checkButtonPresenceVisibilityText(
                jsDialog, 0, org.chromium.chrome.R.string.stay_on_this_page,
                "Stay on this page");
        clickCancel(jsDialog);

        assertEquals(BEFORE_UNLOAD_URL, getActivity().getActiveContentViewCore()
                .getWebContents().getUrl());
        executeJavaScriptAndWaitForDialog("history.back();");

        jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);
        checkButtonPresenceVisibilityText(
                jsDialog, 2, org.chromium.chrome.R.string.leave_this_page,
                "Leave this page");

        final TestCallbackHelperContainer.OnPageFinishedHelper onPageLoaded =
                getActiveTabTestCallbackHelperContainer().getOnPageFinishedHelper();
        int callCount = onPageLoaded.getCallCount();
        clickOk(jsDialog);
        onPageLoaded.waitForCallback(callCount);
        assertEquals(EMPTY_PAGE, getActivity().getActiveContentViewCore()
                .getWebContents().getUrl());
    }

    /**
     * Verifies that when showing a beforeunload dialogs as a result of a page
     * reload, the correct UI strings are used.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testBeforeUnloadOnReloadDialog()
            throws InterruptedException, TimeoutException, ExecutionException {
        loadUrlWithSanitization(BEFORE_UNLOAD_URL);
        executeJavaScriptAndWaitForDialog("window.location.reload();");

        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);

        checkButtonPresenceVisibilityText(
                jsDialog, 0, org.chromium.chrome.R.string.dont_reload_this_page,
                "Don't reload this page");
        checkButtonPresenceVisibilityText(
                jsDialog, 2, org.chromium.chrome.R.string.reload_this_page,
                "Reload this page");
    }

    /**
     * Verifies that repeated dialogs give the option to disable dialogs
     * altogether and then that disabling them works.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testDisableRepeatedDialogs()
            throws InterruptedException, TimeoutException, ExecutionException {
        OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("alert('Android');");

        // Show a dialog once.
        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);

        clickCancel(jsDialog);
        scriptEvent.waitUntilHasValue();

        // Show it again, it should have the option to suppress subsequent dialogs.
        scriptEvent = executeJavaScriptAndWaitForDialog("alert('Android');");
        jsDialog = getCurrentDialog();
        assertNotNull("No dialog showing.", jsDialog);
        final AlertDialog dialog = jsDialog.getDialogForTest();
        String errorMessage = ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                final CheckBox suppress = (CheckBox) dialog.findViewById(
                        org.chromium.chrome.R.id.suppress_js_modal_dialogs);
                if (suppress == null) return "Suppress checkbox not found.";
                if (suppress.getVisibility() != View.VISIBLE) {
                    return "Suppress checkbox is not visible.";
                }
                suppress.setChecked(true);
                return null;
            }
        });
        assertNull(errorMessage, errorMessage);
        clickCancel(jsDialog);
        scriptEvent.waitUntilHasValue();

        scriptEvent.evaluateJavaScript(getActivity().getActiveContentViewCore(),
                "alert('Android');");
        assertTrue("No further dialog boxes should be shown.", scriptEvent.waitUntilHasValue());
    }

    /**
     * Displays a dialog and closes the tab in the background before attempting
     * to accept the dialog. Verifies that the dialog is dismissed when the tab
     * is closed.
     */
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testDialogDismissedAfterClosingTab() throws InterruptedException {
        executeJavaScriptAndWaitForDialog("alert('Android')");

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().createTab(EMPTY_PAGE);
            }
        });

        // Closing the tab should have dismissed the dialog.
        boolean criteriaSatisfied = CriteriaHelper.pollForCriteria(
                new JavascriptAppModalDialogShownCriteria(false));
        assertTrue("The dialog should have been dismissed when its tab was closed.",
                criteriaSatisfied);
    }

    /**
     * Asynchronously executes the given code for spawning a dialog and waits
     * for the dialog to be visible.
     */
    private OnEvaluateJavaScriptResultHelper executeJavaScriptAndWaitForDialog(String script)
            throws InterruptedException {
        return executeJavaScriptAndWaitForDialog(new OnEvaluateJavaScriptResultHelper(), script);
    }

    /**
     * Given a JavaScript evaluation helper, asynchronously executes the given
     * code for spawning a dialog and waits for the dialog to be visible.
     */
    private OnEvaluateJavaScriptResultHelper executeJavaScriptAndWaitForDialog(
            final OnEvaluateJavaScriptResultHelper helper, String script)
            throws InterruptedException {
        helper.evaluateJavaScript(getActivity().getActiveContentViewCore(),
                script);
        boolean criteriaSatisfied = CriteriaHelper.pollForCriteria(
                new JavascriptAppModalDialogShownCriteria(true));
        assertTrue("Could not spawn or locate a modal dialog.", criteriaSatisfied);
        return helper;
    }

    /**
     * Returns an array of the 3 buttons for this dialog, in the order
     * BUTTON_NEGATIVE, BUTTON_NEUTRAL and BUTTON_POSITIVE. Any of these values
     * can be null.
     */
    private Button[] getAlertDialogButtons(final AlertDialog dialog) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Button[]>() {
            @Override
            public Button[] call() {
                final Button[] buttons = new Button[3];
                buttons[0] = dialog.getButton(DialogInterface.BUTTON_NEGATIVE);
                buttons[1] = dialog.getButton(DialogInterface.BUTTON_NEUTRAL);
                buttons[2] = dialog.getButton(DialogInterface.BUTTON_POSITIVE);
                return buttons;
            }
        });
    }

    /**
     * Returns the current JavaScript modal dialog showing or null if no such dialog is currently
     * showing.
     */
    private JavascriptAppModalDialog getCurrentDialog() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<JavascriptAppModalDialog>() {
            @Override
            public JavascriptAppModalDialog call() {
                return JavascriptAppModalDialog.getCurrentDialogForTest();
            }
        });
    }

    private static class JavascriptAppModalDialogShownCriteria implements Criteria {
        private final boolean mShouldBeShown;

        public JavascriptAppModalDialogShownCriteria(boolean shouldBeShown) {
            mShouldBeShown = shouldBeShown;
        }

        @Override
        public boolean isSatisfied() {
            try {
                return ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        final boolean isShown =
                                JavascriptAppModalDialog.getCurrentDialogForTest() != null;
                        return mShouldBeShown == isShown;
                    }
                });
            } catch (ExecutionException e) {
                Log.e(TAG, "Failed to getCurrentDialog", e);
                return false;
            }
        }
    }

    /**
     * Simulates pressing the OK button of the passed dialog.
     */
    private void clickOk(JavascriptAppModalDialog dialog) {
        clickButton(dialog, DialogInterface.BUTTON_POSITIVE);
    }

    /**
     * Simulates pressing the Cancel button of the passed dialog.
     */
    private void clickCancel(JavascriptAppModalDialog dialog) {
        clickButton(dialog, DialogInterface.BUTTON_NEGATIVE);
    }

    private void clickButton(final JavascriptAppModalDialog dialog, final int whichButton) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                dialog.onClick(null, whichButton);
            }
        });
    }

    private void checkButtonPresenceVisibilityText(
            JavascriptAppModalDialog jsDialog, int buttonIndex,
            int expectedTextResourceId, String readableName) throws ExecutionException {
        final Button[] buttons = getAlertDialogButtons(jsDialog.getDialogForTest());
        final Button button = buttons[buttonIndex];
        assertNotNull("No '" + readableName + "' button in confirm dialog.", button);
        assertEquals("'" + readableName + "' button is not visible.",
                View.VISIBLE,
                button.getVisibility());
        assertEquals("'" + readableName + "' button has wrong text",
                getActivity().getResources().getString(expectedTextResourceId),
                button.getText().toString());
    }

    private TestCallbackHelperContainer getActiveTabTestCallbackHelperContainer() {
        return new TestCallbackHelperContainer(getActivity().getActiveTab().getContentViewCore());
    }
}
