// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp_public.contents;

/**
 * BlimpContentsObserver is the Java representation of a native BlimpContentsObserver object.
 *
 * The BlimpContentsObserver is an observer API implemented by classes which are interested in
 * various events related to {@link BlimpContents}.
 */
public interface BlimpContentsObserver {
    /**
     * Invoked when the BlimpContents's navigation state is changed.
     */
    void onNavigationStateChanged();
}
