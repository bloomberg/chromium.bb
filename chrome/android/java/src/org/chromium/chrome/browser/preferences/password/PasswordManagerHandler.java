// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

/**
 * Interface for retrieving passwords and password exceptions (websites for which Chrome should not
 * save password) from native code.
 */
public interface PasswordManagerHandler {
    /**
     * An interface which a client can use to listen to changes to password and password exception
     * lists.
     */
    interface PasswordListObserver {
        /**
         * Called when passwords list is updated.
         * @param count Number of entries in the password list.
         */
        void passwordListAvailable(int count);

        /**
         * Called when password exceptions list is updated.
         * @param count Number of entries in the password exception list.
         */
        void passwordExceptionListAvailable(int count);
    }

    /**
     * Called to start fetching password and exception lists.
     */
    void updatePasswordLists();

    /**
     * Get the saved password entry at index.
     *
     * @param index Index of Password.
     * @return SavedPasswordEntry at index.
     */
    SavedPasswordEntry getSavedPasswordEntry(int index);

    /**
     * Get saved password exception at index.
     *
     * @param index of exception
     * @return Origin of password exception.
     */
    String getSavedPasswordException(int index);

    /**
     * Remove saved password entry at index.
     *
     * @param index of password entry to remove.
     */
    void removeSavedPasswordEntry(int index);

    /**
     * Remove saved exception entry at index.
     *
     * @param index of exception entry.
     */
    void removeSavedPasswordException(int index);

    /**
     * Trigger serializing the saved passwords in the background.
     *
     * @param callback is called on completion, with the serialized passwords and their count as
     * argument.
     */
    void serializePasswords(ByteArrayIntCallback callback);
}
