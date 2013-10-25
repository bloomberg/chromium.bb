# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# mojom's classes provide an interface to mojo modules. Modules are collections
# of interfaces and structs to be used by mojo ipc clients and servers.
#
# A simple interface would be created this way:
# module = mojom.Module('Foo')
# interface = module.AddInterface('Bar')
# method = interface.AddMethod('Tat', 0)
# method.AddParameter('baz', 0, mojom.INT32)
#
class Kind(object):
  def __init__(self, spec = None):
    self.spec = spec


# Initialize the set of primitive types. These can be accessed by clients.
BOOL   = Kind('b')
INT8   = Kind('i8')
INT16  = Kind('i16')
INT32  = Kind('i32')
INT64  = Kind('i64')
UINT8  = Kind('u8')
UINT16 = Kind('u16')
UINT32 = Kind('u32')
UINT64 = Kind('u64')
FLOAT  = Kind('f')
DOUBLE = Kind('d')
STRING = Kind('s')
HANDLE = Kind('h')


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
  HANDLE
)


class Field(object):
  def __init__(self, name = None, kind = None, ordinal = None, default = None):
    self.name = name
    self.kind = kind
    self.ordinal = ordinal
    self.default = default


class Struct(Kind):
  def __init__(self, name = None):
    self.name = name
    if name != None:
      spec = 'x:' + name
    else:
      spec = None
    Kind.__init__(self, spec)
    self.fields = []

  def AddField(self, name, kind, ordinal = None, default = None):
    field = Field(name, kind, ordinal, default)
    self.fields.append(field)
    return field


class Array(Kind):
  def __init__(self, kind = None):
    self.kind = kind
    if kind != None:
      Kind.__init__(self, 'a:' + kind.spec)
    else:
      Kind.__init__(self)


class Parameter(object):
  def __init__(self, name = None, kind = None, ordinal = None, default = None):
    self.name = name
    self.ordinal = ordinal
    self.kind = kind
    self.default = default


class Method(object):
  def __init__(self, name = None, ordinal = None):
    self.name = name
    self.ordinal = ordinal
    self.parameters = []

  def AddParameter(self, name, kind, ordinal = None, default = None):
    parameter = Parameter(name, kind, ordinal, default)
    self.parameters.append(parameter)
    return parameter


class Interface(object):
  def __init__(self, name = None):
    self.name = name
    self.methods = []

  def AddMethod(self, name, ordinal = None):
    method = Method(name, ordinal)
    self.methods.append(method)
    return method;


class Module(object):
  def __init__(self, name = None, namespace = None):
    self.name = name
    self.namespace = namespace
    self.structs = []
    self.interfaces = []

  def AddInterface(self, name):
    interface = Interface(name);
    self.interfaces.append(interface)
    return interface;

  def AddStruct(self, name):
    struct = Struct(name)
    self.structs.append(struct)
    return struct;
