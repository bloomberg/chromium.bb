// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.PackageManager;
import android.view.ActionMode;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.base.WindowAndroid;

/**
 * Unit tests for {@SelectionPopupController}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionPopupControllerTest {
    private SelectionPopupController mController;
    private Context mContext;
    private WindowAndroid mWindowAndroid;
    private WebContents mWebContents;
    private View mView;
    private RenderCoordinates mRenderCoordinates;
    private ActionMode mActionMode;
    private PackageManager mPackageManager;

    private static final String MOUNTAIN_FULL = "585 Franklin Street, Mountain View, CA 94041";
    private static final String MOUNTAIN = "Mountain";
    private static final String AMPHITHEATRE_FULL = "1600 Amphitheatre Parkway";
    private static final String AMPHITHEATRE = "Amphitheatre";

    private class TestSelectionClient implements SelectionClient {
        private SelectionClient.Result mResult;
        private SelectionClient.ResultCallback mResultCallback;

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
            mResultCallback.onClassified(mResult);
            return true;
        }

        @Override
        public void cancelAllRequests() {}

        public void setResult(SelectionClient.Result result) {
            mResult = result;
        }

        public void setResultCallback(SelectionClient.ResultCallback callback) {
            mResultCallback = callback;
        }
    }

    class SelectionClientOnlyReturnTrue extends TestSelectionClient {
        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            return true;
        }
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;

        mContext = Mockito.mock(Context.class);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        mWebContents = Mockito.mock(WebContents.class);
        mView = Mockito.mock(View.class);
        mRenderCoordinates = Mockito.mock(RenderCoordinates.class);
        mActionMode = Mockito.mock(ActionMode.class);
        mPackageManager = Mockito.mock(PackageManager.class);

        when(mContext.getPackageManager()).thenReturn(mPackageManager);

        mController = SelectionPopupController.createForTesting(
                mContext, mWindowAndroid, mWebContents, mView, mRenderCoordinates);
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionAdjustSelectionRange() {
        InOrder order = inOrder(mWebContents, mView);
        SelectionClient.Result result = resultForAmphitheatre();

        // Setup SelectionClient for SelectionPopupController.
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mController.getResultCallback());
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, AMPHITHEATRE, /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_LONG_PRESS);

        // adjustSelectionByCharacterOffset() should be called.
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(result.startAdjust, result.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        when(mView.startActionMode(any(FloatingActionModeCallback.class), anyInt()))
                .thenReturn(mActionMode);

        // Call showSelectionMenu again, which is adjustSelectionByCharacterOffset triggered.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, AMPHITHEATRE_FULL,
                /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_ADJUST_SELECTION);

        order.verify(mView).startActionMode(
                isA(FloatingActionModeCallback.class), eq(ActionMode.TYPE_FLOATING));

        SelectionClient.Result returnResult = mController.getClassificationResult();
        assertEquals(-5, returnResult.startAdjust);
        assertEquals(8, returnResult.endAdjust);
        assertEquals("Maps", returnResult.label);

        assertTrue(mController.isActionModeValid());
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionAnotherLongPressAfterAdjustment() {
        InOrder order = inOrder(mWebContents, mView);
        SelectionClient.Result result = resultForAmphitheatre();
        SelectionClient.Result newResult = resultForMountain();

        // Set SelectionClient for SelectionPopupController.
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mController.getResultCallback());
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, AMPHITHEATRE, /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_LONG_PRESS);

        // adjustSelectionByCharacterOffset() should be called.
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(result.startAdjust, result.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        // Another long press triggered showSelectionMenu() call.
        client.setResult(newResult);
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, MOUNTAIN, /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_LONG_PRESS);
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(newResult.startAdjust, newResult.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        when(mView.startActionMode(any(FloatingActionModeCallback.class), anyInt()))
                .thenReturn(mActionMode);

        // First adjustSelectionByCharacterOffset() triggered.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, AMPHITHEATRE_FULL,
                /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_ADJUST_SELECTION);

        SelectionClient.Result returnResult = mController.getClassificationResult();
        assertEquals(-21, returnResult.startAdjust);
        assertEquals(15, returnResult.endAdjust);
        assertEquals("Maps", returnResult.label);

        // Second adjustSelectionByCharacterOffset() triggered.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, MOUNTAIN_FULL,
                /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_ADJUST_SELECTION);

        order.verify(mView).startActionMode(
                isA(FloatingActionModeCallback.class), eq(ActionMode.TYPE_FLOATING));
        assertTrue(mController.isActionModeValid());
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionAnotherLongPressBeforeAdjustment() {
        InOrder order = inOrder(mWebContents, mView);
        SelectionClient.Result result = resultForAmphitheatre();
        SelectionClient.Result newResult = resultForMountain();

        // This client won't call SmartSelectionCallback.
        TestSelectionClient client = new SelectionClientOnlyReturnTrue();
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, AMPHITHEATRE, /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_LONG_PRESS);

        // Another long press triggered showSelectionMenu() call.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, MOUNTAIN, /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_LONG_PRESS);

        // Then we done with the first classification.
        mController.getResultCallback().onClassified(result);

        // Followed by the second classifaction.
        mController.getResultCallback().onClassified(newResult);

        // adjustSelectionByCharacterOffset() should be called.
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(result.startAdjust, result.endAdjust, true);
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(newResult.startAdjust, newResult.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        when(mView.startActionMode(any(FloatingActionModeCallback.class), anyInt()))
                .thenReturn(mActionMode);

        // First adjustSelectionByCharacterOffset() triggered.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, AMPHITHEATRE_FULL,
                /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_ADJUST_SELECTION);

        SelectionClient.Result returnResult = mController.getClassificationResult();
        assertEquals(-21, returnResult.startAdjust);
        assertEquals(15, returnResult.endAdjust);
        assertEquals("Maps", returnResult.label);

        // Second adjustSelectionByCharacterOffset() triggered.
        mController.showSelectionMenu(0, 0, 0, 0, /* isEditable = */ true,
                /* isPasswordType = */ false, MOUNTAIN_FULL,
                /* canSelectAll = */ true,
                /* canRichlyEdit = */ true, /* shouldSuggest = */ true,
                MenuSourceType.MENU_SOURCE_ADJUST_SELECTION);

        order.verify(mView).startActionMode(
                isA(FloatingActionModeCallback.class), eq(ActionMode.TYPE_FLOATING));
        assertTrue(mController.isActionModeValid());
    }

    // Result generated by long press "Amphitheatre" in "1600 Amphitheatre Parkway".
    private SelectionClient.Result resultForAmphitheatre() {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -5;
        result.endAdjust = 8;
        result.label = "Maps";
        return result;
    }

    // Result generated by long press "Mountain" in "585 Franklin Street, Mountain View, CA 94041".
    private SelectionClient.Result resultForMountain() {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -21;
        result.endAdjust = 15;
        result.label = "Maps";
        return result;
    }
}
