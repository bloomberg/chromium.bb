// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Tests org.chromium.chrome.browser.webapps.AddToHomescreenDialog by verifying
 * that the calling the show() method actually shows the dialog and checks that
 * some expected elements inside the dialog are present.
 *
 * This is mostly intended as a smoke test because the dialog isn't used in
 * Chromium for the moment.
 */
public class AddToHomescreenDialogTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    public AddToHomescreenDialogTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    @Feature("{Webapp}")
    public void testSmoke() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertNull(AddToHomescreenDialog.getCurrentDialogForTest());

                AddToHomescreenDialog.show(getActivity(), getActivity().getActivityTab());
                AlertDialog dialog = AddToHomescreenDialog.getCurrentDialogForTest();
                assertNotNull(dialog);

                assertTrue(dialog.isShowing());

                assertNotNull(dialog.findViewById(R.id.spinny));
                assertNotNull(dialog.findViewById(R.id.icon));
                assertNotNull(dialog.findViewById(R.id.text));
                assertNotNull(dialog.getButton(DialogInterface.BUTTON_POSITIVE));
                assertNotNull(dialog.getButton(DialogInterface.BUTTON_NEGATIVE));
            }
        });
    }
}
