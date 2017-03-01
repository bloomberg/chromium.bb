// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.support.test.filters.SmallTest;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.blink.mojom.MediaSessionAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.HashSet;
import java.util.Set;

/**
 * Test of media notifications to see whether the control buttons update when MediaSessionActions
 * change or the tab navigates.
 */
public class NotificationActionsUpdatedTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final int NOTIFICATION_ID = R.id.media_playback_notification;
    private static final String TEST_PAGE_URL = "/content/test/data/media/session/title1.html";

    private Tab mTab;
    private EmbeddedTestServer mTestServer;

    public NotificationActionsUpdatedTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTab = getActivity().getActivityTab();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @SmallTest
    public void testActionsDefaultToNull() throws Throwable {
        simulateMediaSessionStateChanged(mTab, true, false);
        assertActionsMatch(new HashSet<Integer>());
    }

    @SmallTest
    public void testActionPropagateProperly() throws Throwable {
        simulateMediaSessionActionsChanged(mTab, buildActions());
        simulateMediaSessionStateChanged(mTab, true, false);
        assertActionsMatch(buildActions());
    }

    @SmallTest
    public void testActionsResetAfterNavigation() throws Throwable {
        loadUrl("about:blank");
        simulateMediaSessionActionsChanged(mTab, buildActions());
        simulateMediaSessionStateChanged(mTab, true, false);
        assertActionsMatch(buildActions());

        loadUrl("data:text/html;charset=utf-8,"
                + "<html><head><title>title1</title></head><body/></html>");
        assertActionsMatch(new HashSet<Integer>());
    }

    @SmallTest
    public void testActionsPersistAfterSamePageNavigation() throws Throwable {
        ensureTestServer();
        loadUrl(mTestServer.getURL(TEST_PAGE_URL));
        simulateMediaSessionActionsChanged(mTab, buildActions());
        simulateMediaSessionStateChanged(mTab, true, false);
        assertActionsMatch(buildActions());

        NotificationTestUtils.simulateSamePageNavigation(
                getInstrumentation(), mTab, mTestServer.getURL(TEST_PAGE_URL));
        assertActionsMatch(buildActions());
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

    private void simulateMediaSessionActionsChanged(final Tab tab, final Set<Integer> actions) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    ObserverList.RewindableIterator<MediaSessionObserver> observers =
                            MediaSession.fromWebContents(tab.getWebContents())
                            .getObserversForTesting();
                    while (observers.hasNext()) {
                        observers.next().mediaSessionActionsChanged(actions);
                    }
                }
            });
    }

    private void assertActionsMatch(final Set<Integer> expectedActions) {
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
                    Set<Integer> observedActions =
                            MediaNotificationManager
                            .getMediaNotificationInfoForTesting(NOTIFICATION_ID)
                            .mediaSessionActions;

                    assertEquals(expectedActions, observedActions);
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

    private Set<Integer> buildActions() {
        HashSet<Integer> actions = new HashSet<>();

        actions.add(MediaSessionAction.PLAY);
        actions.add(MediaSessionAction.PAUSE);
        return actions;
    }
}
