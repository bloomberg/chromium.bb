// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * Represents the MessageChannel object. Inspired from
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/web-messaging.html#message-channels
 */
public class MessageChannel {
    // The message port IDs of port1 and port2.
    private int mPort1;
    private int mPort2;

    public MessageChannel(int port1, int port2) {
        mPort1 = port1;
        mPort2 = port2;
    }

    public int port1() {
        return mPort1;
    }

    public int port2() {
        return mPort2;
    }
}
