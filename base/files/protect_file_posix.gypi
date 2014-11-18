# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Provides sanity-checks and early crashes on some improper use of posix file
# descriptors. See protect_file_posix.cc for details.
#
# Usage:
#   {
#     'target_name': 'libsomething',
#     'type': 'shared_library',  // Do *not* use it for static libraries.
#     'includes': [
#       'base/files/protect_file_posix.gypi',
#     ],
#     ...
#   }
{
   'conditions': [
     # In the component build the interceptors have to be declared with
     # non-hidden visibility, which is not desirable for the release build.
     # Disable the extra checks for the component build for simplicity.
     ['component != "shared_library"', {
       'ldflags': [
         '-Wl,--wrap=close',
       ],
       'dependencies': [
         '<(DEPTH)/base/base.gyp:protect_file_posix',
       ],
     }],
   ],
}
