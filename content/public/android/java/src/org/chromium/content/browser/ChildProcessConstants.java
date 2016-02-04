// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

/**
 * Constants to be used by child processes.
 */
public class ChildProcessConstants {
    // Names of items placed in the bind or start command intent, or connection bundle.
    // Used to pass the command line params to child processes.
    public static final String EXTRA_COMMAND_LINE =
            "com.google.android.apps.chrome.extra.command_line";
    // Used to pass file descriptors to child processes.
    public static final String EXTRA_FILES = "com.google.android.apps.chrome.extra.extraFiles";
    // Used to pass the CPU core count to child processes.
    public static final String EXTRA_CPU_COUNT = "com.google.android.apps.chrome.extra.cpu_count";
    // Used to pass the CPU features mask to child processes.
    public static final String EXTRA_CPU_FEATURES =
            "com.google.android.apps.chrome.extra.cpu_features";

    // Used to pass the IChildProcessCallback to a child process. For a started service,
    // file descriptors cannot be passed in the intent that starts the service. As a result,
    // a Binder object needs to be passed through which the child process can retrieve the file
    // descriptors.
    public static final String EXTRA_CHILD_PROCESS_CALLBACK =
            "com.google.android.apps.chrome.extra.child_process_callback";
}
