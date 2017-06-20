// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

/** Implementation of ChildProcessService that uses the content specific delegate. */
public class ContentChildProcessService extends ChildProcessService {
    public ContentChildProcessService() {
        super(new ContentChildProcessServiceDelegate());
    }
}
