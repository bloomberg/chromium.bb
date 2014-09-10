# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The meta classes used by the mojo python bindings."""

class MojoEnumType(type):
  """Meta class for enumerations.

  Usage:
    class MyEnum(object):
      __metaclass__ = MojoEnumType
      VALUES = [
        ('A', 0),
        'B',
        ('C', 5),
      ]

      This will define a enum with 3 values, A = 0, B = 1 and C = 5.
  """
  def __new__(mcs, name, bases, dictionary):
    class_members = {
      '__new__': None,
    }
    for value in dictionary.pop('VALUES', []):
      if not isinstance(value, tuple):
        raise ValueError('incorrect value: %r' % value)
      key, enum_value = value
      if isinstance(key, str) and isinstance(enum_value, int):
        class_members[key] = enum_value
      else:
        raise ValueError('incorrect value: %r' % value)
    return type.__new__(mcs, name, bases, class_members)

  def __setattr__(mcs, key, value):
    raise AttributeError, 'can\'t set attribute'

  def __delattr__(mcs, key):
    raise AttributeError, 'can\'t delete attribute'
