// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.network.HttpRequest;
import com.google.android.libraries.feed.host.network.HttpResponse;
import com.google.android.libraries.feed.host.network.NetworkClient;

/** A {@link NetworkClient} implementation for use in testing. */
class TestNetworkClient implements NetworkClient {
    @Override
    public void send(HttpRequest request, Consumer<HttpResponse> responseConsumer) {}

    @Override
    public void close() throws Exception {}
}
