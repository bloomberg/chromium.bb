// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build;
import android.test.FlakyTest;
import android.view.ContextMenu;

import junit.framework.Assert;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

public class ContextMenuTest extends ChromeShellTestBase {
    @Override
    public void setUp() throws Exception {
        super.setUp();
        launchChromeShellWithUrl(TestHttpServerClient.getUrl(
                "chrome/test/data/android/contextmenu/context_menu_test.html"));
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        assertWaitForPageScaleFactorMatch(0.5f);
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser", "Main"})
    public void testCopyLinkURL() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActiveTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testLink",
                R.id.contextmenu_copy_link_address_text);

        assertStringContains("test_link.html", getClipboardText());
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testCopyImageLinkCopiesLinkURL() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActiveTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testImageLink",
                R.id.contextmenu_copy_link_address_text);

        assertStringContains("test_link.html", getClipboardText());
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testCopyLinkTextSimple() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActiveTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testLink",
                R.id.contextmenu_copy_link_text);

        assertEquals("Clipboard text was not what was expected", "Test Link",
                getClipboardText());
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testCopyLinkTextComplex() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActiveTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "copyLinkTextComplex",
                R.id.contextmenu_copy_link_text);

        assertEquals("Clipboard text was not what was expected",
                "This is pretty   extreme \n(newline). ", getClipboardText());
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testCopyImageToClipboard() throws InterruptedException, TimeoutException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN) return;

        Tab tab = getActivity().getActiveTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testImage",
                R.id.contextmenu_copy_image);

        String expectedUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/contextmenu/test_image.png");

        assertEquals("Clipboard text is not correct", expectedUrl, getClipboardText());
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testLongPressOnImage() throws InterruptedException, TimeoutException {
        final Tab tab = getActivity().getActiveTab();

        TestCallbackHelperContainer helper =
                new TestCallbackHelperContainer(tab.getContentViewCore());

        OnPageFinishedHelper callback = helper.getOnPageFinishedHelper();
        int callbackCount = callback.getCallCount();

        ContextMenuUtils.selectContextMenuItem(this, tab, "testImage",
                R.id.contextmenu_open_image);

        callback.waitForCallback(callbackCount);

        String expectedUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/contextmenu/test_image.png");

        final AtomicReference<String> actualUrl = new AtomicReference<String>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                actualUrl.set(tab.getUrl());
            }
        });

        assertEquals("Failed to navigate to the image", expectedUrl, actualUrl.get());
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testLongPressOnImageLink() throws InterruptedException, TimeoutException {
        final Tab tab = getActivity().getActiveTab();

        TestCallbackHelperContainer helper =
                new TestCallbackHelperContainer(tab.getContentViewCore());

        OnPageFinishedHelper callback = helper.getOnPageFinishedHelper();
        int callbackCount = callback.getCallCount();

        ContextMenuUtils.selectContextMenuItem(this, tab, "testImage",
                R.id.contextmenu_open_image);

        callback.waitForCallback(callbackCount);

        final AtomicReference<String> actualTitle = new AtomicReference<String>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                actualTitle.set(tab.getTitle());
            }
        });

        assertTrue("Navigated to the wrong page.", actualTitle.get().startsWith("test_image.png"));
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testDismissContextMenuOnBack() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActiveTab();
        ContextMenu menu = ContextMenuUtils.openContextMenu(this, tab, "testImage");
        assertNotNull("Context menu was not properly created", menu);
        assertFalse("Context menu did not have window focus", getActivity().hasWindowFocus());

        KeyUtils.pressBack(getInstrumentation());

        Assert.assertTrue("Activity did not regain focus.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().hasWindowFocus();
                    }
                }));
    }

    // http://crbug.com/326769
    @FlakyTest
    // @LargeTest
    @Feature({"Browser"})
    public void testDismissContextMenuOnClick() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActiveTab();
        ContextMenu menu = ContextMenuUtils.openContextMenu(this, tab, "testImage");
        assertNotNull("Context menu was not properly created", menu);
        assertFalse("Context menu did not have window focus", getActivity().hasWindowFocus());

        TestTouchUtils.singleClickView(getInstrumentation(), tab.getView(), 0, 0);

        Assert.assertTrue("Activity did not regain focus.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().hasWindowFocus();
                    }
                }));
    }

    private String getClipboardText() {
        ClipboardManager clipMgr =
                (ClipboardManager) getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clipData = clipMgr.getPrimaryClip();
        assertNotNull("Primary clip is null", clipData);
        assertTrue("Primary clip contains no items.", clipData.getItemCount() > 0);
        return clipData.getItemAt(0).getText().toString();
    }

    private void assertStringContains(String subString, String superString) {
        assertTrue("String '" + superString + "' does not contain '" + subString + "'",
                superString.contains(subString));
    }
}
