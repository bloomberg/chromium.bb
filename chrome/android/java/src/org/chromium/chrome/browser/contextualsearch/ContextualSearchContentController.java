// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;


/**
 * An interface loading and managing content in the contextual search panel.
 */
public interface ContextualSearchContentController {

    /**
     * Loads a URL in the search content view.
     * @param url the URL of the page to load.
     */
    void loadUrl(String url);

    // --------------------------------------------------------------------------------------------
    // These need to be stubbed out for testing.
    // --------------------------------------------------------------------------------------------

    /**
     * Creates and sets up a new Search Panel's {@code ContentViewCore}. If there's an existing
     * {@code ContentViewCore} being used, it will be destroyed first. This should be called as
     * late as possible to avoid unnecessarily consuming memory.
     */
    void createNewContentView();

    /**
     * Destroys the Search Panel's {@code ContentViewCore}.
     */
    void destroyContentView();
}
