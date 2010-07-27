# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Dummy target to satisfy gyp. It is also the placeholder for libcros, which
# will be checked out here.

{
  'targets': [
    { 'target_name': 'cros_api',
      'type': '<(library)',
    },
  ],
}
