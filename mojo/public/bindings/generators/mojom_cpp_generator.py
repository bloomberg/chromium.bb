# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import mojom
import mojom_pack
import os
import sys

from string import Template

# mojom_cpp_generator provides a way to generate c++ code from a mojom.Module.
# cpp = mojom_cpp_generator.CPPGenerator(module)
# cpp.GenerateFiles("/tmp/g")
def ReadTemplate(filename):
  dir = os.path.dirname(__file__)
  with open(dir + '/' + filename, 'r') as file:
    return Template(file.read())


class CPPGenerator(object):
  struct_template = ReadTemplate("cpp_struct.template")
  interface_template = ReadTemplate("cpp_interface.template")
  field_template = Template("  $itype ${field}_;")
  bool_field_template = Template("  uint8_t ${field}_ : 1;")
  setter_template = \
      Template("  void set_$field($stype $field) { ${field}_ = $field; }")
  getter_template = \
      Template("  $gtype $field() const { return ${field}_; }")
  ptr_getter_template = \
      Template("  $gtype $field() const { return ${field}_.ptr; }")
  pad_template = Template("  uint8_t _pad${count}_[$pad];")

  kind_to_type = {
    mojom.BOOL:   "bool",
    mojom.INT8:   "int8_t",
    mojom.UINT8:  "uint8_t",
    mojom.INT16:  "int16_t",
    mojom.UINT16: "uint16_t",
    mojom.INT32:  "int32_t",
    mojom.UINT32: "uint32_t",
    mojom.FLOAT:  "float",
    mojom.HANDLE: "mojo::Handle",
    mojom.INT64:  "int64_t",
    mojom.UINT64: "uint64_t",
    mojom.DOUBLE: "double",
  }

  @classmethod
  def GetType(cls, kind):
    if isinstance(kind, mojom.Struct):
      return "%s*" % kind.name.capitalize()
    if isinstance(kind, mojom.Array):
      return "mojo::Array<%s>*" % cls.GetType(kind.kind)
    if kind.spec == 's':
      return "mojo::String*"
    return cls.kind_to_type[kind]

  @classmethod
  def GetConstType(cls, kind):
    if isinstance(kind, mojom.Struct):
      return "const %s*" % kind.name.capitalize()
    if isinstance(kind, mojom.Array):
      return "const mojo::Array<%s>*" % cls.GetConstType(kind.kind)
    if kind.spec == 's':
      return "const mojo::String*"
    return cls.kind_to_type[kind]

  @classmethod
  def GetGetterLine(cls, pf):
    kind = pf.field.kind
    template = None
    gtype = cls.GetConstType(kind)
    if isinstance(kind, (mojom.Struct, mojom.Array)) or kind.spec == 's':
      template = cls.ptr_getter_template
    else:
      template = cls.getter_template
    return template.substitute(field=pf.field.name, gtype=gtype)

  @classmethod
  def GetSetterLine(cls, pf):
    stype = cls.GetType(pf.field.kind)
    return cls.setter_template.substitute(field=pf.field.name, stype=stype)

  @classmethod
  def GetFieldLine(cls, pf):
    kind = pf.field.kind
    if kind.spec == 'b':
      return cls.bool_field_template.substitute(field=pf.field.name)
    itype = None
    if isinstance(kind, mojom.Struct):
      itype = "mojo::internal::StructPointer<%s>" % kind.name.capitalize()
    elif isinstance(kind, mojom.Array):
      itype = "mojo::internal::StructPointer<%s>" % cls.GetType(kind.kind)
    elif kind.spec == 's':
      itype = "mojo::internal::StringPointer"
    else:
      itype = cls.kind_to_type[kind]
    return cls.field_template.substitute(field=pf.field.name, itype=itype)

  def GetHeaderGuard(self, component):
    return "MOJO_GENERATED_BINDINGS_%s_%s_H_" % \
        (self.module.name.upper(), component.name.upper())

  def OpenComponentFile(self, directory, component):
    filename = "%s_%s.h" % \
        (self.module.name.lower(), component.name.lower())
    return open(directory + '/' + filename, "w+")

  def __init__(self, module):
    self.module = module
    self.file = file

  def GenerateStruct(self, struct, file):
    fields = []
    setters = []
    getters = []

    ps = mojom_pack.PackedStruct(struct)
    pad_count = 0
    num_fields = len(ps.packed_fields)
    for i in xrange(num_fields):
      pf = ps.packed_fields[i]
      fields.append(self.GetFieldLine(pf))
      if i < (num_fields - 2):
        next_pf = ps.packed_fields[i+1]
        pad = next_pf.offset - (pf.offset + pf.size)
        if pad > 0:
          fields.append(self.pad_template.substitute(count=pad_count, pad=pad))
          pad_count += 1
      setters.append(self.GetSetterLine(pf))
      getters.append(self.GetGetterLine(pf))

    if num_fields > 0:
      last_field = ps.packed_fields[num_fields - 1]
      offset = last_field.offset + last_field.size
      pad = mojom_pack.GetPad(offset, 8)
      if pad > 0:
        fields.append(self.pad_template.substitute(count=pad_count, pad=pad))
        pad_count += 1
      size = offset + pad
    else:
      size = 0

    file.write(self.struct_template.substitute(
      year = datetime.date.today().year,
      header_guard = self.GetHeaderGuard(struct),
      namespace = self.module.namespace,
      classname = struct.name.capitalize(),
      num_fields = len(ps.packed_fields),
      size = size + 8,
      setters = '\n'.join(setters),
      getters = '\n'.join(getters),
      fields = '\n'.join(fields)))

  def GenerateInterface(self, interface, file):
    cpp_methods = []
    for method in interface.methods:
      cpp_method = "  virtual void %s(" % method.name
      first_param = True
      for param in method.parameters:
        if first_param == True:
          first_param = False
        else:
          cpp_method += ", "
        cpp_method += "%s %s" % (self.GetConstType(param.kind), param.name)
      cpp_method += ") = 0;"
      cpp_methods.append(cpp_method)

    file.write(self.interface_template.substitute(
      year = datetime.date.today().year,
      header_guard = self.GetHeaderGuard(interface),
      namespace = self.module.namespace,
      classname = interface.name.capitalize(),
      methods = '\n'.join(cpp_methods)))

  # Pass |directory| to emit files to disk. Omit |directory| to echo all files
  # to stdout.
  def GenerateFiles(self, directory=None):
    for struct in self.module.structs:
      if directory is None:
        self.GenerateStruct(struct, sys.stdout)
      else:
        with self.OpenComponentFile(directory, struct) as file:
          self.GenerateStruct(struct, file)
    for interface in self.module.interfaces:
      if directory is None:
        self.GenerateInterface(interface, sys.stdout)
      else:
        with self.OpenComponentFile(directory, interface) as file:
          self.GenerateInterface(interface, file)
