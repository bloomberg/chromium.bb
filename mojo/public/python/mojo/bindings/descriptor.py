# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
The descriptors used to define generated elements of the mojo python bindings.
"""

# pylint: disable=F0401
from mojo.system import Handle

class Type(object):
  """Describes the type of a struct field or a method parameter,"""

  def Convert(self, value): # pylint: disable=R0201
    """
    Convert the given value into its canonical representation, raising an
    exception if the value cannot be converted.
    """
    return value

  def GetDefaultValue(self, value):
    """
    Returns the default value for this type associated with the given value.
    This method must be able to correcly handle value being None.
    """
    return self.Convert(value)


class BooleanType(Type):
  """Type object for booleans"""

  def Convert(self, value):
    return bool(value)


class NumericType(Type):
  """Base Type object for all numeric types"""

  def GetDefaultValue(self, value):
    if value is None:
      return self.convert(0)
    return self.Convert(value)


class IntegerType(NumericType):
  """Type object for integer types."""

  def __init__(self, size, signed):
    NumericType.__init__(self)
    self.size = size
    self.signed = signed
    if self.signed:
      self._min_value = -(1 << (size - 1))
      self._max_value = (1 << (size - 1)) - 1
    else:
      self._min_value = 0
      self._max_value = (1 << size) - 1

  def Convert(self, value):
    if value is None:
      raise ValueError('None is not an integer.')
    if not isinstance(value, (int, long)):
      raise ValueError('%r is not an integer type' % value)
    if value < self._min_value or value > self._max_value:
      raise ValueError('%r is not in the range [%d, %d]' %
                       (value, self._min_value, self._max_value))
    return value


class FloatType(NumericType):
  """Type object for floating point number types."""

  def __init__(self, size):
    NumericType.__init__(self)
    self.size = size

  def Convert(self, value):
    if value is None:
      raise ValueError('None is not an floating point number.')
    if not isinstance(value, (int, long, float)):
      raise ValueError('%r is not a numeric type' % value)
    return float(value)


class StringType(Type):
  """
  Type object for strings.

  Strings are represented as unicode, and the conversion is done using the
  default encoding if a string instance is used.
  """

  def __init__(self, nullable=False):
    Type.__init__(self)
    self.nullable = nullable

  def Convert(self, value):
    if value is None or isinstance(value, unicode):
      return value
    if isinstance(value, str):
      return unicode(value)
    raise ValueError('%r is not a string' % value)


class HandleType(Type):
  """Type object for handles."""

  def __init__(self, nullable=False):
    Type.__init__(self)
    self.nullable = nullable

  def Convert(self, value):
    if value is None:
      return Handle()
    if not isinstance(value, Handle):
      raise ValueError('%r is not a handle' % value)
    return value


class ArrayType(Type):
  """Type object for arrays."""

  def __init__(self, sub_type, nullable=False, length=0):
    Type.__init__(self)
    self.sub_type = sub_type
    self.nullable = nullable
    self.length = length

  def Convert(self, value):
    if value is None:
      return value
    return [self.sub_type.Convert(x) for x in value]


class StructType(Type):
  """Type object for structs."""

  def __init__(self, struct_type, nullable=False):
    Type.__init__(self)
    self.struct_type = struct_type
    self.nullable = nullable

  def Convert(self, value):
    if value is None or isinstance(value, self.struct_type):
      return value
    raise ValueError('%r is not an instance of %r' % (value, self.struct_type))

  def GetDefaultValue(self, value):
    if value:
      return self.struct_type()
    return None


class NoneType(Type):
  """Placeholder type, used temporarily until all mojo types are handled."""

  def Convert(self, value):
    return None


TYPE_NONE = NoneType()

TYPE_BOOL = BooleanType()

TYPE_INT8 = IntegerType(8, True)
TYPE_INT16 = IntegerType(16, True)
TYPE_INT32 = IntegerType(32, True)
TYPE_INT64 = IntegerType(64, True)

TYPE_UINT8 = IntegerType(8, False)
TYPE_UINT16 = IntegerType(16, False)
TYPE_UINT32 = IntegerType(32, False)
TYPE_UINT64 = IntegerType(64, False)

TYPE_FLOAT = FloatType(4)
TYPE_DOUBLE = FloatType(8)

TYPE_STRING = StringType()
TYPE_NULLABLE_STRING = StringType(True)

TYPE_HANDLE = HandleType()
TYPE_NULLABLE_HANDLE = HandleType(True)


class FieldDescriptor(object):
  """Describes a field in a generated struct."""

  def __init__(self, name, field_type, offset,
               bit_offset=None, default_value=None):
    self.name = name
    self.field_type = field_type
    self.offset = offset
    self.bit_offset = bit_offset
    self._default_value = default_value

  def GetDefaultValue(self):
    return self.field_type.GetDefaultValue(self._default_value)
