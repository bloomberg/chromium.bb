// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.scope.FeedProcessScope;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.host.logging.RequestReason;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.io.StringWriter;

/**
 * Java-side bridge that receives and responds to native-initiated requests for debugging data. This
 * data is then used to populate the Feed Internals page.
 */
@JNINamespace("feed")
public class FeedDebuggingBridge {
    @CalledByNative
    static String getFeedFetchUrl() {
        return FeedConfiguration.getFeedServerEndpoint();
    }

    @CalledByNative
    static String getFeedProcessScopeDump() {
        FeedProcessScope feedProcessScope = FeedProcessScopeFactory.getFeedProcessScope();
        Dumper dumper = Dumper.newDefaultDumper();
        dumper.dump(feedProcessScope);

        try {
            StringWriter writer = new StringWriter();
            dumper.write(writer);
            return writer.toString();
        } catch (IOException e) {
            return "Unable to dump FeedProcessScope";
        }
    }

    @CalledByNative
    static void triggerRefresh() {
        FeedProcessScope feedProcessScope = FeedProcessScopeFactory.getFeedProcessScope();

        // Do nothing if Feed is disabled.
        if (feedProcessScope == null) return;

        RequestManager requestManager = feedProcessScope.getRequestManager();

        // Trigger a refresh with the default consumer, so notification goes to the
        // FeedSchedulerHost and last fetch status and time will be updated.
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED);
    }
}
