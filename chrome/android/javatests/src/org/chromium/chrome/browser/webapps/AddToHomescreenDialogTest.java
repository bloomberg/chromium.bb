// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.DialogInterface;
import android.support.test.filters.SmallTest;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Tests org.chromium.chrome.browser.webapps.AddToHomescreenDialog by verifying
 * that the calling the show() method actually shows the dialog and checks that
 * some expected elements inside the dialog are present.
 *
 * This is mostly intended as a smoke test.
 */
public class AddToHomescreenDialogTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static class MockAddToHomescreenManager extends AddToHomescreenManager {
        public MockAddToHomescreenManager(Activity activity, Tab tab) {
            super(activity, tab);
        }

        @Override
        public void addShortcut(String userRequestedTitle) {}
        @Override
        public void onFinished() {}
    }

    public AddToHomescreenDialogTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    @Feature("{Webapp}")
    @RetryOnFailure
    public void testSmoke() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AddToHomescreenDialog dialog =
                        new AddToHomescreenDialog(new MockAddToHomescreenManager(
                                getActivity(), getActivity().getActivityTab()));
                dialog.show(getActivity());

                AlertDialog alertDialog = dialog.getAlertDialogForTesting();
                assertNotNull(alertDialog);

                assertTrue(alertDialog.isShowing());

                assertNotNull(alertDialog.findViewById(R.id.spinny));
                assertNotNull(alertDialog.findViewById(R.id.icon));
                assertNotNull(alertDialog.findViewById(R.id.text));
                assertNotNull(alertDialog.getButton(DialogInterface.BUTTON_POSITIVE));
                assertNotNull(alertDialog.getButton(DialogInterface.BUTTON_NEGATIVE));
            }
        });
    }
}
