// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Handler;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;

import org.chromium.content.browser.AppWebMessagePort;

/**
 * This class is used to convert a WebMessagePort to a MessagePort in chromium
 * world.
 */
@TargetApi(Build.VERSION_CODES.M)
public class WebMessagePortAdapter extends WebMessagePort {
    private AppWebMessagePort mPort;

    public WebMessagePortAdapter(AppWebMessagePort port) {
        mPort = port;
    }

    public void postMessage(WebMessage message) {
        mPort.postMessage(message.getData(), toAppWebMessagePorts(message.getPorts()));
    }

    public void close() {
        mPort.close();
    }

    public void setWebMessageCallback(WebMessageCallback callback) {
        setWebMessageCallback(callback, null);
    }

    public void setWebMessageCallback(final WebMessageCallback callback, final Handler handler) {
        mPort.setMessageCallback(new AppWebMessagePort.MessageCallback() {
            @Override
            public void onMessage(String message, AppWebMessagePort[] ports) {
                callback.onMessage(WebMessagePortAdapter.this,
                        new WebMessage(message, fromAppWebMessagePorts(ports)));
            }
        }, handler);
    }

    public AppWebMessagePort getPort() {
        return mPort;
    }

    public static WebMessagePort[] fromAppWebMessagePorts(AppWebMessagePort[] messagePorts) {
        if (messagePorts == null) return null;
        WebMessagePort[] ports = new WebMessagePort[messagePorts.length];
        for (int i = 0; i < messagePorts.length; i++) {
            ports[i] = new WebMessagePortAdapter(messagePorts[i]);
        }
        return ports;
    }

    public static AppWebMessagePort[] toAppWebMessagePorts(WebMessagePort[] webMessagePorts) {
        if (webMessagePorts == null) return null;
        AppWebMessagePort[] ports = new AppWebMessagePort[webMessagePorts.length];
        for (int i = 0; i < webMessagePorts.length; i++) {
            ports[i] = ((WebMessagePortAdapter) webMessagePorts[i]).getPort();
        }
        return ports;
    }
}
