// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

/**
 * The exception that is thrown when the intialization of a process was failed.
 * TODO (aberent) the real code of this has to stay in
 * org.chromium.content.common.ProcessInitException until the downstream Android
 * code has switched to using this class.
 */
public class ProcessInitException extends org.chromium.content.common.ProcessInitException {
    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     */
    public ProcessInitException(int errorCode) {
        super(errorCode);
    }

    /**
     * @param errorCode This will be one of the LoaderErrors error codes.
     * @param throwable The wrapped throwable obj.
     */
    public ProcessInitException(int errorCode, Throwable throwable) {
        super(errorCode, throwable);
    }
}
