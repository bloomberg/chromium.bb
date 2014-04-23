// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

/**
 * Access to the core singleton.
 */
public class CoreSingleton {

    /**
     * Access to the {@link Core} singleton.
     */
    public static Core getInstance() {
        return CoreImpl.getInstance();
    }
}
