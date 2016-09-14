// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch.action;

/**
 * Listens to notifications sent by the {@link SearchAction} class.
 * This is part of the 2016-refactoring (crbug.com/624609, go/cs-refactoring-2016).
 */
public class SearchActionListener {
    /**
     * Indicates that the Contextual Search Context, which includes the selected word
     * and surrounding text, is ready to be accessed.
     * @param action The {@link SearchAction} whose context is ready.
     */
    protected void onContextReady(SearchAction action) {}

    /**
     * Indicates that the {@code SearchAction} has been accepted (not suppressed).
     * @param action The {@link SearchAction} that has been accepted.
     */
    protected void onActionAccepted(SearchAction action) {}

    /**
     * Indicates that the {@code SearchAction} has been suppressed (not accepted).
     * @param action The {@link SearchAction} that has been suppressed.
     */
    protected void onActionSuppressed(SearchAction action) {}

    /**
     * Indicates that the {@code SearchAction} has ended.
     * @param action The {@link SearchAction} that has ended.
     */
    protected void onActionEnded(SearchAction action) {}
}
