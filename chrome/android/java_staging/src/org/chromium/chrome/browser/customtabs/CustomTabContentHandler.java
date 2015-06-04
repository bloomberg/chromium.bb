// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Interface to handle custom tab calls whenever the session id matched.
 * TODO(yusufo): Add a way to handle mayLaunchUrl as well.
 */
public interface CustomTabContentHandler {
    /**
     * Load a new url inside the {@link CustomTabContentHandler}.
     * @param params The params to use while loading the url.
     */
    void loadUrl(LoadUrlParams params);

    /**
     * @return The session id this {@link CustomTabContentHandler} is associated with.
     */
    long getSessionId();
}
