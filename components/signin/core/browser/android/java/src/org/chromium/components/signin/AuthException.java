// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

/**
 * AuthException abstracts away authenticator specific exceptions behind a single interface.
 * It is used for passing information that is useful for better handling of errors.
 */
public class AuthException extends Exception {
    public static final boolean TRANSIENT = true;
    public static final boolean NONTRANSIENT = false;

    private final boolean mIsTransientError;

    /**
     * Wraps exception that caused auth failure along with transience flag.
     * @param isTransientError Whether the error is transient and we can retry.
     *         Use {@link #TRANSIENT} and {@link #NONTRANSIENT} for readability.
     * @param cause Exception that caused auth failure.
     */
    public AuthException(boolean isTransientError, Exception cause) {
        super(cause);
        mIsTransientError = isTransientError;
    }

    /**
     * Wraps exception that caused auth failure along with transience flag and message.
     * @param isTransientError Whether the error is transient and we can retry.
     *         Use {@link #TRANSIENT} and {@link #NONTRANSIENT} for readability.
     * @param message Message describing context in which auth failure happened.
     * @param cause Exception that caused auth failure.
     */
    public AuthException(boolean isTransientError, String message, Exception cause) {
        super(message, cause);
        mIsTransientError = isTransientError;
    }

    /**
     * @return Whether the error is transient and we can retry.
     */
    public boolean isTransientError() {
        return mIsTransientError;
    }
}
