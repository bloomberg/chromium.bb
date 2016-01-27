// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Test of media notifications to see whether the text updates when the tab title changes
 */
public class NotificationTitleUpdatedTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final int NOTIFICATION_ID = R.id.media_playback_notification;

    private Tab mTab;

    public NotificationTitleUpdatedTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTab = getActivity().getActivityTab();
        simulateUpdateTitle(mTab, "title1");
    }

    @SmallTest
    public void testSessionStatePlaying() throws InterruptedException {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title2");
    }

    @SmallTest
    public void testSessionStatePaused() {
        simulateMediaSessionStateChanged(mTab, true, true);
        assertTitleMatches("title1");
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title2");
    }

    @SmallTest
    public void testSessionStateUncontrollable() {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");
        simulateMediaSessionStateChanged(mTab, false, false);
        simulateUpdateTitle(mTab, "title2");
    }

    /**
     * Test if a notification accepts the title update from another tab, using the following steps:
     *   1. set the title of mTab, start the media session, a notification should show up;
     *   2. stop the media session of mTab, the notification shall hide;
     *   3. create newTab, set the title of mTab, start the media session of mTab,
     *      a notification should show up;
     *   4. change the title of newTab and then mTab to different names,
     *      the notification should have the title of newTab.
     */
    @SmallTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testMultipleTabs() throws Throwable {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");
        simulateMediaSessionStateChanged(mTab, false, false);

        Tab newTab = loadUrlInNewTab("about:blank");
        assertNotNull(newTab);

        simulateMediaSessionStateChanged(newTab, true, false);
        simulateUpdateTitle(newTab, "title3");
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title3");
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void simulateMediaSessionStateChanged(
            final Tab tab, final boolean isControllable, final boolean isSuspended) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    ObserverList.RewindableIterator<WebContentsObserver> observers =
                            tab.getWebContents().getObserversForTesting();
                    while (observers.hasNext()) {
                        observers.next().mediaSessionStateChanged(
                                isControllable, isSuspended);
                    }
                }
            });
    }

    private void simulateUpdateTitle(Tab tab, String title) {
        try {
            TabTitleObserver observer = new TabTitleObserver(tab, title);
            JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    tab.getWebContents(),
                    "document.title = '" + title + "';");
            observer.waitForTitleUpdate(5);
        } catch (Exception e) {
            throw new RuntimeException(e + "failed to update title");
        }
    }

    void assertTitleMatches(final String title) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    assertEquals(title, MediaNotificationManager
                            .getNotificationInfoForTesting(NOTIFICATION_ID).title);
                }
            });
    }
}
