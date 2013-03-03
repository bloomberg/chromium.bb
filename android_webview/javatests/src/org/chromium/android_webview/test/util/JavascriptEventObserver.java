// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.chromium.content.browser.ContentViewCore;

/**
 * This class is used to be notified when a javascript event happened. It add itself as
 * a javascript interface, so it could be notified by javascript when needed.
 *
 * 1. Call register() is to add a javascript interface into ContentViewCore.
 * 2. Using waitForEnvent() to wait javascript event.
 * 3. In javascript call notifyJava() when you want Java side know something is done.
 */
public class JavascriptEventObserver {
    private Object mEvent = new Object();
    private boolean mHappened;

    /**
     * Register into javascript, must be called in UI thread.
     *
     * @param contentViewCore
     * @param name the name of object used in javascript
     */
    public void register(ContentViewCore contentViewCore, String name) {
        contentViewCore.addPossiblyUnsafeJavascriptInterface(this, name, null);
    }

    /**
     * Wait for the javascript event happen for specific time, there is no timeout parameter,
     * return true if the event happened.
     */
    public boolean waitForEvent(long time) throws InterruptedException {
        synchronized (mEvent) {
            if (mHappened) return mHappened;
            mEvent.wait(time);
            boolean happened = mHappened;
            mHappened = false;
            return happened;
        }
    }

    /**
     * Wait for the javascript event happen, there is no timeout parameter, you usually
     * should depend on unit test's timeout.
     */
    public void waitForEvent() throws InterruptedException {
        synchronized (mEvent) {
            if (mHappened) return;
            while (!mHappened) {
                mEvent.wait();
            }
            mHappened = false;
        }
    }

    /**
     * Javascript should call this method by name.notifyJava, the name is the |name|
     * parameter of register() method.
     */
    public void notifyJava() {
        synchronized (mEvent) {
            mHappened = true;
            mEvent.notify();
        }
    }
}
