// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.support.test.filters.LargeTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.Button;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.ui.PhotoPickerListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for the PhotoPickerDialog class.
 */
public class PhotoPickerDialogTest extends ChromeActivityTestCaseBase<ChromeActivity>
        implements PhotoPickerListener, SelectionObserver<PickerBitmap> {
    // The dialog we are testing.
    private PhotoPickerDialog mDialog;

    // The data to show in the dialog (A map of filepath to last-modified time).
    // Map<String, Long> mTestFiles;
    private List<PickerBitmap> mTestFiles;

    // The selection delegate for the dialog.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // The last action recorded in the dialog (e.g. photo selected).
    private Action mLastActionRecorded;

    // The final set of photos picked by the dialog. Can be an empty array, if
    // nothing was selected.
    private String[] mLastSelectedPhotos;

    // The list of currently selected photos (built piecemeal).
    private List<PickerBitmap> mCurrentPhotoSelection;

    // A callback that fires when something is selected in the dialog.
    public final CallbackHelper onSelectionCallback = new CallbackHelper();

    // A callback that fires when an action is taken in the dialog (cancel/done etc).
    public final CallbackHelper onActionCallback = new CallbackHelper();

    public PhotoPickerDialogTest() {
        super(ChromeActivity.class);
    }

    // ChromeActivityTestCaseBase:

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mTestFiles = new ArrayList<>();
        mTestFiles.add(new PickerBitmap("a", 5L, PickerBitmap.PICTURE));
        mTestFiles.add(new PickerBitmap("b", 4L, PickerBitmap.PICTURE));
        mTestFiles.add(new PickerBitmap("c", 3L, PickerBitmap.PICTURE));
        mTestFiles.add(new PickerBitmap("d", 2L, PickerBitmap.PICTURE));
        mTestFiles.add(new PickerBitmap("e", 1L, PickerBitmap.PICTURE));
        mTestFiles.add(new PickerBitmap("f", 0L, PickerBitmap.PICTURE));
        PickerCategoryView.setTestFiles(mTestFiles);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    // PhotoPickerDialog.PhotoPickerListener:

    @Override
    public void onPickerUserAction(Action action, String[] photos) {
        mLastActionRecorded = action;
        mLastSelectedPhotos = photos != null ? photos.clone() : null;
        if (mLastSelectedPhotos != null) Arrays.sort(mLastSelectedPhotos);
        onActionCallback.notifyCalled();
    }

    // SelectionObserver:

    @Override
    public void onSelectionStateChange(List<PickerBitmap> photosSelected) {
        mCurrentPhotoSelection = new ArrayList<>(photosSelected);
        onSelectionCallback.notifyCalled();
    }

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mDialog.findViewById(R.id.recycler_view);
    }

    private PhotoPickerDialog createDialog(final boolean multiselect, final List<String> mimeTypes)
            throws Exception {
        final PhotoPickerDialog dialog =
                ThreadUtils.runOnUiThreadBlocking(new Callable<PhotoPickerDialog>() {
                    @Override
                    public PhotoPickerDialog call() {
                        final PhotoPickerDialog dialog = new PhotoPickerDialog(
                                getActivity(), PhotoPickerDialogTest.this, multiselect, mimeTypes);
                        dialog.show();
                        return dialog;
                    }
                });

        mSelectionDelegate = dialog.getCategoryViewForTesting().getSelectionDelegateForTesting();
        if (!multiselect) mSelectionDelegate.setSingleSelectionMode();
        mSelectionDelegate.addObserver(this);
        mDialog = dialog;

        return dialog;
    }

    private void clickView(final int position, final int expectedSelectionCount) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, position);

        int callCount = onSelectionCallback.getCallCount();
        TouchCommon.singleClickView(
                recyclerView.findViewHolderForAdapterPosition(position).itemView);
        onSelectionCallback.waitForCallback(callCount, 1);

        // Validate the correct selection took place.
        assertEquals(expectedSelectionCount, mCurrentPhotoSelection.size());
        assertTrue(mSelectionDelegate.isItemSelected(mTestFiles.get(position)));
    }

    private void clickDone() throws Exception {
        mLastActionRecorded = null;

        PhotoPickerToolbar toolbar = (PhotoPickerToolbar) mDialog.findViewById(R.id.action_bar);
        Button done = (Button) toolbar.findViewById(R.id.done);
        int callCount = onActionCallback.getCallCount();
        TouchCommon.singleClickView(done);
        onActionCallback.waitForCallback(callCount, 1);
        assertEquals(PhotoPickerListener.Action.PHOTOS_SELECTED, mLastActionRecorded);
    }

    public void clickCancel() throws Exception {
        mLastActionRecorded = null;

        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        View cancel = new View(getActivity());
        int callCount = onActionCallback.getCallCount();
        categoryView.onClick(cancel);
        onActionCallback.waitForCallback(callCount, 1);
        assertEquals(PhotoPickerListener.Action.CANCEL, mLastActionRecorded);
    }

    private void dismissDialog() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDialog.dismiss();
            }
        });
    }

    @LargeTest
    public void testNoSelection() throws Throwable {
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        assertTrue(mDialog.isShowing());

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount);
        clickCancel();

        assertEquals(null, mLastSelectedPhotos);
        assertEquals(PhotoPickerListener.Action.CANCEL, mLastActionRecorded);

        dismissDialog();
    }

    @LargeTest
    public void testSingleSelectionPhoto() throws Throwable {
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        assertTrue(mDialog.isShowing());

        // Expected selection count is 1 because clicking on a new view unselects other.
        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount);
        clickView(1, expectedSelectionCount);
        clickDone();

        assertEquals(1, mLastSelectedPhotos.length);
        assertEquals(PhotoPickerListener.Action.PHOTOS_SELECTED, mLastActionRecorded);
        assertEquals(mTestFiles.get(1).getFilePath(), mLastSelectedPhotos[0]);

        dismissDialog();
    }

    @LargeTest
    public void testMultiSelectionPhoto() throws Throwable {
        createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
        assertTrue(mDialog.isShowing());

        // Multi-selection is enabled, so each click is counted.
        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount++);
        clickView(2, expectedSelectionCount++);
        clickView(4, expectedSelectionCount++);
        clickDone();

        assertEquals(3, mLastSelectedPhotos.length);
        assertEquals(PhotoPickerListener.Action.PHOTOS_SELECTED, mLastActionRecorded);
        assertEquals(mTestFiles.get(0).getFilePath(), mLastSelectedPhotos[0]);
        assertEquals(mTestFiles.get(2).getFilePath(), mLastSelectedPhotos[1]);
        assertEquals(mTestFiles.get(4).getFilePath(), mLastSelectedPhotos[2]);

        dismissDialog();
    }
}
