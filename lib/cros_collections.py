# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite extensions on top of the collections module."""

from __future__ import print_function


def Collection(classname, **kwargs):
  """Create a new class with mutable named members.

  This is like collections.namedtuple, but mutable.  Also similar to the
  python 3.3 types.SimpleNamespace.

  Examples:
    # Declare default values for this new class.
    Foo = cros_build_lib.Collection('Foo', a=0, b=10)
    # Create a new class but set b to 4.
    foo = Foo(b=4)
    # Print out a (will be the default 0) and b (will be 4).
    print('a = %i, b = %i' % (foo.a, foo.b))
  """

  def sn_init(self, **kwargs):
    """The new class's __init__ function."""
    # First verify the kwargs don't have excess settings.
    valid_keys = set(self.__slots__[1:])
    these_keys = set(kwargs.keys())
    invalid_keys = these_keys - valid_keys
    if invalid_keys:
      raise TypeError('invalid keyword arguments for this object: %r' %
                      invalid_keys)

    # Now initialize this object.
    for k in valid_keys:
      setattr(self, k, kwargs.get(k, self.__defaults__[k]))

  def sn_repr(self):
    """The new class's __repr__ function."""
    return '%s(%s)' % (classname, ', '.join(
        '%s=%r' % (k, getattr(self, k)) for k in self.__slots__[1:]))

  # Give the new class a unique name and then generate the code for it.
  classname = 'Collection_%s' % classname
  expr = '\n'.join((
      'class %(classname)s(object):',
      '  __slots__ = ["__defaults__", "%(slots)s"]',
      '  __defaults__ = {}',
  )) % {
      'classname': classname,
      'slots': '", "'.join(sorted(str(k) for k in kwargs)),
  }

  # Create the class in a local namespace as exec requires.
  namespace = {}
  exec expr in namespace  # pylint: disable=exec-used
  new_class = namespace[classname]

  # Bind the helpers.
  new_class.__defaults__ = kwargs.copy()
  new_class.__init__ = sn_init
  new_class.__repr__ = sn_repr

  return new_class
