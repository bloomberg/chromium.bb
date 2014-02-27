// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Proxy;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewStatics;
import org.chromium.net.ProxyChangeListener;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 *  Tests for ContentView methods that don't fall into any other category.
 */
public class ContentViewMiscTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mContentViewCore = testContainerView.getContentViewCore();
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFindAddress() {
        assertNull(ContentViewStatics.findAddress("This is some random text"));

        String googleAddr = "1600 Amphitheatre Pkwy, Mountain View, CA 94043";
        String testString = "Address: " + googleAddr + "  in a string";
        assertEquals(googleAddr, ContentViewStatics.findAddress(testString));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testEnableDisablePlatformNotifications() {

        // Set up mock contexts to use with the listener
        final AtomicReference<BroadcastReceiver> receiverRef =
                new AtomicReference<BroadcastReceiver>();
        final MockContext appContext = new MockContext() {
            @Override
            public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
                receiverRef.set(receiver);
                return null;
            }
        };
        final MockContext context = new MockContext() {
            @Override
            public Context getApplicationContext() {
                return appContext;
            }
        };

        // Set up a delegate so we know when native code is about to get
        // informed of a proxy change.
        final AtomicBoolean proxyChanged = new AtomicBoolean();
        final ProxyChangeListener.Delegate delegate = new ProxyChangeListener.Delegate() {
            @Override
            public void proxySettingsChanged() {
                proxyChanged.set(true);
            }
        };
        Intent intent = new Intent();
        intent.setAction(Proxy.PROXY_CHANGE_ACTION);

        // Create the listener that's going to be used for the test
        ProxyChangeListener listener = ProxyChangeListener.create(context);
        listener.setDelegateForTesting(delegate);
        listener.start(0);

        // Start the actual tests

        // Make sure everything works by default
        proxyChanged.set(false);
        receiverRef.get().onReceive(context, intent);
        assertEquals(true, proxyChanged.get());

        // Now disable platform notifications and make sure we don't notify
        // native code.
        proxyChanged.set(false);
        ContentViewStatics.disablePlatformNotifications();
        receiverRef.get().onReceive(context, intent);
        assertEquals(false, proxyChanged.get());

        // Now re-enable notifications to make sure they work again.
        ContentViewStatics.enablePlatformNotifications();
        receiverRef.get().onReceive(context, intent);
        assertEquals(true, proxyChanged.get());
    }
}
