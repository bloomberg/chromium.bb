// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.blimp.BlimpNativeInstrumentationTestCase;
import org.chromium.blimp.core_public.BlimpContents;
import org.chromium.blimp.core_public.BlimpNavigationController;
import org.chromium.blimp.core_public.EmptyBlimpContentsObserver;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests the basic behavior of a {@link BlimpContents}.
 */
public class BlimpContentsTest extends BlimpNativeInstrumentationTestCase {
    private static final String EXAMPLE_URL = "https://www.example.com/";
    private static final String OTHER_EXAMPLE_URL = "https://www.otherexample.com/";

    private static class TestBlimpContentsObserver extends EmptyBlimpContentsObserver {
        public String url;
        @Override
        public void onUrlUpdated(String url) {
            this.url = url;
        }
    }

    private static void runUntilIdle() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {}
        });
    }

    @DisabledTest(message = "https://crbug.com/622236")
    @SmallTest
    public void testBasic() throws InterruptedException {
        waitUntilNativeIsReady();

        final AtomicReference<BlimpContents> blimpContents = new AtomicReference<>();
        final AtomicReference<BlimpNavigationController> navigationController =
                new AtomicReference<>();
        final AtomicReference<TestBlimpContentsObserver> observer1 = new AtomicReference<>();
        final AtomicReference<TestBlimpContentsObserver> observer2 = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                blimpContents.set(BlimpContentsFactory.createBlimpContents());
                navigationController.set(blimpContents.get().getNavigationController());
                observer1.set(new TestBlimpContentsObserver());
                observer2.set(new TestBlimpContentsObserver());
                blimpContents.get().addObserver(observer1.get());
                blimpContents.get().addObserver(observer2.get());

                navigationController.get().loadUrl(EXAMPLE_URL);
            }
        });

        runUntilIdle();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals(EXAMPLE_URL, navigationController.get().getUrl());
                assertEquals(EXAMPLE_URL, observer1.get().url);
                assertEquals(EXAMPLE_URL, observer2.get().url);

                // Observer should no longer receive callbacks.
                blimpContents.get().removeObserver(observer1.get());

                navigationController.get().loadUrl(OTHER_EXAMPLE_URL);
            }
        });

        runUntilIdle();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals(OTHER_EXAMPLE_URL, navigationController.get().getUrl());
                assertEquals(EXAMPLE_URL, observer1.get().url);
                assertEquals(OTHER_EXAMPLE_URL, observer2.get().url);

                blimpContents.get().destroy();
            }
        });

        runUntilIdle();
    }
}
