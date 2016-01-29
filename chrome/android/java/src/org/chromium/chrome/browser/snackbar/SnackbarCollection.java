// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;

import java.util.Deque;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;

/**
 * A data structure that holds all the {@link Snackbar}s managed by {@link SnackbarManager}.
 */
class SnackbarCollection {
    private Deque<Snackbar> mStack = new LinkedList<>();
    private Queue<Snackbar> mQueue = new LinkedList<>();

    /**
     * Adds a new snackbar to the collection. If the new snackbar is of
     * {@link Snackbar#TYPE_ACTION} and current snackbar is of
     * {@link Snackbar#TYPE_NOTIFICATION}, the current snackbar will be removed from the
     * collection immediately.
     */
    void add(Snackbar snackbar) {
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
     * Removes the current snackbar from the collection after the user has clicked on the action
     * button.
     */
    Snackbar removeCurrentDueToAction() {
        return removeCurrent(true);
    }

    private Snackbar removeCurrent(boolean isAction) {
        Snackbar current = !mStack.isEmpty() ? mStack.pop() : mQueue.poll();
        if (current != null) {
            SnackbarController controller = current.getController();
            if (isAction) controller.onAction(current.getActionData());
            else controller.onDismissNoAction(current.getActionData());
        }
        return current;
    }

    /**
     * @return The snackbar that is currently displayed.
     */
    Snackbar getCurrent() {
        return !mStack.isEmpty() ? mStack.peek() : mQueue.peek();
    }

    boolean isEmpty() {
        return mStack.isEmpty() && mQueue.isEmpty();
    }

    void clear() {
        while (!isEmpty()) {
            removeCurrent(false);
        }
    }

    void removeCurrentDueToTimeout() {
        removeCurrent(false);
        Snackbar current;
        while ((current = getCurrent()) != null && current.isTypeAction()) {
            removeCurrent(false);
        }
    }

    boolean removeMatchingSnackbars(SnackbarController controller) {
        boolean snackbarRemoved = false;
        Iterator<Snackbar> iter = mStack.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();
            if (snackbar.getController() == controller) {
                iter.remove();
                snackbarRemoved = true;
            }
        }
        iter = mQueue.iterator();
        while (iter.hasNext()) {
            Snackbar snackbar = iter.next();
            if (snackbar.getController() == controller) {
                iter.remove();
                snackbarRemoved = true;
            }
        }
        return snackbarRemoved;
    }

    boolean removeMatchingSnackbars(SnackbarController controller, Object data) {
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
        iter = mQueue.iterator();
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