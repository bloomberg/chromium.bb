// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Notification;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.MediaMetadata;

/**
 * Test of media notifications to see whether the text updates when the tab title changes or the
 * MediaMetadata gets updated.
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

    private void doTestSessionStatePlaying() throws InterruptedException {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title2");
    }

    private void doTestSessionStatePaused() throws InterruptedException {
        simulateMediaSessionStateChanged(mTab, true, true);
        assertTitleMatches("title1");
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title2");
    }

    private void doTestSessionStateUncontrollable() throws InterruptedException {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");
        simulateMediaSessionStateChanged(mTab, false, false);
        simulateUpdateTitle(mTab, "title2");
    }

    private void doTestMediaMetadataSetsTitle() throws InterruptedException {
        simulateMediaSessionStateChanged(mTab, true, false, new MediaMetadata("title2", "", ""));
        assertTitleMatches("title2");
    }

    private void doTestMediaMetadataOverridesTitle() throws InterruptedException {
        simulateMediaSessionStateChanged(mTab, true, false, new MediaMetadata("title2", "", ""));
        assertTitleMatches("title2");

        simulateUpdateTitle(mTab, "title3");
        assertTitleMatches("title2");
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
    private void doTestMultipleTabs() throws Throwable {
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

    @SmallTest
    @CommandLineFlags.Add("enable-features=MediaStyleNotification")
    public void testSessionStatePlaying_MediaStyleNotification() throws InterruptedException {
        doTestSessionStatePlaying();
    }

    @SmallTest
    @CommandLineFlags.Add("enable-features=MediaStyleNotification")
    public void testSessionStatePaused_MediaStyleNotification() throws InterruptedException {
        doTestSessionStatePaused();
    }

    @SmallTest
    @CommandLineFlags.Add("enable-features=MediaStyleNotification")
    public void testSessionStateUncontrollable_MediaStyleNotification()
            throws InterruptedException {
        doTestSessionStateUncontrollable();
    }

    @SmallTest
    @CommandLineFlags.Add("enable-features=MediaStyleNotification")
    public void testMediaMetadataSetsTitle_MediaStyleNotification() throws InterruptedException {
        doTestMediaMetadataSetsTitle();
    }

    @SmallTest
    @CommandLineFlags.Add("enable-features=MediaStyleNotification")
    public void testMediaMetadataOverridesTitle_MediaStyleNotification()
            throws InterruptedException {
        doTestMediaMetadataOverridesTitle();
    }

    @SmallTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @CommandLineFlags.Add("enable-features=MediaStyleNotification")
    public void testMultipleTabs_MediaStyleNotification() throws Throwable {
        doTestMultipleTabs();
    }

    @SmallTest
    @CommandLineFlags.Add("disable-features=MediaStyleNotification")
    public void testSessionStatePlaying_CustomNotification() throws InterruptedException {
        doTestSessionStatePlaying();
    }

    @SmallTest
    @CommandLineFlags.Add("disable-features=MediaStyleNotification")
    public void testSessionStatePaused_CustomNotification() throws InterruptedException {
        doTestSessionStatePaused();
    }

    @SmallTest
    @CommandLineFlags.Add("disable-features=MediaStyleNotification")
    public void testSessionStateUncontrollable_CustomNotification() throws InterruptedException {
        doTestSessionStateUncontrollable();
    }

    @SmallTest
    @CommandLineFlags.Add("disable-features=MediaStyleNotification")
    public void testMediaMetadataSetsTitle_CustomNotification() throws InterruptedException {
        doTestMediaMetadataSetsTitle();
    }

    @SmallTest
    @CommandLineFlags.Add("disable-features=MediaStyleNotification")
    public void testMediaMetadataOverridesTitle_CustomNotification() throws InterruptedException {
        doTestMediaMetadataOverridesTitle();
    }

    @SmallTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @CommandLineFlags.Add("disable-features=MediaStyleNotification")
    public void testMultipleTabs_CustomNotification() throws Throwable {
        doTestMultipleTabs();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void simulateMediaSessionStateChanged(
            final Tab tab, final boolean isControllable, final boolean isSuspended) {
        simulateMediaSessionStateChanged(
                tab, isControllable, isSuspended, new MediaMetadata("", "", ""));
    }

    private void simulateMediaSessionStateChanged(final Tab tab, final boolean isControllable,
            final boolean isSuspended, final MediaMetadata metadata) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    ObserverList.RewindableIterator<WebContentsObserver> observers =
                            tab.getWebContents().getObserversForTesting();
                    while (observers.hasNext()) {
                        observers.next().mediaSessionStateChanged(
                                isControllable, isSuspended, metadata);
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

    void assertTitleMatches(final String title) throws InterruptedException {
        // The service might still not be created which delays the creation of the notification
        // builder.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return MediaNotificationManager.getNotificationBuilderForTesting(NOTIFICATION_ID)
                        != null;
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    Notification notification =
                            MediaNotificationManager
                                    .getNotificationBuilderForTesting(NOTIFICATION_ID)
                                    .build();

                    View contentView = notification.contentView.apply(
                            getActivity().getApplicationContext(), null);
                    String observedText = null;
                    if (ChromeFeatureList.isEnabled(ChromeFeatureList.MEDIA_STYLE_NOTIFICATION)) {
                        TextView view = (TextView) contentView.findViewById(android.R.id.title);
                        if (view == null) {
                            // Case where NotificationCompat does not use the native Notification.
                            // The TextView id will be in Chrome's namespace.
                            view = (TextView) contentView.findViewById(R.id.title);
                        }
                        observedText = view.getText().toString();
                    } else {
                        observedText = ((TextView) contentView.findViewById(R.id.title))
                                               .getText()
                                               .toString();
                    }
                    assertEquals(title, observedText);
                }
            });
    }
}
