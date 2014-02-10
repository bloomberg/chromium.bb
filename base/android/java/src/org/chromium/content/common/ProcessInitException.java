// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import org.chromium.base.library_loader.LoaderErrors;

/**
 * The exception that is thrown when the intialization of a process was failed.
 * TODO(aberent) Remove this once the downstream patch lands, and move this code to base.
 * In advance of its deletion this has been moved into the base directory structure, to
 * allow org.chromium.base.library_loader.ProcessInitException to derive from it. This
 * will go away as soon as the downstream patch lands.
 */
public class ProcessInitException extends Exception {
    private int mErrorCode = LoaderErrors.LOADER_ERROR_NORMAL_COMPLETION;

    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     */
    public ProcessInitException(int errorCode) {
        mErrorCode = errorCode;
    }

    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     * @param throwable The wrapped throwable obj.
     */
    public ProcessInitException(int errorCode, Throwable throwable) {
        super(null, throwable);
        mErrorCode = errorCode;
    }

    /**
     * Return the error code.
     */
    public int getErrorCode() {
        return mErrorCode;
    }
}
