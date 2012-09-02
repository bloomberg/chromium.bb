# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Purpose of this module is to hold common optparse functionality.

Currently not much, but should expand going forward.
"""

import optparse
from chromite.lib import osutils


def AbsolutePath(_option, opt, value):
  """Expand paths and make them absolute."""
  expanded = osutils.ExpandPath(value)
  if expanded == '/':
    raise optparse.OptionValueError('Invalid path %s specified for %s'
                                    % (expanded, opt))

  return expanded


def ValidateGSPath(_option, opt, value):
  """Expand paths and make them absolute."""
  value = value.strip().rstrip('/')
  if not value.startswith('gs://'):
    raise optparse.OptionValueError('Invalid gs path %s specified for %s'
                                    % (value, opt))

  return value


class Option(optparse.Option):
  """
  Subclass Option class to implement path evaluation, and other useful types.
  """
  TYPES = optparse.Option.TYPES + ('path', 'gs_path')
  TYPE_CHECKER = optparse.Option.TYPE_CHECKER.copy()
  TYPE_CHECKER['path'] = AbsolutePath
  TYPE_CHECKER['gs_path'] = ValidateGSPath


class OptionParser(optparse.OptionParser):

  """Custom parser adding our custom option class in."""

  DEFAULT_OPTION_CLASS = Option

  def __init__(self, usage=None, **kwargs):
    # Allow usage to be passed positionally, since that's semi common.
    kwargs.setdefault('option_class', self.DEFAULT_OPTION_CLASS)
    optparse.OptionParser.__init__(self, usage=usage, **kwargs)
