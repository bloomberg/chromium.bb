// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.process_launcher.ChildProcessConnection;

/**
 * Manages an LRU list of connections to control moderate binding. Binding is added when inserted
 * into the manager, and removed based on signals such as backgrounded, or removed explicitly.
 *
 * Thread-safety: most of the methods are called on the launcher thread.
 */
interface BindingManager {
    /** Inserts or moves to the most recent entry. */
    void increaseRecency(ChildProcessConnection connection);

    /** Removes from manager if present. */
    void dropRecency(ChildProcessConnection connection);

    /**
     * Called when the embedding application is sent to background.
     * The embedder needs to ensure that:
     *  - every onBroughtToForeground() is followed by onSentToBackground()
     *  - pairs of consecutive onBroughtToForeground() / onSentToBackground() calls do not overlap
     */
    void onSentToBackground();

    /** Called when the embedding application is brought to foreground. */
    void onBroughtToForeground();

    /** Releases all moderate bindings. */
    void releaseAllModerateBindings();
}
