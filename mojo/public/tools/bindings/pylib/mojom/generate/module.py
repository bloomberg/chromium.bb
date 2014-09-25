# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This module's classes provide an interface to mojo modules. Modules are
# collections of interfaces and structs to be used by mojo ipc clients and
# servers.
#
# A simple interface would be created this way:
# module = mojom.generate.module.Module('Foo')
# interface = module.AddInterface('Bar')
# method = interface.AddMethod('Tat', 0)
# method.AddParameter('baz', 0, mojom.INT32)


class Kind(object):
  def __init__(self, spec=None):
    self.spec = spec
    self.parent_kind = None


class ReferenceKind(Kind):
  """ReferenceKind represents pointer types and handle types.
     A type is nullable means that NULL (for pointer types) or invalid handle
     (for handle types) is a legal value for the type.
  """

  def __init__(self, spec=None, is_nullable=False):
    assert spec is None or is_nullable == spec.startswith('?')
    Kind.__init__(self, spec)
    self.is_nullable = is_nullable
    self.shared_definition = {}

  def MakeNullableKind(self):
    assert not self.is_nullable

    if self == STRING:
      return NULLABLE_STRING
    if self == HANDLE:
      return NULLABLE_HANDLE
    if self == DCPIPE:
      return NULLABLE_DCPIPE
    if self == DPPIPE:
      return NULLABLE_DPPIPE
    if self == MSGPIPE:
      return NULLABLE_MSGPIPE
    if self == SHAREDBUFFER:
      return NULLABLE_SHAREDBUFFER

    nullable_kind = type(self)()
    nullable_kind.shared_definition = self.shared_definition
    if self.spec is not None:
      nullable_kind.spec = '?' + self.spec
    nullable_kind.is_nullable = True

    return nullable_kind

  @classmethod
  def AddSharedProperty(cls, name):
    """Adds a property |name| to |cls|, which accesses the corresponding item in
       |shared_definition|.

       The reason of adding such indirection is to enable sharing definition
       between a reference kind and its nullable variation. For example:
         a = Struct('test_struct_1')
         b = a.MakeNullableKind()
         a.name = 'test_struct_2'
         print b.name  # Outputs 'test_struct_2'.
    """
    def Get(self):
      return self.shared_definition[name]

    def Set(self, value):
      self.shared_definition[name] = value

    setattr(cls, name, property(Get, Set))


# Initialize the set of primitive types. These can be accessed by clients.
BOOL                  = Kind('b')
INT8                  = Kind('i8')
INT16                 = Kind('i16')
INT32                 = Kind('i32')
INT64                 = Kind('i64')
UINT8                 = Kind('u8')
UINT16                = Kind('u16')
UINT32                = Kind('u32')
UINT64                = Kind('u64')
FLOAT                 = Kind('f')
DOUBLE                = Kind('d')
STRING                = ReferenceKind('s')
HANDLE                = ReferenceKind('h')
DCPIPE                = ReferenceKind('h:d:c')
DPPIPE                = ReferenceKind('h:d:p')
MSGPIPE               = ReferenceKind('h:m')
SHAREDBUFFER          = ReferenceKind('h:s')
NULLABLE_STRING       = ReferenceKind('?s', True)
NULLABLE_HANDLE       = ReferenceKind('?h', True)
NULLABLE_DCPIPE       = ReferenceKind('?h:d:c', True)
NULLABLE_DPPIPE       = ReferenceKind('?h:d:p', True)
NULLABLE_MSGPIPE      = ReferenceKind('?h:m', True)
NULLABLE_SHAREDBUFFER = ReferenceKind('?h:s', True)


# Collection of all Primitive types
PRIMITIVES = (
  BOOL,
  INT8,
  INT16,
  INT32,
  INT64,
  UINT8,
  UINT16,
  UINT32,
  UINT64,
  FLOAT,
  DOUBLE,
  STRING,
  HANDLE,
  DCPIPE,
  DPPIPE,
  MSGPIPE,
  SHAREDBUFFER,
  NULLABLE_STRING,
  NULLABLE_HANDLE,
  NULLABLE_DCPIPE,
  NULLABLE_DPPIPE,
  NULLABLE_MSGPIPE,
  NULLABLE_SHAREDBUFFER
)


class NamedValue(object):
  def __init__(self, module, parent_kind, name):
    self.module = module
    self.namespace = module.namespace
    self.parent_kind = parent_kind
    self.name = name
    self.imported_from = None

  def GetSpec(self):
    return (self.namespace + '.' +
        (self.parent_kind and (self.parent_kind.name + '.') or "") +
        self.name)


class BuiltinValue(object):
  def __init__(self, value):
    self.value = value


class ConstantValue(NamedValue):
  def __init__(self, module, parent_kind, constant):
    NamedValue.__init__(self, module, parent_kind, constant.name)
    self.constant = constant


class EnumValue(NamedValue):
  def __init__(self, module, enum, field):
    NamedValue.__init__(self, module, enum.parent_kind, field.name)
    self.enum = enum

  def GetSpec(self):
    return (self.namespace + '.' +
        (self.parent_kind and (self.parent_kind.name + '.') or "") +
        self.enum.name + '.' + self.name)


class Constant(object):
  def __init__(self, name=None, kind=None, value=None):
    self.name = name
    self.kind = kind
    self.value = value


class Field(object):
  def __init__(self, name=None, kind=None, ordinal=None, default=None):
    self.name = name
    self.kind = kind
    self.ordinal = ordinal
    self.default = default


class Struct(ReferenceKind):
  ReferenceKind.AddSharedProperty('name')
  ReferenceKind.AddSharedProperty('module')
  ReferenceKind.AddSharedProperty('imported_from')
  ReferenceKind.AddSharedProperty('fields')

  def __init__(self, name=None, module=None):
    if name is not None:
      spec = 'x:' + name
    else:
      spec = None
    ReferenceKind.__init__(self, spec)
    self.name = name
    self.module = module
    self.imported_from = None
    self.fields = []

  def AddField(self, name, kind, ordinal=None, default=None):
    field = Field(name, kind, ordinal, default)
    self.fields.append(field)
    return field


class Array(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      ReferenceKind.__init__(self, 'a:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind


class FixedArray(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')
  ReferenceKind.AddSharedProperty('length')

  def __init__(self, length=-1, kind=None):
    if kind is not None:
      ReferenceKind.__init__(self, 'a%d:%s' % (length, kind.spec))
    else:
      ReferenceKind.__init__(self)
    self.kind = kind
    self.length = length


class InterfaceRequest(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      ReferenceKind.__init__(self, 'r:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind


class Parameter(object):
  def __init__(self, name=None, kind=None, ordinal=None, default=None):
    self.name = name
    self.ordinal = ordinal
    self.kind = kind
    self.default = default


class Method(object):
  def __init__(self, interface, name, ordinal=None):
    self.interface = interface
    self.name = name
    self.ordinal = ordinal
    self.parameters = []
    self.response_parameters = None

  def AddParameter(self, name, kind, ordinal=None, default=None):
    parameter = Parameter(name, kind, ordinal, default)
    self.parameters.append(parameter)
    return parameter

  def AddResponseParameter(self, name, kind, ordinal=None, default=None):
    if self.response_parameters == None:
      self.response_parameters = []
    parameter = Parameter(name, kind, ordinal, default)
    self.response_parameters.append(parameter)
    return parameter


class Interface(ReferenceKind):
  ReferenceKind.AddSharedProperty('module')
  ReferenceKind.AddSharedProperty('name')
  ReferenceKind.AddSharedProperty('imported_from')
  ReferenceKind.AddSharedProperty('client')
  ReferenceKind.AddSharedProperty('methods')

  def __init__(self, name=None, client=None, module=None):
    if name is not None:
      spec = 'x:' + name
    else:
      spec = None
    ReferenceKind.__init__(self, spec)
    self.module = module
    self.name = name
    self.imported_from = None
    self.client = client
    self.methods = []

  def AddMethod(self, name, ordinal=None):
    method = Method(self, name, ordinal=ordinal)
    self.methods.append(method)
    return method


class EnumField(object):
  def __init__(self, name=None, value=None):
    self.name = name
    self.value = value


class Enum(Kind):
  def __init__(self, name=None, module=None):
    self.module = module
    self.name = name
    self.imported_from = None
    if name is not None:
      spec = 'x:' + name
    else:
      spec = None
    Kind.__init__(self, spec)
    self.fields = []


class Module(object):
  def __init__(self, name=None, namespace=None):
    self.name = name
    self.path = name
    self.namespace = namespace
    self.structs = []
    self.interfaces = []

  def AddInterface(self, name):
    self.interfaces.append(Interface(name, module=self))
    return interface

  def AddStruct(self, name):
    struct=Struct(name, module=self)
    self.structs.append(struct)
    return struct


def IsBoolKind(kind):
  return kind.spec == BOOL.spec


def IsFloatKind(kind):
  return kind.spec == FLOAT.spec


def IsStringKind(kind):
  return kind.spec == STRING.spec or kind.spec == NULLABLE_STRING.spec


def IsHandleKind(kind):
  return kind.spec == HANDLE.spec or kind.spec == NULLABLE_HANDLE.spec


def IsDataPipeConsumerKind(kind):
  return kind.spec == DCPIPE.spec or kind.spec == NULLABLE_DCPIPE.spec


def IsDataPipeProducerKind(kind):
  return kind.spec == DPPIPE.spec or kind.spec == NULLABLE_DPPIPE.spec


def IsMessagePipeKind(kind):
  return kind.spec == MSGPIPE.spec or kind.spec == NULLABLE_MSGPIPE.spec


def IsSharedBufferKind(kind):
  return (kind.spec == SHAREDBUFFER.spec or
          kind.spec == NULLABLE_SHAREDBUFFER.spec)


def IsStructKind(kind):
  return isinstance(kind, Struct)


def IsArrayKind(kind):
  return isinstance(kind, Array)


def IsFixedArrayKind(kind):
  return isinstance(kind, FixedArray)


def IsInterfaceKind(kind):
  return isinstance(kind, Interface)


def IsInterfaceRequestKind(kind):
  return isinstance(kind, InterfaceRequest)


def IsEnumKind(kind):
  return isinstance(kind, Enum)


def IsReferenceKind(kind):
  return isinstance(kind, ReferenceKind)


def IsNullableKind(kind):
  return IsReferenceKind(kind) and kind.is_nullable


def IsAnyArrayKind(kind):
  return IsArrayKind(kind) or IsFixedArrayKind(kind)


def IsObjectKind(kind):
  return IsStructKind(kind) or IsAnyArrayKind(kind) or IsStringKind(kind)


def IsNonInterfaceHandleKind(kind):
  return (IsHandleKind(kind) or
          IsDataPipeConsumerKind(kind) or
          IsDataPipeProducerKind(kind) or
          IsMessagePipeKind(kind) or
          IsSharedBufferKind(kind))


def IsAnyHandleKind(kind):
  return (IsNonInterfaceHandleKind(kind) or
          IsInterfaceKind(kind) or
          IsInterfaceRequestKind(kind))


def IsMoveOnlyKind(kind):
  return IsObjectKind(kind) or IsAnyHandleKind(kind)


def HasCallbacks(interface):
  for method in interface.methods:
    if method.response_parameters != None:
      return True
  return False

