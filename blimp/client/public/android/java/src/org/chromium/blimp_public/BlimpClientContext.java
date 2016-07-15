// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp_public;

/**
 * BlimpClientContext is the Java representation of a native BlimpClientContext object.
 * It is owned by the native BrowserContext.
 *
 * BlimpClientContext is the core class for the Blimp client. It provides hooks for creating
 * BlimpContents and other features that are per BrowserContext/Profile.
 */
public interface BlimpClientContext {
    /**
     * Creates a {@link BlimpContents} and takes ownership of it. The caller must call
     * {@link BlimpContents#destroy()} for destruction of the BlimpContents.
     */
    BlimpContents createBlimpContents();
}
