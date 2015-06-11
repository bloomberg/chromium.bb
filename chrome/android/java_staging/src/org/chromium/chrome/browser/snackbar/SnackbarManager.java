// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.os.Handler;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;

import com.google.android.apps.chrome.R;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashSet;
import java.util.Stack;

/**
 * Manager for the snackbar showing at the bottom of activity.
 * <p/>
 * There should be only one SnackbarManager and one snackbar in the activity. The manager maintains
 * a stack to store all entries that should be displayed. When showing a new snackbar, old entry
 * will be pushed to stack and text/button will be updated to the newest entry.
 * <p/>
 * When action button is clicked, this manager will call
 * {@link SnackbarController#onAction(Object)} in corresponding listener, and show the next
 * entry in stack. Otherwise if no action is taken by user during
 * {@link #DEFAULT_SNACKBAR_SHOW_DURATION_MS} milliseconds, it will clear the stack and call
 * {@link SnackbarController#onDismissNoAction(Object)} to all listeners.
 */
public class SnackbarManager implements OnClickListener, OnGlobalLayoutListener {

    /**
     * Interface that shows the ability to provide a unified snackbar manager.
     */
    public interface SnackbarManageable {
        /**
         * @return The snackbar manager that has a proper anchor view.
         */
        SnackbarManager getSnackbarManager();
    }

    /**
     * Controller that post entries to snackbar manager and interact with snackbar manager during
     * dismissal and action click event.
     */
    public static interface SnackbarController {
        /**
         * Callback triggered when user clicks on button at end of snackbar. This method is only
         * called for controller having posted the entry the user clicked on; other controllers are
         * not notified. Also once this {@link #onAction(Object)} is called,
         * {@link #onDismissNoAction(Object)} and {@link #onDismissForEachType(boolean)} will not be
         * called.
         * @param actionData Data object passed when showing this specific snackbar.
         */
        void onAction(Object actionData);

        /**
         * Callback triggered when the snackbar is dismissed by either timeout or UI environment
         * change. This callback will be called for each entry a controller has posted, _except_ for
         * entries which the user has done action with, by clicking the action button.
         * @param actionData Data object associated with the dismissed snackbar entry.
         */
        void onDismissNoAction(Object actionData);

        /**
         * Notify each SnackbarControllers instance only once immediately before the snackbar is
         * dismissed. This function is likely to be used for controllers to do user metrics for
         * dismissal.
         * @param isTimeout Whether this dismissal is triggered by timeout.
         */
        void onDismissForEachType(boolean isTimeout);
    }

    private static final int DEFAULT_SNACKBAR_SHOW_DURATION_MS = 3000;
    private static final int ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS = 6000;

    // Used instead of the constant so tests can override the value.
    private static int sUndoBarShowDurationMs = DEFAULT_SNACKBAR_SHOW_DURATION_MS;
    private static int sAccessibilityUndoBarDurationMs = ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS;

    private final boolean mIsTablet;

    private View mParent;
    // Variable storing current xy position of parent view.
    private int[] mTempTopLeft = new int[2];
    private final Handler mUIThreadHandler;
    private Stack<SnackbarEntry> mStack = new Stack<SnackbarEntry>();
    private SnackbarPopupWindow mPopup;
    private final Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            dismissSnackbar(true);
        }
    };

    /**
     * Create an instance of SnackbarManager with the root view of entire activity.
     * @param parent The view that snackbar anchors to. Since SnackbarManager should be initialized
     *               during activity initialization, parent should always be set to root view of
     *               entire activity.
     */
    public SnackbarManager(View parent) {
        mParent = parent;
        mUIThreadHandler = new Handler();
        mIsTablet = DeviceFormFactor.isTablet(parent.getContext());
    }

    /**
     * Shows a snackbar with description text and an action button.
     * @param template Teamplate used to compose full description.
     * @param description Text for description showing at start of snackbar.
     * @param actionText Text for action button to show.
     * @param actionData Data bound to this snackbar entry. Will be returned to listeners when
     *                   action be clicked or snackbar be dismissed.
     * @param controller Listener for this snackbar entry.
     */
    public void showSnackbar(String template, String description, String actionText,
            Object actionData, SnackbarController controller) {
        int duration = sUndoBarShowDurationMs;
        // Duration for snackbars to show is different in normal mode and in accessibility mode.
        if (DeviceClassManager.isAccessibilityModeEnabled(mParent.getContext())) {
            duration = sAccessibilityUndoBarDurationMs;
        }
        showSnackbar(template, description, actionText, actionData, controller, duration);
    }

    /**
     * Shows a snackbar for the given timeout duration with description text and an action button.
     * Allows overriding the default timeout of {@link #DEFAULT_SNACKBAR_SHOW_DURATION_MS} with
     * a custom value.
     * @param template Teamplate used to compose full description.
     * @param description Text for description showing at start of snackbar.
     * @param actionText Text for action button to show.
     * @param actionData Data bound to this snackbar entry. Will be returned to listeners when
     *                   action be clicked or snackbar be dismissed.
     * @param controller Listener for this snackbar entry.
     * @param timeoutMs The timeout to use in ms.
     */
    public void showSnackbar(String template, String description, String actionText,
            Object actionData, SnackbarController controller, int timeoutMs) {
        mUIThreadHandler.removeCallbacks(mHideRunnable);
        mUIThreadHandler.postDelayed(mHideRunnable, timeoutMs);

        mStack.push(new SnackbarEntry(template, description, actionText, actionData, controller));
        if (mPopup == null) {
            mPopup = new SnackbarPopupWindow(mParent, this, template, description, actionText);
            showPopupAtBottom();
            mParent.getViewTreeObserver().addOnGlobalLayoutListener(this);
        } else {
            mPopup.setTextViews(template, description, actionText, true);
        }

        mPopup.announceforAccessibility();
    }

    /**
     * Convinient function for showSnackbar. Note this method adds passed entry to stack.
     */
    private void showSnackbar(SnackbarEntry entry) {
        showSnackbar(entry.mTemplate, entry.mDescription, entry.mActionText, entry.mData,
                entry.mController);
    }

    /**
     * Change parent view of snackbar. This method is likely to be called when a new window is
     * hiding the snackbar and will dismiss all snackbars.
     * @param newParent The new parent view snackbar anchors to.
     */
    public void setParentView(View newParent) {
        if (newParent == mParent) return;
        mParent.getViewTreeObserver().removeOnGlobalLayoutListener(this);
        mUIThreadHandler.removeCallbacks(mHideRunnable);
        dismissSnackbar(false);
        mParent = newParent;
    }

    /**
     * Dismisses snackbar, clears out all entries in stack and prevents future remove callbacks from
     * happening. This method also unregisters this class from global layout notifications.
     * @param isTimeout Whether dismissal was triggered by timeout.
     */
    public void dismissSnackbar(boolean isTimeout) {
        mUIThreadHandler.removeCallbacks(mHideRunnable);

        if (mPopup != null) {
            mPopup.dismiss();
            mPopup = null;
        }

        HashSet<SnackbarController> controllers = new HashSet<SnackbarController>();

        while (!mStack.isEmpty()) {
            SnackbarEntry entry = mStack.pop();
            if (!controllers.contains(entry.mController)) {
                entry.mController.onDismissForEachType(isTimeout);
                controllers.add(entry.mController);
            }
            entry.mController.onDismissNoAction(entry.mData);
        }
        mParent.getViewTreeObserver().removeOnGlobalLayoutListener(this);
    }

    /**
     * Removes all entries for certain type of controller. This method is used when a controller
     * wants to remove all entries it posted to snackbar manager before.
     * @param controller This method only removes entries posted by this controller.
     */
    public void removeSnackbarEntry(SnackbarController controller) {
        boolean isFound = false;
        SnackbarEntry[] snackbarEntries = new SnackbarEntry[mStack.size()];
        mStack.toArray(snackbarEntries);
        for (SnackbarEntry entry : snackbarEntries) {
            if (entry.mController == controller) {
                mStack.remove(entry);
                isFound = true;
            }
        }
        if (!isFound) return;

        finishSnackbarEntryRemoval(controller);
    }

    /**
     * Removes all entries for certain type of controller and with specified data. This method is
     * used when a controller wants to remove some entries it posted to snackbar manager before.
     * However it does not affect other controllers' entries. Note that this method assumes
     * different types of snackbar controllers are not sharing the same instance.
     * @param controller This method only removes entries posted by this controller.
     * @param data Identifier of an entry to be removed from stack.
     */
    public void removeSnackbarEntry(SnackbarController controller, Object data) {
        boolean isFound = false;
        for (SnackbarEntry entry : mStack) {
            if (entry.mData != null && entry.mData.equals(data)
                    && entry.mController == controller) {
                mStack.remove(entry);
                isFound = true;
                break;
            }
        }
        if (!isFound) return;

        finishSnackbarEntryRemoval(controller);
    }

    private void finishSnackbarEntryRemoval(SnackbarController controller) {
        controller.onDismissForEachType(false);

        if (mStack.isEmpty()) {
            dismissSnackbar(false);
        } else {
            // Refresh the snackbar to let it show top of stack and have full timeout.
            showSnackbar(mStack.pop());
        }
    }

    /**
     * Handles click event for action button at end of snackbar.
     */
    @Override
    public void onClick(View v) {
        assert !mStack.isEmpty();

        SnackbarEntry entry = mStack.pop();
        entry.mController.onAction(entry.mData);

        if (!mStack.isEmpty()) {
            showSnackbar(mStack.pop());
        } else {
            dismissSnackbar(false);
        }
    }

    /**
     * Calculates the show-up position from TOP START corner of parent view as a workaround of an
     * android bug http://b/17789629 on Lollipop.
     */
    private void showPopupAtBottom() {
        int margin = mIsTablet ? mParent.getResources().getDimensionPixelSize(
                R.dimen.undo_bar_tablet_margin) : 0;
        mParent.getLocationInWindow(mTempTopLeft);
        mPopup.showAtLocation(mParent, Gravity.START | Gravity.TOP, margin,
                mTempTopLeft[1] + mParent.getHeight() - mPopup.getHeight() - margin);
    }

    /**
     * Resize and re-align popup window when device orientation changes, or soft keyboard shows up.
     */
    @Override
    public void onGlobalLayout() {
        if (mPopup == null) return;
        mParent.getLocationInWindow(mTempTopLeft);
        if (mIsTablet) {
            int margin = mParent.getResources().getDimensionPixelSize(
                    R.dimen.undo_bar_tablet_margin);
            int width = mParent.getResources().getDimensionPixelSize(
                    R.dimen.undo_bar_tablet_width);
            boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(mParent);
            int startPosition = isRtl ? mParent.getRight() - width - margin
                    : mParent.getLeft() + margin;
            mPopup.update(startPosition,
                    mTempTopLeft[1] + mParent.getHeight() - mPopup.getHeight() - margin, width, -1);
        } else {
            // Phone relayout
            mPopup.update(mParent.getLeft(),
                    mTempTopLeft[1] + mParent.getHeight() - mPopup.getHeight(), mParent.getWidth(),
                    -1);
        }
    }

    /**
     * @return Whether there is a snackbar on screen.
     */
    public boolean isShowing() {
        if (mPopup == null) return false;
        return mPopup.isShowing();
    }

    /**
     * Allows overriding the default timeout of {@link #DEFAULT_SNACKBAR_SHOW_DURATION_MS} with
     * a custom value.  This is meant to be used by tests.
     * @param timeoutMs The new timeout to use in ms.
     */
    @VisibleForTesting
    public static void setTimeoutForTesting(int timeoutMs) {
        sUndoBarShowDurationMs = timeoutMs;
        sAccessibilityUndoBarDurationMs = timeoutMs;
    }

    /**
     * Simple data structure representing a single snackbar in stack.
     */
    private static class SnackbarEntry {
        public String mTemplate;
        public String mDescription;
        public String mActionText;
        public Object mData;
        public SnackbarController mController;

        public SnackbarEntry(String template, String description, String actionText,
                Object actionData, SnackbarController controller) {
            mTemplate = template;
            mDescription = description;
            mActionText = actionText;
            mData = actionData;
            mController = controller;
        }
    }
}