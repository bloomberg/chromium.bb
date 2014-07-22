// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

/**
 * A class which implements this interface can receive {@link Message} objects.
 */
public interface MessageReceiver {

    /**
     * Receive a {@link MessageWithHeader}. The {@link MessageReceiver} is allowed to mutable the
     * message. Returns |true| if the message has been handled, |false| otherwise.
     */
    boolean accept(MessageWithHeader message);
}
