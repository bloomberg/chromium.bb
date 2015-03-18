// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.CalledByNative;

/**
 * Credential type which is used to represent credential which will be shown in account chooser
 * infobar.
 * */
public class Credential {
    private final String mUsername;
    private final String mDisplayName;
    private final int mType;
    private final int mIndex;

    /**
     * @param username username which is used to authenticate user.
     *                 The value is PasswordForm::username_value.
     * @param displayName user friendly name to show in the UI. It can be empty.
     *                    The value is PasswordForm::display_name.
     * @param type type which should be either local or federated. The value corresponds to a
     *             C++ enum CredentialType.
     * @param index position in array of credentials.
     */
    public Credential(String username, String displayName, int type, int index) {
        mUsername = username;
        mDisplayName = displayName;
        mType = type;
        mIndex = index;
    }

    public String getUsername() {
        return mUsername;
    }

    public String getDisplayName() {
        return mDisplayName;
    }

    public int getIndex() {
        return mIndex;
    }

    public int getType() {
        return mType;
    }

    @CalledByNative
    private static Credential createCredential(
            String username, String displayName, int type, int index) {
        return new Credential(username, displayName, type, index);
    }

    @CalledByNative
    private static Credential[] createCredentialArray(int size) {
        return new Credential[size];
    }
}
