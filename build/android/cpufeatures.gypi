# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Depend on the Android NDK's cpu feature detection. This additional level of
# abstraction is no longer necessary and targets can depend directly on
# build/android/ndk.gyp:cpu_features instead.
# TODO(torne): delete this once all DEPS have been rolled to not use it.
# http://crbug.com/440793
{
  'dependencies': [
    '<(DEPTH)/build/android/ndk.gyp:cpu_features',
  ],
}
