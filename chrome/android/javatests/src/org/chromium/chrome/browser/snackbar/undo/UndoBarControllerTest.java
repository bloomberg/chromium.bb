// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar.undo;

import android.support.test.filters.SmallTest;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the UndoBarController.
 */
public class UndoBarControllerTest extends ChromeTabbedActivityTestBase {
    private SnackbarManager mSnackbarManager;
    private TabModel mTabModel;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mSnackbarManager = getActivity().getSnackbarManager();
        mTabModel = getActivity().getCurrentTabModel();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    public void testCloseAll_SingleTab_Undo() throws Exception {
        assertNull(getCurrentSnackbar());
        assertEquals(1, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(getInstrumentation(), getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertEquals("Closed about:blank", getSnackbarText());
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, mTabModel.getCount());

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(1, mTabModel.getCount());
    }

    @SmallTest
    public void testCloseAll_SingleTab_Dismiss() throws Exception {
        assertNull(getCurrentSnackbar());
        assertEquals(1, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(getInstrumentation(), getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertEquals("Closed about:blank", getSnackbarText());
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, mTabModel.getCount());

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(0, mTabModel.getCount());
    }

    @SmallTest
    public void testCloseAll_MultipleTabs_Undo() throws Exception {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(getInstrumentation(), getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertEquals("2 tabs closed", getSnackbarText());
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, mTabModel.getCount());

        clickSnackbar();

        assertNull(getCurrentSnackbar());
        assertEquals(2, mTabModel.getCount());
    }

    @SmallTest
    public void testCloseAll_MultipleTabs_Dismiss() throws Exception {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());

        assertNull(getCurrentSnackbar());
        assertEquals(2, mTabModel.getCount());

        ChromeTabUtils.closeAllTabs(getInstrumentation(), getActivity());

        Snackbar currentSnackbar = getCurrentSnackbar();
        assertEquals("2 tabs closed", getSnackbarText());
        assertTrue(currentSnackbar.getController() instanceof UndoBarController);
        assertEquals(0, mTabModel.getCount());

        dismissSnackbars();

        assertNull(getCurrentSnackbar());
        assertEquals(0, mTabModel.getCount());
    }

    private void clickSnackbar() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSnackbarManager.onClick(getActivity().findViewById(R.id.snackbar_button));
            }
        });
    }

    private void dismissSnackbars() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSnackbarManager.dismissSnackbars(
                        mSnackbarManager.getCurrentSnackbarForTesting().getController());
            }
        });
    }

    private String getSnackbarText() {
        return ((TextView) getActivity().findViewById(R.id.snackbar_message)).getText().toString();
    }

    private Snackbar getCurrentSnackbar() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Snackbar>() {
            @Override
            public Snackbar call() throws Exception {
                return mSnackbarManager.getCurrentSnackbarForTesting();
            }
        });
    }
}
