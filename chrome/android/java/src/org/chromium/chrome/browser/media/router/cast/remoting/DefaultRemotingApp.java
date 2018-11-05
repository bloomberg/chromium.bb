// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast.remoting;

import com.google.android.gms.cast.CastMediaControlIntent;
import com.google.android.gms.common.api.GoogleApiClient;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.net.URI;
import java.net.URISyntaxException;

/**
 * App corresponding to the default media receiver application.
 */
@UsedByReflection("RemotingAppLauncher.java")
public class DefaultRemotingApp implements RemotingAppLauncher.RemotingApp {
    private static final String TAG = "DefaultRmtApp";

    public DefaultRemotingApp() {}

    // RemotingApp implementation
    @Override
    public boolean canPlayMedia(RemotingMediaSource source) {
        try {
            String scheme = new URI(source.getMediaUrl()).getScheme();
            if (scheme == null) return false;

            return scheme.equals(UrlConstants.HTTP_SCHEME)
                    || scheme.equals(UrlConstants.HTTPS_SCHEME);
        } catch (URISyntaxException e) {
            return false;
        }
    }

    @Override
    public String getApplicationId() {
        // Can be overriden downstream.
        return CastMediaControlIntent.DEFAULT_MEDIA_RECEIVER_APPLICATION_ID;
    }

    @Override
    public boolean hasCustomLoad() {
        // Can be overriden downstream.
        return false;
    }

    @Override
    public void load(GoogleApiClient client, long startTime, MediaSource source) {
        // Can be overriden downstream.
    }
}
