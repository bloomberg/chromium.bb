// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.ViewGroup;

/**
 * Listener that is notified when the container view is replaced.
 * See {@link ContentViewCore#setContainerView} for additional details.
 */
public abstract class ContainerViewObserver {
    public void onContainerViewChanged(ViewGroup newContainerView) {
        // Intentional no-op.
    }
}

