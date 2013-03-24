# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
   },
  'includes': [
    'autofill.gypi',
    'auto_login_parser.gypi',
    'components_tests.gypi',
    'navigation_interception.gypi',
    'user_prefs.gypi',
    'visitedlink.gypi',
    'webdata.gypi',
    'web_contents_delegate_android.gypi',
  ],
}
