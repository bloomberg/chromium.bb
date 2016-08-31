// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import org.chromium.blimp_public.BlimpClientContextDelegate;

/**
 * BlimpPreferencesDelegate provides functionalities needed by preferences UX.
 */
public interface BlimpPreferencesDelegate {
    /**
     * @return The {@link BlimpClientContextDelegate} that contains embedder functionalities.
     */
    BlimpClientContextDelegate getDelegate();

    /**
     * Initialize setting page.
     * Setup all helper objects that the setting page needs, such as IdentitySource in native code.
     */
    void initSettingsPage(AboutBlimpPreferences preferences);

    /**
     * Start authentication flow and connection to engine.
     */
    void connect();
}
