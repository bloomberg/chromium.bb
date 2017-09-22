// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;

/**
 * Unit tests for {@SelectionPopupController}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionPopupControllerTest {
    SelectionPopupController mController;
    Context mContext;
    WindowAndroid mWindowAndroid;
    WebContents mWebContents;
    View mView;
    RenderCoordinates mRenderCoordinates;

    class MySelectionClient implements SelectionClient {
        @Override
        public void onSelectionChanged(String selection) {}

        @Override
        public void onSelectionEvent(int eventType, float posXPix, float poxYPix) {}

        @Override
        public void showUnhandledTapUIIfNeeded(int x, int y) {}

        @Override
        public void selectWordAroundCaretAck(boolean didSelect, int startAdjust, int endAdjust) {}

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            return false;
        }

        @Override
        public void cancelAllRequests() {}
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mContext = Mockito.mock(Context.class);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        mWebContents = Mockito.mock(WebContents.class);
        mView = Mockito.mock(View.class);
        mRenderCoordinates = Mockito.mock(RenderCoordinates.class);

        mController = SelectionPopupController.createForTesting(
                mContext, mWindowAndroid, mWebContents, mView, mRenderCoordinates);
    }

    @Test
    @Feature({"SmartSelection"})
    public void testSmartSelectionAdjustSelectionRange() {
        assertNotNull(mController);
        MySelectionClient client = new MySelectionClient();
    }
}
