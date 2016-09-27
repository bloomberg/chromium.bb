// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp_public;

import android.preference.PreferenceFragment;

import org.chromium.blimp_public.contents.BlimpContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.Map;

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
     * @param windowAndroid the window this BlimpContents will be displayed in.
     */
    BlimpContents createBlimpContents(WindowAndroid windowAndroid);

    /**
     * @return If Blimp is supported with this build.
     */
    boolean isBlimpSupported();

    /**
     * @return If Blimp is enabled in settings UI by the user. Or if we have development mode
     * command line arguments.
     */
    boolean isBlimpEnabled();

    /**
     * Attach blimp settings UI to a {@link PreferenceFragment}
     * @param fragment PreferenceFragment that blimp settings UI attached to.
     */
    void attachBlimpPreferences(PreferenceFragment fragment);

    /**
     * Set the {@link BlimpClientContextDelegate}, functions in this interface should be used in
     * Java Blimp code only.
     */
    void setDelegate(BlimpClientContextDelegate delegate);

    /**
     * Start authentication flow and connection to engine.
     */
    void connect();

    /**
     * Gathers data about Blimp that should be send for feedback reports.
     * @return a map of all the Blimp-related feedback data.
     */
    Map<String, String> getFeedbackMap();
}
