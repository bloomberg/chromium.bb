# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import re
import types

_SEGMENT_RE_BASE = r'[a-zA-Z0-9][a-zA-Z0-9:_\-.]*'
_STREAM_NAME_RE = re.compile('^(' + _SEGMENT_RE_BASE + ')(/' +
                             _SEGMENT_RE_BASE + ')*$')
_MAX_STREAM_NAME_LENGTH = 4096

_MAX_TAG_KEY_LENGTH = 64
_MAX_TAG_VALUE_LENGTH = 4096

def validate_stream_name(v, maxlen=None):
  """Verifies that a given stream name is valid.

  Args:
    v (str): The stream name string.


  Raises:
    ValueError if the stream name is invalid.
  """
  maxlen = maxlen or _MAX_STREAM_NAME_LENGTH
  if len(v) > maxlen:
    raise ValueError('Maximum length exceeded (%d > %d)' % (len(v), maxlen))
  if _STREAM_NAME_RE.match(v) is None:
    raise ValueError('Invalid stream name')


def validate_tag(key, value):
  """Verifies that a given tag key/value is valid.

  Args:
    k (str): The tag key.
    v (str): The tag value.

  Raises:
    ValueError if the tag is not valid.
  """
  validate_stream_name(key, maxlen=_MAX_TAG_KEY_LENGTH)
  validate_stream_name(value, maxlen=_MAX_TAG_VALUE_LENGTH)
