# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

def FormatKey(key):
  """Normalize a key by making sure it has a .html extension, and convert any
  '.'s to '_'s.
  """
  if key.endswith('.html'):
    key = key[:-5]
  safe_key = key.replace('.', '_')
  return safe_key + '.html'

def SanitizeAPIName(name, api_path=''):
  """Sanitizes API filenames that are in subdirectories.
  """
  filename = os.path.splitext(name)[0][len(api_path):].replace(os.sep, '_')
  if 'experimental' in filename:
    filename = 'experimental_' + filename.replace('experimental_', '')
  return filename

def GetLinkToRefType(namespace_name, ref_type):
  """Returns a link given a $ref.
  """
  if ref_type.startswith(namespace_name + '.'):
    type_name = ref_type[len(namespace_name + '.'):]
    return { 'href': '#type-' + type_name, 'text': type_name }
  elif '.' not in ref_type:
    return { 'href': '#type-' + ref_type, 'text': ref_type }
  api, type_name = ref_type.rsplit('.', 1)
  return { 'href': api + '.html#type-' + type_name, 'text': ref_type }
