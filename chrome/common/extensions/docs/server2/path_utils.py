# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def FormatKey(key):
  """Normalize a key by making sure it has a .html extension, and convert any
  '.'s to '_'s.
  """
  if key.endswith('.html'):
    key = key[:-5]
  safe_key = key.replace('.', '_')
  return safe_key + '.html'
