// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.app.Dialog;
import android.os.Build;
import android.view.View;
import android.view.ViewTreeObserver.OnPreDrawListener;

import junit.framework.Assert;

import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.ui.widget.Toast;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Various utils for hardware acceleration tests.
 */
public class Utils {

    /**
     * Asserts that activity is hardware accelerated only on high-end devices.
     * I.e. on low-end devices hardware acceleration must be off.
     */
    public static void assertHardwareAcceleration(ChromeActivity activity) throws Exception {
        assertActivityAcceleration(activity);
        assertChildWindowAcceleration(activity);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Toasts are only HW accelerated on LOLLIPOP+
            assertToastAcceleration(activity);
        }
    }

    /**
     * Asserts that there is no thread named 'RenderThread' (which is essential
     * for hardware acceleration).
     */
    public static void assertNoRenderThread() {
        Assert.assertFalse(collectThreadNames().contains("RenderThread"));
    }

    private static void assertActivityAcceleration(final ChromeActivity activity) throws Exception {
        final AtomicBoolean accelerated = new AtomicBoolean();
        final CallbackHelper listenerCalled = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final View view = activity.getWindow().getDecorView();
                view.getViewTreeObserver().addOnPreDrawListener(new OnPreDrawListener() {
                    @Override
                    public boolean onPreDraw() {
                        view.getViewTreeObserver().removeOnPreDrawListener(this);
                        accelerated.set(view.isHardwareAccelerated());
                        listenerCalled.notifyCalled();
                        return true;
                    }
                });
                view.invalidate();
            }
        });

        listenerCalled.waitForCallback(0);
        assertAcceleration(accelerated);
    }

    private static void assertToastAcceleration(final ChromeActivity activity)
            throws Exception {
        final AtomicBoolean accelerated = new AtomicBoolean();
        final CallbackHelper listenerCalled = new CallbackHelper();

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                // We are using Toast.makeText(context, ...) instead of new Toast(context)
                // because that Toast constructor is unused and is deleted by proguard.
                Toast toast = Toast.makeText(activity, "", Toast.LENGTH_SHORT);
                toast.setView(new View(activity) {
                    @Override
                    public void onAttachedToWindow() {
                        super.onAttachedToWindow();
                        accelerated.set(isHardwareAccelerated());
                        listenerCalled.notifyCalled();
                    }
                });
                toast.show();
            }
        });

        listenerCalled.waitForCallback(0);
        assertAcceleration(accelerated);
    }

    private static void assertChildWindowAcceleration(final ChromeActivity activity)
            throws Exception {
        final AtomicBoolean accelerated = new AtomicBoolean();
        final CallbackHelper listenerCalled = new CallbackHelper();

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                final Dialog dialog = new Dialog(activity);
                dialog.setContentView(new View(activity) {
                    @Override
                    public void onAttachedToWindow() {
                        super.onAttachedToWindow();
                        accelerated.set(isHardwareAccelerated());
                        listenerCalled.notifyCalled();
                        dialog.dismiss();
                    }
                });
                dialog.show();
            }
        });

        listenerCalled.waitForCallback(0);
        assertAcceleration(accelerated);
    }

    private static void assertAcceleration(AtomicBoolean accelerated) {
        if (SysUtils.isLowEndDevice()) {
            Assert.assertFalse(accelerated.get());
        } else {
            Assert.assertTrue(accelerated.get());
        }
    }

    private static Set<String> collectThreadNames() {
        Set<String> names = new HashSet<String>();
        for (Thread thread : Thread.getAllStackTraces().keySet()) {
            names.add(thread.getName());
        }
        return names;
    }
}
