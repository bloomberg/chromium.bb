// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Notification;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.common.MediaMetadata;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Test of media notifications to see whether the text updates when the tab title changes or the
 * MediaMetadata gets updated.
 */
@RetryOnFailure
public class NotificationTitleUpdatedTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final int NOTIFICATION_ID = R.id.media_playback_notification;
    private static final String TEST_PAGE_URL_1 = "/content/test/data/media/session/title1.html";
    private static final String TEST_PAGE_URL_2 = "/content/test/data/media/session/title2.html";

    private Tab mTab;
    private EmbeddedTestServer mTestServer;

    public NotificationTitleUpdatedTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTab = getActivity().getActivityTab();
        simulateUpdateTitle(mTab, "title1");
    }

    @Override
    protected void tearDown() throws Exception {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @SmallTest
    public void testSessionStatePlaying() {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title2");
    }

    @SmallTest
    public void testSessionStatePausedAfterPlaying() {
        simulateMediaSessionStateChanged(mTab, true, false);
        simulateMediaSessionStateChanged(mTab, true, true);
        assertTitleMatches("title1");
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title2");
    }

    @SmallTest
    public void testSessionStateNewlyPaused() {
        simulateMediaSessionStateChanged(mTab, true, true);
        getInstrumentation().waitForIdleSync();
        assertNull(MediaNotificationManager.getNotificationBuilderForTesting(NOTIFICATION_ID));

        simulateUpdateTitle(mTab, "title2");
        getInstrumentation().waitForIdleSync();
        assertNull(MediaNotificationManager.getNotificationBuilderForTesting(NOTIFICATION_ID));
    }

    @SmallTest
    public void testSessionStateUncontrollable() {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");
        simulateMediaSessionStateChanged(mTab, false, false);
        simulateUpdateTitle(mTab, "title2");
    }

    @SmallTest
    public void testMediaMetadataSetsTitle() {
        simulateMediaSessionStateChanged(mTab, true, false);
        simulateMediaSessionMetadataChanged(mTab, new MediaMetadata("title2", "", ""));
        assertTitleMatches("title2");
    }

    @SmallTest
    public void testMediaMetadataOverridesTitle() {
        simulateMediaSessionStateChanged(mTab, true, false);
        simulateMediaSessionMetadataChanged(mTab, new MediaMetadata("title2", "", ""));
        assertTitleMatches("title2");

        simulateUpdateTitle(mTab, "title3");
        assertTitleMatches("title2");
    }

    /**
     * Test if a notification accepts the title update from another tab, using the following steps:
     *   1. set the title of mTab, start the media session, a notification should show up;
     *   2. stop the media session of mTab, the notification shall hide;
     *   3. create newTab, start the media session of newTab, a notification should show up,
     *      set the title of newTab, the notification title should match newTab;
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

    /**
     * Test for the notification should not update for a paused tab if it mismatches the current tab
     * id:
     *   1. set the title of mTab, start the media session, a notification should show up;
     *   2. create newTab, start the media session of newTab, a notification should show up,
     *      set the title of newTab, the notification will match the title;
     *   3. stop the media session of mTab, and change its title, the notification should not
     *      change.
     */
    @SmallTest
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testPreferLastActiveTab() throws Throwable {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertTitleMatches("title1");

        Tab newTab = loadUrlInNewTab("about:blank");
        assertNotNull(newTab);

        simulateMediaSessionStateChanged(newTab, true, false);
        simulateUpdateTitle(newTab, "title3");

        simulateMediaSessionStateChanged(mTab, false, false);
        simulateUpdateTitle(mTab, "title2");
        assertTitleMatches("title3");
    }

    @SmallTest
    public void testMediaMetadataResetsAfterNavigation() throws Throwable {
        loadUrl("about:blank");
        simulateMediaSessionStateChanged(mTab, true, false);
        simulateMediaSessionMetadataChanged(mTab, new MediaMetadata("title2", "", ""));
        assertTitleMatches("title2");

        loadUrl("data:text/html;charset=utf-8,"
                + "<html><head><title>title1</title></head><body/></html>");
        assertTitleMatches("title1");
    }

    @SmallTest
    public void testMediaMetadataResetsAfterSameOriginNavigation() throws Throwable {
        ensureTestServer();
        loadUrl(mTestServer.getURL(TEST_PAGE_URL_1));
        simulateMediaSessionStateChanged(mTab, true, false);
        simulateMediaSessionMetadataChanged(mTab, new MediaMetadata("title2", "", ""));
        assertTitleMatches("title2");

        loadUrl(mTestServer.getURL(TEST_PAGE_URL_2));
        simulateUpdateTitle(mTab, "title3");
        assertTitleMatches("title3");
    }

    @SmallTest
    public void testMediaMetadataPersistsAfterSamePageNavigation() throws Throwable {
        ensureTestServer();
        loadUrl(mTestServer.getURL(TEST_PAGE_URL_1));
        simulateMediaSessionStateChanged(mTab, true, false);
        simulateMediaSessionMetadataChanged(mTab, new MediaMetadata("title2", "", ""));
        assertTitleMatches("title2");

        NotificationTestUtils.simulateSamePageNavigation(
                getInstrumentation(), mTab, mTestServer.getURL(TEST_PAGE_URL_1));
        assertTitleMatches("title2");
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
                    ObserverList.RewindableIterator<MediaSessionObserver> observers =
                            MediaSession.fromWebContents(tab.getWebContents())
                                    .getObserversForTesting();
                    while (observers.hasNext()) {
                        observers.next().mediaSessionStateChanged(isControllable, isSuspended);
                    }
                }
            });
    }

    private void simulateMediaSessionMetadataChanged(final Tab tab, final MediaMetadata metadata) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ObserverList.RewindableIterator<MediaSessionObserver> observers =
                        MediaSession.fromWebContents(tab.getWebContents()).getObserversForTesting();
                while (observers.hasNext()) {
                    observers.next().mediaSessionMetadataChanged(metadata);
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

    private void assertTitleMatches(final String title) {
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
                    TextView view = (TextView) contentView.findViewById(android.R.id.title);
                    if (view == null) {
                        // Case where NotificationCompat does not use the native Notification.
                        // The TextView id will be in Chrome's namespace.
                        view = (TextView) contentView.findViewById(R.id.title);
                    }
                    observedText = view.getText().toString();

                    assertEquals(title, observedText);
                }
            });
    }

    private void ensureTestServer() {
        try {
            mTestServer = EmbeddedTestServer.createAndStartServer(
                getInstrumentation().getContext());
        } catch (Exception e) {
            fail("Failed to start test server");
        }
    }
}
