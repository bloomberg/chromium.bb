// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Tests that the AsyncTabCreationParamsManager works as expected.
 */
public class AsyncTabCreationParamsManagerTest extends InstrumentationTestCase {
    @SmallTest
    @UiThreadTest
    public void testBasicAddingAndRemoval() throws Exception {
        AsyncTabCreationParams asyncParams =
                new AsyncTabCreationParams(new LoadUrlParams("http://google.com"));
        AsyncTabCreationParamsManager.add(11684, asyncParams);

        AsyncTabCreationParams retrievedParams = AsyncTabCreationParamsManager.remove(11684);
        assertEquals("Removed incorrect parameters from the map", asyncParams, retrievedParams);

        AsyncTabCreationParams failedParams = AsyncTabCreationParamsManager.remove(11684);
        assertNull("Removed same parameters twice", failedParams);
    }
}