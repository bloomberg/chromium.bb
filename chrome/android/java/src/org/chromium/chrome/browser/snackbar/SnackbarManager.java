// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.graphics.Rect;
import android.os.Handler;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.Window;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Deque;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;

/**
 * Manager for the snackbar showing at the bottom of activity. There should be only one
 * SnackbarManager and one snackbar in the activity.
 * <p/>
 * When action button is clicked, this manager will call {@link SnackbarController#onAction(Object)}
 * in corresponding listener, and show the next entry. Otherwise if no action is taken by user
 * during {@link #DEFAULT_SNACKBAR_DURATION_MS} milliseconds, it will call
 * {@link SnackbarController#onDismissNoAction(Object)}.
 */
public class SnackbarManager implements OnClickListener, OnGlobalLayoutListener {

    /**
     * Interface that shows the ability to provide a snackbar manager. Activities implementing this
     * interface must call {@link SnackbarManager#onStart()} and {@link SnackbarManager#onStop()} in
     * corresponding lifecycle events.
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
         * Called when the user clicks the action button on the snackbar.
         * @param actionData Data object passed when showing this specific snackbar.
         */
        void onAction(Object actionData);

        /**
         * Called when the snackbar is dismissed by tiemout or UI enviroment change.
         * @param actionData Data object associated with the dismissed snackbar entry.
         */
        void onDismissNoAction(Object actionData);
    }

    private static final int DEFAULT_SNACKBAR_DURATION_MS = 3000;
    private static final int ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS = 6000;

    // Used instead of the constant so tests can override the value.
    private static int sSnackbarDurationMs = DEFAULT_SNACKBAR_DURATION_MS;
    private static int sAccessibilitySnackbarDurationMs = ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS;

    private final boolean mIsTablet;

    private View mDecor;
    private final Handler mUIThreadHandler;
    private SnackbarCollection mSnackbars = new SnackbarCollection();
    private SnackbarPopupWindow mPopup;
    private boolean mActivityInForeground;
    private final Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            mSnackbars.removeCurrentDueToTimeout();
            updatePopup();
        }
    };

    // Variables used and reused in popup position calculations.
    private int[] mTempDecorPosition = new int[2];
    private Rect mTempVisibleDisplayFrame = new Rect();

    /**
     * Constructs a SnackbarManager to show snackbars in the given window.
     */
    public SnackbarManager(Window window) {
        mDecor = window.getDecorView();
        mUIThreadHandler = new Handler();
        mIsTablet = DeviceFormFactor.isTablet(mDecor.getContext());
    }

    /**
     * Notifies the snackbar manager that the activity is running in foreground now.
     */
    public void onStart() {
        mActivityInForeground = true;
    }

    /**
     * Notifies the snackbar manager that the activity has been pushed to background.
     */
    public void onStop() {
        mSnackbars.clear();
        updatePopup();
        mActivityInForeground = false;
    }

    /**
     * Shows a snackbar at the bottom of the screen, or above the keyboard if the keyboard is
     * visible.
     */
    public void showSnackbar(Snackbar snackbar) {
        if (!mActivityInForeground) return;
        mSnackbars.add(snackbar);
        updatePopup();
        mPopup.announceforAccessibility();
    }

    /**
     * Dismisses snackbars that are associated with the given {@link SnackbarController}.
     *
     * @param controller Only snackbars with this controller will be removed.
     */
    public void dismissSnackbars(SnackbarController controller) {
        if (mSnackbars.removeMatchingSnackbars(controller)) {
            updatePopup();
        }
    }

    /**
     * Dismisses snackbars that have a certain controller and action data.
     *
     * @param controller Only snackbars with this controller will be removed.
     * @param actionData Only snackbars whose action data is equal to actionData will be removed.
     */
    public void dismissSnackbars(SnackbarController controller, Object actionData) {
        if (mSnackbars.removeMatchingSnackbars(controller, actionData)) {
            updatePopup();
        }
    }

    /**
     * Handles click event for action button at end of snackbar.
     */
    @Override
    public void onClick(View v) {
        mSnackbars.removeCurrent(true);
        updatePopup();
    }

    /**
     * @return Whether there is a snackbar on screen.
     */
    public boolean isShowing() {
        return mPopup != null && mPopup.isShowing();
    }

    /**
     * Resize and re-position popup window when the device orientation changes or the software
     * keyboard appears. Be careful not to let the snackbar overlap the Android navigation bar:
     * http://b/17789629.
     */
    @Override
    public void onGlobalLayout() {
        if (mPopup == null) return;

        mDecor.getLocationInWindow(mTempDecorPosition);
        mDecor.getWindowVisibleDisplayFrame(mTempVisibleDisplayFrame);
        int decorBottom = mTempDecorPosition[1] + mDecor.getHeight();
        int visibleBottom = Math.min(mTempVisibleDisplayFrame.bottom, decorBottom);

        if (mIsTablet) {
            int margin = mDecor.getResources().getDimensionPixelOffset(
                    R.dimen.snackbar_tablet_margin);
            int width = mDecor.getResources().getDimensionPixelSize(R.dimen.snackbar_tablet_width);
            boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(mDecor);
            int startPosition = isRtl ? mDecor.getRight() - width - margin
                    : mDecor.getLeft() + margin;
            mPopup.update(startPosition, decorBottom - visibleBottom + margin, width, -1);
        } else {
            mPopup.update(mDecor.getLeft(), decorBottom - visibleBottom, mDecor.getWidth(), -1);
        }
    }

    /**
     * Updates the snackbar popup window to reflect the value of mSnackbars.currentSnackbar(), which
     * may be null. This might show, change, or hide the popup.
     */
    private void updatePopup() {
        if (!mActivityInForeground) return;
        Snackbar currentSnackbar = mSnackbars.getCurrent();
        if (currentSnackbar == null) {
            mUIThreadHandler.removeCallbacks(mHideRunnable);
            if (mPopup != null) {
                mPopup.dismiss();
                mPopup = null;
            }
            mDecor.getViewTreeObserver().removeOnGlobalLayoutListener(this);
        } else {
            boolean popupChanged = true;
            if (mPopup == null) {
                mPopup = new SnackbarPopupWindow(mDecor, this, currentSnackbar);
                // When the keyboard is showing, translating the snackbar upwards looks bad because
                // it overlaps the keyboard. In this case, use an alternative animation without
                // translation.
                boolean isKeyboardShowing = UiUtils.isKeyboardShowing(mDecor.getContext(), mDecor);
                mPopup.setAnimationStyle(isKeyboardShowing ? R.style.SnackbarAnimationWithKeyboard
                        : R.style.SnackbarAnimation);

                mDecor.getLocationInWindow(mTempDecorPosition);
                mDecor.getWindowVisibleDisplayFrame(mTempVisibleDisplayFrame);
                int decorBottom = mTempDecorPosition[1] + mDecor.getHeight();
                int visibleBottom = Math.min(mTempVisibleDisplayFrame.bottom, decorBottom);
                int margin = mIsTablet ? mDecor.getResources().getDimensionPixelSize(
                        R.dimen.snackbar_tablet_margin) : 0;

                mPopup.showAtLocation(mDecor, Gravity.START | Gravity.BOTTOM, margin,
                        decorBottom - visibleBottom + margin);
                mDecor.getViewTreeObserver().addOnGlobalLayoutListener(this);
            } else {
                popupChanged = mPopup.update(currentSnackbar);
            }

            if (popupChanged) {
                int durationMs = getDuration(currentSnackbar);
                mUIThreadHandler.removeCallbacks(mHideRunnable);
                mUIThreadHandler.postDelayed(mHideRunnable, durationMs);
                mPopup.announceforAccessibility();
            }
        }

    }

    private int getDuration(Snackbar snackbar) {
        int durationMs = snackbar.getDuration();
        if (durationMs == 0) {
            durationMs = DeviceClassManager.isAccessibilityModeEnabled(mDecor.getContext())
                    ? sAccessibilitySnackbarDurationMs : sSnackbarDurationMs;
        }
        return durationMs;
    }

    /**
     * Overrides the default snackbar duration with a custom value for testing.
     * @param durationMs The duration to use in ms.
     */
    @VisibleForTesting
    public static void setDurationForTesting(int durationMs) {
        sSnackbarDurationMs = durationMs;
        sAccessibilitySnackbarDurationMs = durationMs;
    }

    /**
     * @return The currently showing snackbar. For testing only.
     */
    @VisibleForTesting
    Snackbar getCurrentSnackbarForTesting() {
        return mSnackbars.getCurrent();
    }

    private static class SnackbarCollection {
        private Deque<Snackbar> mStack = new LinkedList<>();
        private Queue<Snackbar> mQueue = new LinkedList<>();

        /**
         * Adds a new snackbar to the collection. If the new snackbar is of
         * {@link Snackbar#TYPE_ACTION} and current snackbar is of
         * {@link Snackbar#TYPE_NOTIFICATION}, the current snackbar will be removed from the
         * collection immediately.
         */
        public void add(Snackbar snackbar) {
            if (snackbar.isTypeAction()) {
                if (getCurrent() != null && !getCurrent().isTypeAction()) {
                    removeCurrent(false);
                }
                mStack.push(snackbar);
            } else {
                mQueue.offer(snackbar);
            }
        }

        /**
         * Removes the current snackbar from the collection.
         * @param isAction Whether the removal is triggered by user clicking the action button.
         */
        public void removeCurrent(boolean isAction) {
            Snackbar current = !mStack.isEmpty() ? mStack.pop() : mQueue.poll();
            if (current != null) {
                SnackbarController controller = current.getController();
                if (isAction) controller.onAction(current.getActionData());
                else controller.onDismissNoAction(current.getActionData());
            }
        }

        /**
         * @return The snackbar that is currently displayed.
         */
        public Snackbar getCurrent() {
            return !mStack.isEmpty() ? mStack.peek() : mQueue.peek();
        }

        public boolean isEmpty() {
            return mStack.isEmpty() && mQueue.isEmpty();
        }

        public void clear() {
            while (!isEmpty()) {
                removeCurrent(false);
            }
        }

        public void removeCurrentDueToTimeout() {
            removeCurrent(false);
            Snackbar current;
            while ((current = getCurrent()) != null && current.isTypeAction()) {
                removeCurrent(false);
            }
        }

        public boolean removeMatchingSnackbars(SnackbarController controller) {
            boolean snackbarRemoved = false;
            Iterator<Snackbar> iter = mStack.iterator();
            while (iter.hasNext()) {
                Snackbar snackbar = iter.next();
                if (snackbar.getController() == controller) {
                    iter.remove();
                    snackbarRemoved = true;
                }
            }
            return snackbarRemoved;
        }

        public boolean removeMatchingSnackbars(SnackbarController controller, Object data) {
            boolean snackbarRemoved = false;
            Iterator<Snackbar> iter = mStack.iterator();
            while (iter.hasNext()) {
                Snackbar snackbar = iter.next();
                if (snackbar.getController() == controller
                        && objectsAreEqual(snackbar.getActionData(), data)) {
                    iter.remove();
                    snackbarRemoved = true;
                }
            }
            return snackbarRemoved;
        }

        private static boolean objectsAreEqual(Object a, Object b) {
            if (a == null && b == null) return true;
            if (a == null || b == null) return false;
            return a.equals(b);
        }
    }
}
