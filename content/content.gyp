# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'includes': [
    'content_browser.gypi',
    'content_common.gypi',
    'content_gpu.gypi',
    'content_plugin.gypi',
    'content_ppapi_plugin.gypi',
    'content_renderer.gypi',
    'content_worker.gypi',
  ],
}
