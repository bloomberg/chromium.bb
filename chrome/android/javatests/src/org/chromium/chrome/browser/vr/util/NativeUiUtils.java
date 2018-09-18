// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.graphics.PointF;
import android.view.Choreographer;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.UiTestOperationType;
import org.chromium.chrome.browser.vr.UserFriendlyElementName;
import org.chromium.chrome.browser.vr.VrControllerTestAction;
import org.chromium.chrome.browser.vr.VrDialog;
import org.chromium.chrome.browser.vr.VrShell;
import org.chromium.chrome.browser.vr.VrUiTestActivityResult;
import org.chromium.chrome.browser.vr.VrViewContainer;

import java.io.File;
import java.util.concurrent.CountDownLatch;

/**
 * Class containing utility functions for interacting with the VR browser native UI, e.g. the
 * omnibox or back button.
 */
public class NativeUiUtils {
    // Arbitrary but reasonable amount of time to expect the UI to stop updating after interacting
    // with an element.
    private static final int DEFAULT_UI_QUIESCENCE_TIMEOUT_MS = 1000;

    /**
     * Enables the use of both the mock head pose (locked forward) and Chrome-side mocked controller
     * without performing any specific actions.
     */
    public static void enableMockedInput() {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                0 /* elementName, unused */, VrControllerTestAction.ENABLE_MOCKED_INPUT,
                new PointF() /* position, unused */);
    }

    /**
     * Clicks on a UI element as if done via a controller.
     *
     * @param elementName The UserFriendlyElementName that will be clicked on.
     * @param position A PointF specifying where on the element to send the click relative to a
     *        unit square centered at (0, 0).
     */
    public static void clickElement(int elementName, PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.CLICK, position);
    }

    /**
     * Clicks on a UI element as if done via a controller and waits until all resulting
     * animations have finished and propogated to the point of being visible in screenshots.
     *
     * @param elementName The UserFriendlyElementName that will be clicked on.
     * @param position A PointF specifying where on the element to send the click relative to a
     *        unit square centered at (0, 0).
     */
    public static void clickElementAndWaitForUiQuiescence(
            final int elementName, final PointF position) throws InterruptedException {
        performActionAndWaitForUiQuiescence(() -> { clickElement(elementName, position); });
    }

    /**
     * Clicks on a fallback UI element's positive button, e.g. "Allow" or "Confirm".
     */
    public static void clickFallbackUiPositiveButton() throws InterruptedException {
        clickFallbackUiButton(R.id.positive_button);
    }

    /**
     * Clicks on a fallback UI element's negative button, e.g. "Deny" or "Cancel".
     */
    public static void clickFallbackUiNegativeButton() throws InterruptedException {
        clickFallbackUiButton(R.id.negative_button);
    }

    /**
     * Clicks and drags within a single UI element.
     *
     * @param elementName The UserFriendlyElementName that will be clicked and dragged in.
     * @param positionStart The PointF specifying where on the element to start the click/drag
     *        relative to a unit square centered at (0, 0).
     * @param positionEnd The PointF specifying where on the element to end the click/drag relative
     *        to a unit square centered at (0, 0);
     * @param numInterpolatedSteps How many steps to interpolate the drag between the provided
     *        start and end positions.
     */
    public static void clickAndDragElement(
            int elementName, PointF positionStart, PointF positionEnd, int numInterpolatedSteps) {
        Assert.assertTrue(
                "Given a negative number of steps to interpolate", numInterpolatedSteps >= 0);
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.CLICK_DOWN, positionStart);
        PointF stepOffset =
                new PointF((positionEnd.x - positionStart.x) / (numInterpolatedSteps + 1),
                        (positionEnd.y - positionStart.y) / (numInterpolatedSteps + 1));
        PointF currentPosition = positionStart;
        for (int i = 0; i < numInterpolatedSteps; i++) {
            currentPosition.offset(stepOffset.x, stepOffset.y);
            TestVrShellDelegate.getInstance().performControllerActionForTesting(
                    elementName, VrControllerTestAction.MOVE, currentPosition);
        }
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.MOVE, positionEnd);
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.CLICK_UP, positionEnd);
    }

    /**
     * Sets the native code to start using the real controller and head pose data again instead of
     * fake testing data.
     */
    public static void revertToRealInput() {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                0 /* elementName, unused */, VrControllerTestAction.REVERT_TO_REAL_INPUT,
                new PointF() /* position, unused */);
    }

    /**
     * Runs the given Runnable and waits until the native UI reports that it is quiescent.
     *
     * @param action A Runnable containing the action to perform.
     */
    public static void performActionAndWaitForUiQuiescence(Runnable action)
            throws InterruptedException {
        final TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        final CountDownLatch resultLatch = new CountDownLatch(1);
        // Run on the UI thread to prevent issues with registering a new callback before
        // ReportUiOperationResultForTesting has finished.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            instance.registerUiOperationCallbackForTesting(UiTestOperationType.UI_ACTIVITY_RESULT,
                    () -> { resultLatch.countDown(); }, DEFAULT_UI_QUIESCENCE_TIMEOUT_MS);
        });
        action.run();

        // Wait for any outstanding animations to finish.
        resultLatch.await();
        int uiResult =
                instance.getLastUiOperationResultForTesting(UiTestOperationType.UI_ACTIVITY_RESULT);
        Assert.assertEquals("UI reported non-quiescent result '"
                        + vrUiTestActivityResultToString(uiResult) + "'",
                VrUiTestActivityResult.QUIESCENT, uiResult);
    }

    /**
     * Blocks until the specified number of frames have been triggered by the Choreographer.
     *
     * @param numFrames The number of frames to wait for.
     */
    public static void waitNumFrames(int numFrames) throws InterruptedException {
        final CountDownLatch frameLatch = new CountDownLatch(numFrames);
        ThreadUtils.runOnUiThread(() -> {
            final Choreographer.FrameCallback callback = new Choreographer.FrameCallback() {
                @Override
                public void doFrame(long frameTimeNanos) {
                    if (frameLatch.getCount() == 0) return;
                    Choreographer.getInstance().postFrameCallback(this);
                    frameLatch.countDown();
                }
            };
            Choreographer.getInstance().postFrameCallback(callback);
        });
        frameLatch.await();
    }

    /**
     * Tells the native UI to dump the next frame's frame buffers to disk and waits for it to
     * signal that the dump is complete.
     *
     * @param filepathBase The filepath to use as a base for image dumps. Will have a suffix and
     *        file extension automatically appended.
     */
    public static void dumpNextFramesFrameBuffers(String filepathBase) throws InterruptedException {
        // Clear out any existing images with the names of the files that may be created.
        for (String suffix :
                new String[] {"_WebXrOverlay", "_WebXrContent", "_BrowserUi", "_BrowserContent"}) {
            File dumpFile = new File(filepathBase, suffix + ".png");
            Assert.assertFalse("Failed to delete existing screenshot",
                    dumpFile.exists() && !dumpFile.delete());
        }

        final TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        final CountDownLatch resultLatch = new CountDownLatch(1);

        // Run on the UI thread to prevent issues with registering a new callback before
        // ReportUiOperationResultForTesting has finished.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            instance.registerUiOperationCallbackForTesting(UiTestOperationType.FRAME_BUFFER_DUMPED,
                    () -> { resultLatch.countDown(); }, 0 /* unused */);
        });
        instance.saveNextFrameBufferToDiskForTesting(filepathBase);
        resultLatch.await();
    }

    /**
     * Returns the Container of 2D UI that is shown in VR.
     */
    public static ViewGroup getVrViewContainer() {
        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        return vrShell.getVrViewContainerForTesting();
    }

    private static void clickFallbackUiButton(int buttonId) throws InterruptedException {
        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        VrViewContainer viewContainer = vrShell.getVrViewContainerForTesting();
        Assert.assertTrue(
                "VrViewContainer does not have children", viewContainer.getChildCount() > 0);
        // Click on whatever dialog was most recently added
        VrDialog vrDialog = (VrDialog) viewContainer.getChildAt(viewContainer.getChildCount() - 1);
        View button = vrDialog.findViewById(buttonId);
        Assert.assertNotNull("Did not find view with specified ID", button);
        // Calculate the center of the button we want to click on and scale it to fit a unit square
        // centered on (0,0).
        float x = ((button.getX() + button.getWidth() / 2) - vrDialog.getWidth() / 2)
                / vrDialog.getWidth();
        float y = ((button.getY() + button.getHeight() / 2) - vrDialog.getHeight() / 2)
                / vrDialog.getHeight();
        PointF buttonCenter = new PointF(x, y);
        clickElementAndWaitForUiQuiescence(UserFriendlyElementName.BROWSING_DIALOG, buttonCenter);
    }

    private static String vrUiTestActivityResultToString(int result) {
        switch (result) {
            case VrUiTestActivityResult.UNREPORTED:
                return "Unreported";
            case VrUiTestActivityResult.QUIESCENT:
                return "Quiescent";
            case VrUiTestActivityResult.TIMEOUT_NO_START:
                return "Timeout (UI activity not started)";
            case VrUiTestActivityResult.TIMEOUT_NO_END:
                return "Timeout (UI activity not stopped)";
            default:
                return "Unknown result";
        }
    }
}
