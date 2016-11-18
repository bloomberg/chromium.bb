// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import java.util.Map;

/**
 * Meta data from the WebAPK's Android Manifest.
 */
public class WebApkMetaData {
    public int shellApkVersion;
    public String manifestUrl;
    public String startUrl;
    public String scope;
    public String name;
    public String shortName;
    public int displayMode;
    public int orientation;
    public long themeColor;
    public long backgroundColor;
    public int iconId;
    public Map<String, String> iconUrlAndIconMurmur2HashMap;
}
