// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * Represents the MessageChannel object. Inspired from
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/web-messaging.html#message-channels
 */
public class MessageChannel {
    // The message ports of MessageChannel.
    private MessagePort mPort1;
    private MessagePort mPort2;

    public MessageChannel(MessagePort port1, MessagePort port2) {
        mPort1 = port1;
        mPort2 = port2;
    }

    public MessagePort port1() {
        return mPort1;
    }

    public MessagePort port2() {
        return mPort2;
    }
}
