// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * An interface for creating and managing channels with {@link MessagePort}s on both ends.
 */
public interface MessagePortService {
    /**
     * @return Two {@link MessagePort}s that forms the ends of a message channel created. The user
     *         of the API should send one of these to the main frame of the tab that should be
     *         receiving the messages and hold onto the other one.
     */
    MessagePort[] createMessageChannel();
}
