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

def MarkLast(dicts):
  '''Adds a property 'last' == True to the last element in a list of dicts.
  '''
  if len(dicts) > 0:
    dicts[-1]['last'] = True

def ToUnicode(data):
  '''Returns the str |data| as a unicode object. It's expected to be utf8, but
  there are also latin-1 encodings in there for some reason. Fall back to that.
  '''
  try:
    return unicode(data, 'utf-8')
  except:
    return unicode(data, 'latin-1')
