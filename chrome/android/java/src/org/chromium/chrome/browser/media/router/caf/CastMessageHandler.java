// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

/**
 * The handler for cast messages. It receives events between the Cast SDK and the page, process and
 * dispatch the messages accordingly. The handler talks to the Cast SDK via CastSession, and
 * talks to the pages via the media router.
 */
public class CastMessageHandler {
    // Sequence number used when no sequence number is required or was initially passed.
    static final int INVALID_SEQUENCE_NUMBER = -1;
}
