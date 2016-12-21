// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.Intent;
import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

/**
 * Instrumentation tests for {@link IntentWithGesturesHandler}.
 */
public class IntentWithGesturesHandlerTest extends InstrumentationTestCase {

    @Override
    public void tearDown() throws Exception {
        IntentWithGesturesHandler.getInstance().clear();
        super.tearDown();
    }

    @SmallTest
    public void testCanUseGestureTokenOnlyOnce() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent);
        assertTrue(intent.hasExtra(IntentWithGesturesHandler.EXTRA_USER_GESTURE_TOKEN));
        assertTrue(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent));
        assertFalse(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent));
    }

    @SmallTest
    public void testModifiedGestureToken() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent);
        intent.setData(Uri.parse("content://xyz"));
        assertFalse(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent));
    }

    @SmallTest
    public void testPreviousGestureToken() {
        Intent intent1 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent1);
        Intent intent2 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://xyz"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent2);
        assertFalse(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent1));
    }
}
