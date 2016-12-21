// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_shell.Shell;

import java.util.concurrent.ExecutionException;

/**
 * Test suite to verify the behavior of the shell management logic.
 */
public class ContentShellShellManagementTest extends ContentShellTestBase {

    private static final String TEST_PAGE_1 = UrlUtils.encodeHtmlDataUri(
            "<html><body style='background: red;'></body></html>");
    private static final String TEST_PAGE_2 = UrlUtils.encodeHtmlDataUri(
            "<html><body style='background: green;'></body></html>");

    @SmallTest
    @Feature({"Main"})
    @RetryOnFailure
    public void testMultipleShellsLaunched() throws InterruptedException, ExecutionException {
        final ContentShellActivity activity = launchContentShellWithUrl(TEST_PAGE_1);
        assertEquals(TEST_PAGE_1, activity.getActiveShell().getContentViewCore()
                .getWebContents().getUrl());

        Shell previousActiveShell = activity.getActiveShell();
        assertFalse(previousActiveShell.isDestroyed());

        loadNewShell(TEST_PAGE_2);
        assertEquals(TEST_PAGE_2, activity.getActiveShell().getContentViewCore()
                .getWebContents().getUrl());

        assertNotSame(previousActiveShell, activity.getActiveShell());
        assertTrue(previousActiveShell.isDestroyed());
        assertFalse(previousActiveShell.getContentViewCore().isAlive());
    }

}
