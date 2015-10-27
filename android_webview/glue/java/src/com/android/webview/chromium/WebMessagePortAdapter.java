// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Handler;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;

import org.chromium.android_webview.AwMessagePort;

/**
 * This class is used to convert a WebMessagePort to a MessagePort in chromium
 * world.
 */
@TargetApi(Build.VERSION_CODES.M)
public class WebMessagePortAdapter extends WebMessagePort {

    private AwMessagePort mPort;

    public WebMessagePortAdapter(AwMessagePort port) {
        mPort = port;
    }

    public void postMessage(WebMessage message) {
        mPort.postMessage(message.getData(), toAwMessagePorts(message.getPorts()));
    }

    public void close() {
        mPort.close();
    }

    public void setWebMessageCallback(WebMessageCallback callback) {
        setWebMessageCallback(callback, null);
    }

    public void setWebMessageCallback(final WebMessageCallback callback, final Handler handler) {
        mPort.setMessageCallback(new AwMessagePort.MessageCallback() {
            @Override
            public void onMessage(String message, AwMessagePort[] ports) {
                callback.onMessage(WebMessagePortAdapter.this,
                        new WebMessage(message, fromAwMessagePorts(ports)));
            }
        }, handler);
    }

    public AwMessagePort getPort() {
        return mPort;
    }

    public static WebMessagePort[] fromAwMessagePorts(AwMessagePort[] messagePorts) {
        if (messagePorts == null) return null;
        WebMessagePort[] ports = new WebMessagePort[messagePorts.length];
        for (int i = 0; i < messagePorts.length; i++) {
            ports[i] = new WebMessagePortAdapter(messagePorts[i]);
        }
        return ports;
    }

    public static AwMessagePort[] toAwMessagePorts(WebMessagePort[] webMessagePorts) {
        if (webMessagePorts == null) return null;
        AwMessagePort[] ports = new AwMessagePort[webMessagePorts.length];
        for (int i = 0; i < webMessagePorts.length; i++) {
            ports[i] = ((WebMessagePortAdapter) webMessagePorts[i]).getPort();
        }
        return ports;
    }
}
