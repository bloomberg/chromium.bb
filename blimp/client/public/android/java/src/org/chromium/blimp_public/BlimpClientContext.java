// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp_public;

import android.preference.PreferenceFragment;

import org.chromium.blimp_public.contents.BlimpContents;

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

    /**
     * @return If Blimp is supported with this build.
     */
    boolean isBlimpSupported();

    /**
     * @return If Blimp is enabled by the user.
     */
    boolean isBlimpEnabled();

    /**
     * Attach blimp settings UI to a {@link PreferenceFragment}
     * @param fragment PreferenceFragment that blimp settings UI attached to.
     * @param callback Chrome layer callbacks that passed to Blimp.
     */
    void attachBlimpPreferences(PreferenceFragment fragment, BlimpSettingsCallbacks callback);

    /**
     * Set the {@link BlimpClientContextDelegate}, functions in this interface should be used in
     * Java Blimp code only.
     */
    void setDelegate(BlimpClientContextDelegate delegate);

    /**
     * Start authentication flow and connection to engine.
     * This must be called after AccountTrackerService.onSystemAccountsSeedingComplete, since the
     * embedder may asynchronously seed account info to native layer, and revoke all OAuth2 refresh
     * token during the request.
     */
    void connect();
}
