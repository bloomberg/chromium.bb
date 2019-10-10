// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * This component allows to fill credentials into a form. It suppresses the keyboard until dismissed
 * and acts as a safe surface to fill credentials from.
 */
public interface TouchToFillComponent {
    /**
     * This delegate is called when the TouchToFill component is interacted with (e.g. dismissed or
     * a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user select one of the credentials shown in the TouchToFillComponent.
         */
        void onCredentialSelected(Credential credential);

        /**
         * Called when the user dismisses the TouchToFillComponent. Not called if a suggestion was
         * selected.
         */
        void onDismissed();
    }

    /**
     * Initializes the component.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles dismiss events.
     */
    void initialize(Context context, BottomSheetController sheetController, Delegate delegate);

    /**
     * Displays the given credentials in a new bottom sheet.
     * @param formattedUrl A {@link String} that contains the URL to display credentials for.
     * @param isOriginSecure A {@link boolean} that indicates whether the current origin is secure.
     * @param credentials A list of {@link Credential}s that will be displayed.
     */
    void showCredentials(String formattedUrl, boolean isOriginSecure, List<Credential> credentials);
}
