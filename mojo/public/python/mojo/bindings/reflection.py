# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The metaclasses used by the mojo python bindings."""

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

      This will define a enum with 3 values, 'A' = 0, 'B' = 1 and 'C' = 5.
  """

  def __new__(mcs, name, bases, dictionary):
    dictionary['__slots__'] = ()
    dictionary['__new__'] = None
    for value in dictionary.pop('VALUES', []):
      if not isinstance(value, tuple):
        raise ValueError('incorrect value: %r' % value)
      key, enum_value = value
      if isinstance(key, str) and isinstance(enum_value, int):
        dictionary[key] = enum_value
      else:
        raise ValueError('incorrect value: %r' % value)
    return type.__new__(mcs, name, bases, dictionary)

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
        'fields': [
           FieldDescriptor('x', _descriptor.TYPE_INT32, 0),
        ],
      }

      This will define an struct, with:
      - 2 constants 'C1' and 'C2';
      - 2 enums 'ENUM1' and 'ENUM2', each of those having 2 values, 'V1' and
        'V2';
      - 1 int32 field named 'x'.
  """

  def __new__(mcs, name, bases, dictionary):
    dictionary['__slots__'] = ('_fields')
    descriptor = dictionary.pop('DESCRIPTOR', {})

    # Add constants
    dictionary.update(descriptor.get('constants', {}))

    # Add enums
    enums = descriptor.get('enums', {})
    for key in enums:
      dictionary[key] = MojoEnumType(key, (object,), { 'VALUES': enums[key] })

    # Add fields
    for field in descriptor.get('fields', []):
      dictionary[field.name] = _BuildProperty(field)

    # Add init
    dictionary['__init__'] = _StructInit

    return type.__new__(mcs, name, bases, dictionary)

  # Prevent adding new attributes, or mutating constants.
  def __setattr__(mcs, key, value):
    raise AttributeError, 'can\'t set attribute'

  # Prevent deleting constants.
  def __delattr__(mcs, key):
    raise AttributeError, 'can\'t delete attribute'


def _StructInit(self, **kwargs):
  self._fields = {}
  for name in kwargs:
    self.__setattr__(name, kwargs[name])


def _BuildProperty(field):
  """Build the property for the given field."""

  # pylint: disable=W0212
  def Get(self):
    if field.name not in self._fields:
      self._fields[field.name] = field.GetDefaultValue()
    return self._fields[field.name]

  # pylint: disable=W0212
  def Set(self, value):
    self._fields[field.name] = field.field_type.Convert(value)

  return property(Get, Set)
