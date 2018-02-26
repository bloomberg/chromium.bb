// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Application;
import android.content.ComponentCallbacks2;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;

/**
 * Tests for {@link MemoryPressureListener}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class MemoryPressureListenerTest {
    /** Helper class to capture and validate OnMemoryPressureCallback calls.
     */
    private static class MemoryPressureCallback {
        private boolean mCalled;
        private @MemoryPressureLevel int mPressure = MemoryPressureLevel.NONE;

        public void install() {
            MemoryPressureListener.setOnMemoryPressureCallbackForTesting(this::onMemoryPressure);
        }

        public void uninstall() {
            MemoryPressureListener.setOnMemoryPressureCallbackForTesting(null);
        }

        private void onMemoryPressure(@MemoryPressureLevel int pressure) {
            Assert.assertFalse("Should not be called twice", mCalled);
            mCalled = true;
            mPressure = pressure;
        }

        public void assertCalledWith(@MemoryPressureLevel int pressure) {
            Assert.assertTrue(mCalled);
            Assert.assertEquals(pressure, mPressure);
        }

        public void assertNotCalled() {
            Assert.assertFalse(mCalled);
        }
    }

    @Before
    public void setUp() throws Exception {
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        MemoryPressureListener.registerSystemCallback();
    }

    @Test
    @SmallTest
    public void testLowMemoryTranslation() {
        MemoryPressureCallback callback = new MemoryPressureCallback();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            callback.install();
            ((Application) ContextUtils.getApplicationContext()).onLowMemory();
            callback.uninstall();
        });
        callback.assertCalledWith(MemoryPressureLevel.CRITICAL);
    }

    @Test
    @SmallTest
    public void testTrimLevelTranslation() {
        Integer[][] trimLevelToPressureMap = {
                // Levels >= TRIM_MEMORY_COMPLETE map to CRITICAL.
                {ComponentCallbacks2.TRIM_MEMORY_COMPLETE + 1, MemoryPressureLevel.CRITICAL},
                {ComponentCallbacks2.TRIM_MEMORY_COMPLETE, MemoryPressureLevel.CRITICAL},

                // TRIM_MEMORY_RUNNING_CRITICAL maps to CRITICAL.
                {ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL, MemoryPressureLevel.CRITICAL},

                // Levels < TRIM_MEMORY_COMPLETE && >= TRIM_MEMORY_BACKGROUND map to MODERATE.
                {ComponentCallbacks2.TRIM_MEMORY_COMPLETE - 1, MemoryPressureLevel.MODERATE},
                {ComponentCallbacks2.TRIM_MEMORY_BACKGROUND + 1, MemoryPressureLevel.MODERATE},
                {ComponentCallbacks2.TRIM_MEMORY_BACKGROUND, MemoryPressureLevel.MODERATE},

                // Other levels don't trigger memory pressure at all.
                {ComponentCallbacks2.TRIM_MEMORY_BACKGROUND - 1, null},
                {ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW, null},
                {ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE, null},
                {ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN, null}};
        for (Integer[] trimLevelAndPressure : trimLevelToPressureMap) {
            final int trimLevel = trimLevelAndPressure[0];
            Integer pressure = trimLevelAndPressure[1];
            MemoryPressureCallback callback = new MemoryPressureCallback();
            ThreadUtils.runOnUiThreadBlocking(() -> {
                callback.install();
                ((Application) ContextUtils.getApplicationContext()).onTrimMemory(trimLevel);
                callback.uninstall();
            });
            if (pressure == null) {
                callback.assertNotCalled();
            } else {
                callback.assertCalledWith(pressure);
            }
        }
    }
}
