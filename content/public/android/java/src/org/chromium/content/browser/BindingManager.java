// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

/**
 * Manages oom bindings used to bound child services. "Oom binding" is a binding that raises the
 * process oom priority so that it shouldn't be killed by the OS out-of-memory killer under
 * normal conditions (it can still be killed under drastic memory pressure). ChildProcessConnections
 * have two oom bindings: initial binding and strong binding.
 *
 * BindingManager receives calls that signal status of each service (setInForeground(),
 * determinedVisibility()) and the entire embedding application (onSentToBackground(),
 * onBroughtToForeground()) and manipulates child process bindings accordingly.
 *
 * In particular, BindingManager is responsible for:
 * - adding and removing the strong binding as service visibility changes (setInForeground())
 * - removing the initial binding of a service when we can start to rely on the visibility signal /
 *   strong binding exclusively (after determinedVisibility())
 * - dropping the current oom bindings when a new connection is started on a low-memory device
 * - keeping a strong binding on the foreground service while the entire application is in
 *   background
 *
 * Thread-safety: most of the methods will be called only on the main thread, exceptions are
 * explicitly noted.
 */
public interface BindingManager {
    /**
     * Registers a freshly started child process. This can be called on any thread.
     * @param pid handle of the service process
     */
    void addNewConnection(int pid, ManagedChildProcessConnection connection);

    /**
     * Called when the service visibility changes or is determined for the first time. On low-memory
     * devices this will also drop the oom bindings of the last process that was oom-bound if a new
     * process is used in foreground.
     * @param pid handle of the service process
     * @param inForeground true iff the service is visibile to the user
     */
    void setInForeground(int pid, boolean inForeground);

    /**
     * Called when we can begin to rely on the visibility signal only and remove the initial
     * binding. It's safe to call it multiple times, only the first call matters.
     * @param pid handle of the service process
     */
    void onDeterminedVisibility(int pid);

    /**
     * Called when the embedding application is sent to background. We want to maintain a strong
     * binding on the most recently used renderer while the embedder is in background, to indicate
     * the relative importance of the renderer to system oom killer.
     *
     * The embedder needs to ensure that:
     *  - every onBroughtToForeground() is followed by onSentToBackground()
     *  - pairs of consecutive onBroughtToForeground() / onSentToBackground() calls do not overlap
     */
    void onSentToBackground();

    /**
     * Called when the embedding application is brought to foreground. This will drop the strong
     * binding kept on the main renderer during the background period, so the embedder should make
     * sure that this is called after the regular strong binding is attached for the foreground
     * session.
     */
    void onBroughtToForeground();

    /**
     * Should be called when the connection to the child process goes away (either after a clean
     * exit or an unexpected crash). At this point we let go of the reference to the
     * ChildProcessConnection. This can be called on any thread.
     */
    void removeConnection(int pid);

    /**
     * Starts moderate binding management.
     * Please see https://goo.gl/tl9MQm for details.
     */
    void startModerateBindingManagement(Context context, int maxSize);

    /**
     * Releases all moderate bindings.
     */
    void releaseAllModerateBindings();
}
