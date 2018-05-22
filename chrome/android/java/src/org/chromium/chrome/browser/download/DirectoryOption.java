// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import java.io.File;

/**
 * Denotes a given option for directory selection; includes name, location, and space.
 */
public class DirectoryOption {
    /**
     * Name of the current download directory.
     */
    public final String name;

    /**
     * The file location of download directory.
     */
    public final File location;

    /**
     * The available space in this download directory.
     */
    public final long availableSpace;

    public DirectoryOption(String directoryName, File directoryLocation, long directorySpace) {
        name = directoryName;
        location = directoryLocation;
        availableSpace = directorySpace;
    }
}
