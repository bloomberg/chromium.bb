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


class MojoStructType(type):
  """Meta class for structs.

  Usage:
    class MyStruct(object):
      __metaclass__ = MojoStructType
      DESCRIPTOR = {
        'constants': {
          'C1': 1,
          'C2': 2,
        },
        'enums': {
          'ENUM1': [
            ('V1', 1),
            'V2',
          ],
          'ENUM2': [
            ('V1', 1),
            'V2',
          ],
        },
      }

      This will define an struct, with 2 constants C1 and C2, and 2 enums ENUM1
      and ENUM2, each of those having 2 values, V1 and V2.
  """

  def __new__(mcs, name, bases, dictionary):
    class_members = {
      '__slots__': [],
    }
    descriptor = dictionary.pop('DESCRIPTOR', {})

    # Add constants
    class_members.update(descriptor.get('constants', {}))

    # Add enums
    enums = descriptor.get('enums', {})
    for key in enums:
      class_members[key] = MojoEnumType(key,
                                        (object,),
                                        { 'VALUES': enums[key] })

    return type.__new__(mcs, name, bases, class_members)

  def __setattr__(mcs, key, value):
    raise AttributeError, 'can\'t set attribute'

  def __delattr__(mcs, key):
    raise AttributeError, 'can\'t delete attribute'
