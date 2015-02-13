// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestBase;

import java.io.IOException;

/**
 * Tests the MessageLoop implementation.
 */
public class MessageLoopTest extends CronetTestBase {

    @SmallTest
    @Feature({"Cronet"})
    public void testInterrupt() throws Exception {
        final MessageLoop loop = new MessageLoop();
        assertFalse(loop.isRunning());
        TestThread thread = new TestThread() {
            @Override
            public void run() {
                try {
                    loop.loop();
                    mFailed = true;
                } catch (IOException e) {
                    // Expected interrupt.
                }
            }
        };
        thread.start();
        Thread.sleep(1000);
        assertTrue(loop.isRunning());
        assertFalse(loop.hasLoopFailed());
        thread.interrupt();
        Thread.sleep(1000);
        assertFalse(loop.isRunning());
        assertTrue(loop.hasLoopFailed());
        assertFalse(thread.mFailed);
        // Re-spinning the message loop is not allowed after interrupt.
        try {
            loop.loop();
            fail();
        } catch (IllegalStateException e) {
            // Expected.
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testTaskFailed() throws Exception {
        final MessageLoop loop = new MessageLoop();
        assertFalse(loop.isRunning());
        TestThread thread = new TestThread() {
            @Override
            public void run() {
                try {
                    loop.loop();
                    mFailed = true;
                } catch (Exception e) {
                    if (!(e instanceof NullPointerException)) {
                        mFailed = true;
                    }
                }
            }
        };
        Runnable failedTask = new Runnable() {
            @Override
            public void run() {
                throw new NullPointerException();
            }
        };
        thread.start();
        Thread.sleep(1000);
        assertTrue(loop.isRunning());
        assertFalse(loop.hasLoopFailed());
        loop.execute(failedTask);
        Thread.sleep(1000);
        assertFalse(loop.isRunning());
        assertTrue(loop.hasLoopFailed());
        assertFalse(thread.mFailed);
        // Re-spinning the message loop is not allowed after exception.
        try {
            loop.loop();
            fail();
        } catch (IllegalStateException e) {
            // Expected.
        }
    }

    /**
     * A Thread class to move assertion to the main thread, so findbug
     * won't complain.
     */
    private class TestThread extends Thread {
        boolean mFailed = false;

        public TestThread() {
        }

        @Override
        public void run() {
            throw new IllegalStateException();
        }
    }
}
