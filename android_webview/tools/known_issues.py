# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""List of directories which are known issues for Android WebView.

There are a number of directories in the Chromium tree which should be removed
when merging into Android. Some are for licensing reasons; others are to ensure
that the build inside the Android tree does not accidentally include the wrong
headers.

This is not used by the webview_licenses tool itself; it is effectively a
"cache" of the output of webview_licenses.GetIncompatibleDirectories() for the
subset of repositories that WebView needs.

We store a copy here because GetIncompatibleDirectories() doesn't work properly
after things have been removed from the tree - it can no longer see the
README.chromium files for previously-removed directories, but they may have
newly added files in them. As long as this list is up to date, we can remove the
things listed first, and then just run the tool afterwards to validate that it
was sufficient. If the tool returns any extra directories then the snapshotting
process will stop and this list must be updated.

"""

# If there is a temporary license-related issue with a particular third_party
# directory, please put it here, with a comment linking to the bug entry.
KNOWN_ISSUES = [
]

KNOWN_INCOMPATIBLE = {
    '.': [
        # Incompatibly licensed code from the main chromium src/ directory.
        'base/third_party/xdg_mime',
        'breakpad',
        'chrome/installer/mac/third_party/xz',
        'chrome/test/data',
        'third_party/apple_apsl',
        'third_party/apple_sample_code',
        'third_party/bsdiff',
        'third_party/bspatch',
        'third_party/elfutils',
        'third_party/instrumented_libraries',
        'third_party/liblouis',
        'third_party/speech-dispatcher',
        'third_party/sudden_motion_sensor',
        'third_party/swiftshader',
        'third_party/talloc',
        'third_party/webdriver',
        'third_party/wtl',
        'tools/telemetry/third_party/websocket-client',

        # Code we don't want to build/include by accident from the main chromium
        # src/ directory.
        'third_party/libjpeg/*.[ch]',
    ],
    'third_party/icu': [
        # Incompatible code from ICU's repository.
        'source/data/brkitr',
    ],
}

KNOWN_INCOMPATIBLE['.'].extend(KNOWN_ISSUES)
