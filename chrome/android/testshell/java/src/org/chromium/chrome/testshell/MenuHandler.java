// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

/**
 * By implementing this interface the class declares that it has functionality available to open a
 * popup menu visible to the user.
 */
public interface MenuHandler {
    public void showPopupMenu();
}