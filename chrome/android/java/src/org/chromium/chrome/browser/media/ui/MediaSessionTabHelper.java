// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Activity;
import android.media.AudioManager;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.MediaSessionUMA;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

import java.net.URI;
import java.net.URISyntaxException;

/**
 * A tab helper responsible for enabling/disabling media controls and passing
 * media actions from the controls to the {@link org.chromium.content.browser.MediaSession}
 */
public class MediaSessionTabHelper {
    private static final String TAG = "MediaSession";

    private static final String UNICODE_PLAY_CHARACTER = "\u25B6";

    private Tab mTab;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private int mPreviousVolumeControlStream = AudioManager.USE_DEFAULT_STREAM_TYPE;
    private MediaNotificationInfo.Builder mNotificationInfoBuilder = null;

    private MediaNotificationListener mControlsListener = new MediaNotificationListener() {
        @Override
        public void onPlay(int actionSource) {
            MediaSessionUMA
                    .recordPlay(MediaSessionTabHelper.convertMediaActionSourceToUMA(actionSource));

            mWebContents.resumeMediaSession();
        }

        @Override
        public void onPause(int actionSource) {
            MediaSessionUMA.recordPause(
                    MediaSessionTabHelper.convertMediaActionSourceToUMA(actionSource));

            mWebContents.suspendMediaSession();
        }

        @Override
        public void onStop(int actionSource) {
            MediaSessionUMA
                    .recordStop(MediaSessionTabHelper.convertMediaActionSourceToUMA(actionSource));

            mWebContents.stopMediaSession();
        }
    };

    void hideNotification() {
        if (mTab == null) {
            return;
        }
        MediaNotificationManager.hide(mTab.getId(), R.id.media_playback_notification);
        Activity activity = getActivityFromTab(mTab);
        if (activity != null) {
            activity.setVolumeControlStream(mPreviousVolumeControlStream);
        }
        mNotificationInfoBuilder = null;
    }

    private WebContentsObserver createWebContentsObserver(WebContents webContents) {
        return new WebContentsObserver(webContents) {
            @Override
            public void destroy() {
                hideNotification();
                super.destroy();
            }

            @Override
            public void mediaSessionStateChanged(boolean isControllable, boolean isPaused) {
                if (!isControllable) {
                    hideNotification();
                    return;
                }
                String origin = mTab.getUrl();
                try {
                    origin = UrlUtilities.formatUrlForSecurityDisplay(new URI(origin), true);
                } catch (URISyntaxException e) {
                    Log.e(TAG, "Unable to parse the origin from the URL. "
                            + "Showing the full URL instead.");
                }

                mNotificationInfoBuilder = new MediaNotificationInfo.Builder()
                        .setTitle(sanitizeMediaTitle(mTab.getTitle()))
                        .setPaused(isPaused)
                        .setOrigin(origin)
                        .setTabId(mTab.getId())
                        .setPrivate(mTab.isIncognito())
                        .setIcon(R.drawable.audio_playing)
                        .setActions(MediaNotificationInfo.ACTION_PLAY_PAUSE
                                | MediaNotificationInfo.ACTION_SWIPEAWAY)
                        .setContentIntent(Tab.createBringTabToFrontIntent(mTab.getId()))
                        .setId(R.id.media_playback_notification)
                        .setListener(mControlsListener);

                MediaNotificationManager.show(ApplicationStatus.getApplicationContext(),
                        mNotificationInfoBuilder.build());

                Activity activity = getActivityFromTab(mTab);
                if (activity != null) {
                    activity.setVolumeControlStream(AudioManager.STREAM_MUSIC);
                }
            }
        };
    }

    private void setWebContents(WebContents webContents) {
        if (mWebContents == webContents) return;

        cleanupWebContents();
        mWebContents = webContents;
        if (mWebContents != null) mWebContentsObserver = createWebContentsObserver(mWebContents);
    }

    private void cleanupWebContents() {
        if (mWebContentsObserver != null) mWebContentsObserver.destroy();
        mWebContentsObserver = null;
        mWebContents = null;
    }

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onContentChanged(Tab tab) {
            assert tab == mTab;
            setWebContents(tab.getWebContents());
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            assert tab == mTab;
            if (mNotificationInfoBuilder == null) return;

            mNotificationInfoBuilder.setTitle(sanitizeMediaTitle(mTab.getTitle()));
            MediaNotificationManager.show(ApplicationStatus.getApplicationContext(),
                    mNotificationInfoBuilder.build());
        }

        @Override
        public void onDestroyed(Tab tab) {
            assert mTab == tab;

            cleanupWebContents();

            hideNotification();
            mTab.removeObserver(this);
            mTab = null;
        }
    };

    private MediaSessionTabHelper(Tab tab) {
        mTab = tab;
        mTab.addObserver(mTabObserver);
        if (mTab.getWebContents() != null) setWebContents(tab.getWebContents());

        Activity activity = getActivityFromTab(mTab);
        if (activity != null) {
            mPreviousVolumeControlStream = activity.getVolumeControlStream();
        }
    }

    /**
     * Creates the {@link MediaSessionTabHelper} for the given {@link Tab}.
     * @param tab the tab to attach the helper to.
     */
    public static void createForTab(Tab tab) {
        new MediaSessionTabHelper(tab);
    }

    /**
     * Removes all the leading/trailing white spaces and the quite common unicode play character.
     * It improves the visibility of the title in the notification.
     *
     * @param title The original tab title, e.g. "   ▶   Foo - Bar  "
     * @return The sanitized tab title, e.g. "Foo - Bar"
     */
    private String sanitizeMediaTitle(String title) {
        title = title.trim();
        return title.startsWith(UNICODE_PLAY_CHARACTER) ? title.substring(1).trim() : title;
    }

    /**
     * Converts the {@link MediaNotificationListener} action source enum into the
     * {@link MediaSessionUMA} one to ensure matching the histogram values.
     * @param source the source id, must be one of the ACTION_SOURCE_* constants defined in the
     *               {@link MediaNotificationListener} interface.
     * @return the corresponding histogram value.
     */
    public static int convertMediaActionSourceToUMA(int source) {
        if (source == MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION) {
            return MediaSessionUMA.MEDIA_SESSION_ACTION_SOURCE_MEDIA_NOTIFICATION;
        } else if (source == MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION) {
            return MediaSessionUMA.MEDIA_SESSION_ACTION_SOURCE_MEDIA_SESSION;
        } else if (source == MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG) {
            return MediaSessionUMA.MEDIA_SESSION_ACTION_SOURCE_HEADSET_UNPLUG;
        }

        assert false;
        return MediaSessionUMA.MEDIA_SESSION_ACTION_SOURCE_MAX;
    }

    private Activity getActivityFromTab(Tab tab) {
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return null;

        return windowAndroid.getActivity().get();
    }
}
