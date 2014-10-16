// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.content_public.browser.WebContents;

/**
 * @deprecated This class has been renamed to {@link WebContentsObserver} and
 * will be removed when all usages have been removed. See http://crbug.com/3961
 */
@Deprecated
public abstract class WebContentsObserverAndroid extends WebContentsObserver {

    public WebContentsObserverAndroid(WebContents webContents) {
        super(webContents);
    }
}
