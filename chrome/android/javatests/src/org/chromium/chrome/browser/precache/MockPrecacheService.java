// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

/**
 * PrecacheService class with native initialization mocked out.
 */
public class MockPrecacheService extends PrecacheService {
    @Override
    void prepareNativeLibraries() {}
}
