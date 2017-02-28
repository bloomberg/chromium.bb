// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.support.test.filters.MediumTest;
import android.view.KeyEvent;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the feature that auto locks the orientation when a video goes fullscreen.
 * See also content layer org.chromium.content.browser.VideoFullscreenOrientationLockTest
 */
@CommandLineFlags.Add({"enable-features=VideoFullscreenOrientationLock",
        ContentSwitches.DISABLE_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK})
public class VideoFullscreenOrientationLockChromeTest extends ChromeTabbedActivityTestBase {
    private static final String TEST_URL = "content/test/data/media/video-player.html";
    private static final String VIDEO_URL = "content/test/data/media/bear.webm";
    private static final String VIDEO_ID = "video";

    private WebContents getWebContents() {
        return getActivity().getCurrentContentViewCore().getWebContents();
    }

    private void waitForContentsFullscreenState(boolean fullscreenValue)
            throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(fullscreenValue, new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws InterruptedException, TimeoutException {
                        return DOMUtils.isFullscreen(getWebContents());
                    }
                }));
    }

    private boolean isScreenOrientationLocked() {
        return getActivity().getRequestedOrientation()
                != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
    }

    private boolean isScreenOrientationLandscape() throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  return  screen.orientation.type.startsWith('landscape');");
        sb.append("})();");

        return JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), sb.toString())
                .equals("true");
    }

    private void waitUntilLockedToLandscape() throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return isScreenOrientationLocked() && isScreenOrientationLandscape();
                } catch (InterruptedException e) {
                    return false;
                } catch (TimeoutException e) {
                    return false;
                }
            }
        });
    }

    private void waitUntilUnlocked() throws InterruptedException {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !isScreenOrientationLocked();
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(UrlUtils.getIsolatedTestFileUrl(TEST_URL));
    }

    @MediumTest
    @Feature({"VideoFullscreenOrientationLock"})
    public void testUnlockWithDownloadViewerActivity() throws Exception {
        if (DeviceFormFactor.isTablet(getInstrumentation().getContext())) return;

        // Start playback to guarantee it's properly loaded.
        assertTrue(DOMUtils.isMediaPaused(getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(getWebContents(), VIDEO_ID);

        // Trigger requestFullscreen() via a click on a button.
        assertTrue(DOMUtils.clickNode(getActivity().getCurrentContentViewCore(), "fullscreen"));
        waitForContentsFullscreenState(true);

        // Should be locked to landscape now, `waitUntilLockedToLandscape` will throw otherwise.
        waitUntilLockedToLandscape();

        // Orientation lock should be disabled when download viewer activity is started.
        Uri fileUri = Uri.parse(UrlUtils.getIsolatedTestFileUrl(VIDEO_URL));
        String mimeType = "video/mp4";
        Intent intent =
                DownloadUtils.getMediaViewerIntentForDownloadItem(fileUri, fileUri, mimeType);
        IntentHandler.startActivityForTrustedIntent(intent);
        waitUntilUnlocked();

        // Sometimes the web page doesn't transition out of fullscreen until we exit the download
        // activity and return to the web page.
        sendKeys(KeyEvent.KEYCODE_BACK);
        waitForContentsFullscreenState(false);
    }
}
