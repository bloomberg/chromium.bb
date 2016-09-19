// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

/**
 * A physical web collection stores UrlInfos and PwsResults for scanned URLs.
 */
public class PwCollection {
    public final UrlInfo[] urlInfos;
    public final PwsResult[] pwsResults;

    /**
     * Construct a PwCollection.
     * @param urlInfos All nearby URL infos.
     * @param pwsResults The metadata for nearby URLs.
     */
    public PwCollection(UrlInfo[] urlInfos, PwsResult[] pwsResults) {
        this.urlInfos = urlInfos;
        this.pwsResults = pwsResults;
    }
}
