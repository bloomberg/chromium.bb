# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines a common interface for using DOM selectors."""


class DOMSelector(object):
  """Generic class implementing a DOM selector."""
  type_string = None

  def __init__(self, pattern):
    """Initialize selector.

    Args:
     pattern: A string pattern formatted as a selector.
    """
    self.pattern = pattern


class XPath(DOMSelector):
  """An XPath type selector."""
  type_string = 'xpath'


class CSSSelector(DOMSelector):
  """A CSS Selector type selector."""
  type_string = 'css'
