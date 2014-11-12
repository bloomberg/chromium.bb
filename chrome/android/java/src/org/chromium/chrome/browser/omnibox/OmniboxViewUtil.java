// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Utilities for the Omnibox view component.
 */
public class OmniboxViewUtil {

    /**
     * Describes the components of a URL that should be emphasized.
     */
    public static class EmphasizeComponentsResponse {
        /** The start index of the scheme. */
        public final int schemeStart;
        /** The length of the scheme. */
        public final int schemeLength;
        /** The start index of the host. */
        public final int hostStart;
        /** The length of the host. */
        public final int hostLength;

        EmphasizeComponentsResponse(
                int schemeStart, int schemeLength, int hostStart, int hostLength) {
            this.schemeStart = schemeStart;
            this.schemeLength = schemeLength;
            this.hostStart = hostStart;
            this.hostLength = hostLength;
        }

        /**
         * @return Whether the URL has a scheme to be emphasized.
         */
        public boolean hasScheme() {
            return schemeLength > 0;
        }

        /**
         * @return Whether the URL has a host to be emphasized.
         */
        public boolean hasHost() {
            return hostLength > 0;
        }
    }

    /**
     * Sanitizing the given string to be safe to paste into the omnibox.
     *
     * @param clipboardString The string from the clipboard.
     * @return The sanitized version of the string.
     */
    public static String sanitizeTextForPaste(String clipboardString) {
        return nativeSanitizeTextForPaste(clipboardString);
    }

    /**
     * Parses the |text| passed in and determines the location of the scheme and host
     * components to be emphasized.
     *
     * @param profile The profile to be used for parsing.
     * @param text The text to be parsed for emphasis components.
     * @return The response object containing the locations of the emphasis components.
     */
    public static EmphasizeComponentsResponse parseForEmphasizeComponents(
            Profile profile, String text) {
        int[] emphasizeValues = nativeParseForEmphasizeComponents(profile, text);
        assert emphasizeValues != null;
        assert emphasizeValues.length == 4;

        return new EmphasizeComponentsResponse(
                emphasizeValues[0], emphasizeValues[1], emphasizeValues[2], emphasizeValues[3]);
    }

    private static native String nativeSanitizeTextForPaste(String clipboardString);
    private static native int[] nativeParseForEmphasizeComponents(Profile profile, String text);
}
