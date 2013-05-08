# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from base64 import b64encode
from hashlib import sha1
import os

def FormatKey(key):
  '''Normalize a key by making sure it has a .html extension, and convert any
  '.'s to '_'s.
  '''
  if key.endswith('.html'):
    key = key[:-len('.html')]
  safe_key = key.replace('.', '_')
  return '%s.html' % safe_key

def SanitizeAPIName(name):
  '''Sanitizes API filenames that are in subdirectories.
  '''
  filename = os.path.splitext(name)[0].replace(os.sep, '_')
  if 'experimental' in filename:
    filename = 'experimental_' + filename.replace('experimental_', '')
  return filename

def StringIdentity(string):
  '''Creates a small hash of a string.
  '''
  return b64encode(sha1(string).digest())[:8]
