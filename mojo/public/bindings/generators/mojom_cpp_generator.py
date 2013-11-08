# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source files from a mojom.Module."""

import datetime
import mojom
import mojom_pack
import os
import re
import sys

from string import Template

# mojom_cpp_generator provides a way to generate c++ code from a mojom.Module.
# cpp = mojom_cpp_generator.CPPGenerator(module)
# cpp.GenerateFiles("/tmp/g")

class Lines(object):
  """Helper class to maintain list of template expanded lines."""
  def __init__(self, template, indent = None):
    self.template = template
    self.indent = indent
    self.lines = []

  def Add(self, map = {}, **substitutions):
    if len(substitutions) > 0:
      map = map.copy()
      map.update(substitutions)

    self.lines.append(self.template.substitute(map))

  def __repr__(self):
    if self.indent is not None:
      prefix = "".ljust(self.indent, ' ')
      repr = '\n'.join(self.lines)
      self.lines = map(lambda l: prefix + l, repr.splitlines())
    return '\n'.join(self.lines)


def GetStructFromMethod(interface, method):
  """Converts a method's parameters into the fields of a struct."""
  params_class = "%s_%s_Params" % (interface.name, method.name)
  struct = mojom.Struct(params_class)
  for param in method.parameters:
    struct.AddField(param.name, param.kind, param.ordinal)
  return struct

def IsPointerKind(kind):
  return isinstance(kind, (mojom.Struct, mojom.Array)) or kind.spec == 's'

def CamelToUnderscores(camel):
  s = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', camel)
  return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s).lower()

class CPPGenerator(object):

  struct_serialization_compute_template = \
    Template(" +\n      mojo::internal::ComputeSizeOf($NAME->$FIELD())")
  struct_serialization_clone_template = Template(
      "  clone->set_$FIELD(mojo::internal::Clone($NAME->$FIELD(), buf));")
  struct_serialization_encode_template = Template(
      "  Encode(&$NAME->${FIELD}_, handles);")
  struct_serialization_encode_handle_template = Template(
      "  EncodeHandle(&$NAME->${FIELD}_, handles);")
  struct_serialization_decode_template = Template(
      "  if (!Decode(&$NAME->${FIELD}_, message))\n"
      "    return false;")
  struct_serialization_decode_handle_template = Template(
      "  if (!DecodeHandle(&$NAME->${FIELD}_, message.handles))\n"
      "    return false;")
  param_set_template = Template("  params->set_$NAME($NAME);")
  param_struct_set_template = Template(
      "  params->set_$NAME(mojo::internal::Clone($NAME, builder.buffer()));")
  param_struct_compute_template = Template(
      "  payload_size += mojo::internal::ComputeSizeOf($NAME);")
  field_template = Template("  $TYPE ${FIELD}_;")
  bool_field_template = Template("  uint8_t ${FIELD}_ : 1;")
  setter_template = \
      Template("  void set_$FIELD($TYPE $FIELD) { ${FIELD}_ = $FIELD; }")
  ptr_setter_template = \
      Template("  void set_$FIELD($TYPE $FIELD) { ${FIELD}_.ptr = $FIELD; }")
  getter_template = \
      Template("  $TYPE $FIELD() const { return ${FIELD}_; }")
  ptr_getter_template = \
      Template("  $TYPE $FIELD() const { return ${FIELD}_.ptr; }")
  pad_template = Template("  uint8_t _pad${COUNT}_[$PAD];")
  templates  = {}
  HEADER_SIZE = 8

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
  def GetTemplate(cls, template_name):
    if template_name not in cls.templates:
      dir = os.path.dirname(__file__)
      filename = os.path.join(dir, 'cpp_templates', template_name)
      filename = filename.replace('.h', '.h-template')
      filename = filename.replace('.cc', '.cc-template')
      with open(filename, 'r') as file:
        template = Template(file.read())
        cls.templates[template_name] = template
    return cls.templates[template_name]

  @classmethod
  def GetType(cls, kind):
    if isinstance(kind, mojom.Struct):
      return "%s*" % kind.name
    if isinstance(kind, mojom.Array):
      return "mojo::Array<%s>*" % cls.GetType(kind.kind)
    if kind.spec == 's':
      return "mojo::String*"
    return cls.kind_to_type[kind]

  @classmethod
  def GetConstType(cls, kind):
    if isinstance(kind, mojom.Struct):
      return "const %s*" % kind.name
    if isinstance(kind, mojom.Array):
      return "const mojo::Array<%s>*" % cls.GetConstType(kind.kind)
    if kind.spec == 's':
      return "const mojo::String*"
    return cls.kind_to_type[kind]

  @classmethod
  def GetGetterLine(cls, field):
    subs = {'FIELD': field.name, 'TYPE': cls.GetType(field.kind)}
    if IsPointerKind(field.kind):
      return cls.ptr_getter_template.substitute(subs)
    else:
      return cls.getter_template.substitute(subs)

  @classmethod
  def GetSetterLine(cls, field):
    subs = {'FIELD': field.name, 'TYPE': cls.GetType(field.kind)}
    if IsPointerKind(field.kind):
      return cls.ptr_setter_template.substitute(subs)
    else:
      return cls.setter_template.substitute(subs)

  @classmethod
  def GetFieldLine(cls, field):
    kind = field.kind
    if kind.spec == 'b':
      return cls.bool_field_template.substitute(FIELD=field.name)
    itype = None
    if isinstance(kind, mojom.Struct):
      itype = "mojo::internal::StructPointer<%s>" % kind.name
    elif isinstance(kind, mojom.Array):
      itype = "mojo::internal::ArrayPointer<%s>" % cls.GetType(kind.kind)
    elif kind.spec == 's':
      itype = "mojo::internal::StringPointer"
    else:
      itype = cls.kind_to_type[kind]
    return cls.field_template.substitute(FIELD=field.name, TYPE=itype)

  @classmethod
  def GetCaseLine(cls, interface, method):
    params = map(lambda param: "params->%s()" % param.name, method.parameters)
    method_call = "%s(%s);" % (method.name, ", ".join(params))

    return cls.GetTemplate("interface_stub_case").substitute(
        CLASS = interface.name,
        METHOD = method.name,
        METHOD_CALL = method_call);

  @classmethod
  def GetSerializedFields(cls, ps):
    fields = []
    for pf in ps.packed_fields:
      if IsPointerKind(pf.field.kind):
        fields.append(pf.field)
    return fields

  @classmethod
  def GetHandleFields(cls, ps):
    fields = []
    for pf in ps.packed_fields:
      if pf.field.kind.spec == 'h':
        fields.append(pf.field)
    return fields

  def GetHeaderGuard(self, name):
    return "MOJO_GENERATED_BINDINGS_%s_%s_H_" % \
        (self.module.name.upper(), name.upper())

  def GetHeaderFile(self, *components):
    components = map(lambda c: CamelToUnderscores(c), components)
    component_string = '_'.join(components)
    return os.path.join(self.header_dir, "%s.h" % component_string)

  # Pass |output_dir| to emit files to disk. Omit |output_dir| to echo all files
  # to stdout.
  def __init__(self, module, header_dir, output_dir=None):
    self.module = module
    self.header_dir = header_dir
    self.output_dir = output_dir

  def WriteTemplateToFile(self, template_name, **substitutions):
    template = self.GetTemplate(template_name)
    filename = \
        template_name.replace("module", CamelToUnderscores(self.module.name))
    substitutions['YEAR'] = datetime.date.today().year
    substitutions['NAMESPACE'] = self.module.namespace
    if self.output_dir is None:
      file = sys.stdout
    else:
      file = open(os.path.join(self.output_dir, filename), "w+")
    try:
      file.write(template.substitute(substitutions))
    finally:
      if self.output_dir is not None:
        file.close()

  def GetStructDeclaration(self, name, ps, template, subs = {}):
    fields = []
    setters = []
    getters = []
    pad_count = 0
    num_fields = len(ps.packed_fields)
    for i in xrange(num_fields):
      pf = ps.packed_fields[i]
      field = pf.field
      fields.append(self.GetFieldLine(field))
      if i < (num_fields - 1):
        next_pf = ps.packed_fields[i+1]
        pad = next_pf.offset - (pf.offset + pf.size)
        if pad > 0:
          fields.append(self.pad_template.substitute(COUNT=pad_count, PAD=pad))
          pad_count += 1
      setters.append(self.GetSetterLine(field))
      getters.append(self.GetGetterLine(field))

    if num_fields > 0:
      last_field = ps.packed_fields[num_fields - 1]
      offset = last_field.offset + last_field.size
      pad = mojom_pack.GetPad(offset, 8)
      if pad > 0:
        fields.append(self.pad_template.substitute(COUNT=pad_count, PAD=pad))
        pad_count += 1
      size = offset + pad
    else:
      size = 0
    subs.update(
        CLASS = name,
        SETTERS = '\n'.join(setters),
        GETTERS = '\n'.join(getters),
        FIELDS = '\n'.join(fields),
        SIZE = size + self.HEADER_SIZE)
    return template.substitute(subs)

  def GetStructSerialization(
      self, class_name, param_name, ps, template, indent = None):
    struct = ps.struct
    encodes = Lines(self.struct_serialization_encode_template, indent)
    encode_handles = \
        Lines(self.struct_serialization_encode_handle_template, indent)
    decodes = Lines(self.struct_serialization_decode_template, indent)
    decode_handles = \
        Lines(self.struct_serialization_decode_handle_template, indent)
    fields = self.GetSerializedFields(ps)
    handle_fields = self.GetHandleFields(ps)
    for field in fields:
      substitutions = {'NAME': param_name, 'FIELD': field.name}
      encodes.Add(substitutions)
      decodes.Add(substitutions)
    for field in handle_fields:
      substitutions = {'NAME': param_name, 'FIELD': field.name}
      encode_handles.Add(substitutions)
      decode_handles.Add(substitutions)
    return template.substitute(
        CLASS = "%s::%s" % (self.module.namespace, class_name),
        NAME = param_name,
        ENCODES = encodes,
        DECODES = decodes,
        ENCODE_HANDLES = encode_handles,
        DECODE_HANDLES = decode_handles)

  def GetStructClassDeclarations(self):
    struct_decls = map(
        lambda s: self.GetStructDeclaration(
            s.name,
            mojom_pack.PackedStruct(s),
            self.GetTemplate("struct_declaration"),
            {}),
        self.module.structs)
    return '\n'.join(struct_decls)

  def GetInterfaceClassDeclaration(self, interface, template, method_postfix):
    methods = []
    for method in interface.methods:
      params = []
      for param in method.parameters:
        params.append("%s %s" % (self.GetConstType(param.kind), param.name))
      methods.append(
          "  virtual void %s(%s) %s;" %
              (method.name, ", ".join(params), method_postfix))
    return template.substitute(
        CLASS=interface.name,
        METHODS='.\n'.join(methods))

  def GetInterfaceClassDeclarations(self):
    template = self.GetTemplate("interface_declaration")
    interface_decls = \
        map(lambda i:
                self.GetInterfaceClassDeclaration(i, template, " = 0"),
            self.module.interfaces)
    return '\n'.join(interface_decls)

  def GetInterfaceProxyDeclarations(self):
    template = self.GetTemplate("interface_proxy_declaration")
    interface_decls = \
        map(lambda i:
                self.GetInterfaceClassDeclaration(i, template, "MOJO_OVERRIDE"),
            self.module.interfaces)
    return '\n'.join(interface_decls)

  def GetInterfaceStubDeclarations(self):
    template = self.GetTemplate("interface_stub_declaration")
    interface_decls = \
        map(lambda i: template.substitute(CLASS=i.name), self.module.interfaces)
    return '\n'.join(interface_decls)

  def GenerateModuleHeader(self):
    self.WriteTemplateToFile("module.h",
        HEADER_GUARD = self.GetHeaderGuard(self.module.name),
        STRUCT_CLASS_DECLARARTIONS = self.GetStructClassDeclarations(),
        INTERFACE_CLASS_DECLARATIONS = self.GetInterfaceClassDeclarations(),
        INTERFACE_PROXY_DECLARATIONS = self.GetInterfaceProxyDeclarations(),
        INTERFACE_STUB_DECLARATIONS = self.GetInterfaceStubDeclarations())

  def GetParamsDefinition(self, interface, method, next_id):
    struct = GetStructFromMethod(interface, method)
    method_name = "k%s_%s_Name" % (interface.name, method.name)
    if method.ordinal is not None:
      next_id = method.ordinal
    params_def = self.GetStructDeclaration(
        struct.name,
        mojom_pack.PackedStruct(struct),
        self.GetTemplate("params_definition"),
        {'METHOD_NAME': method_name, 'METHOD_ID': next_id})
    return params_def, next_id + 1

  def GetStructDefinitions(self):
    template = self.GetTemplate("struct_definition")
    return '\n'.join(map(
        lambda s: template.substitute(
            CLASS = s.name, NUM_FIELDS = len(s.fields)),
            self.module.structs));

  def GetInterfaceDefinition(self, interface):
    cases = []
    implementations = Lines(self.GetTemplate("proxy_implementation"))

    for method in interface.methods:
      cases.append(self.GetCaseLine(interface, method))
      sets = []
      computes = Lines(self.param_struct_compute_template)
      for param in method.parameters:
        if IsPointerKind(param.kind):
          sets.append(
              self.param_struct_set_template.substitute(NAME=param.name))
          computes.Add(NAME=param.name)
        else:
          sets.append(self.param_set_template.substitute(NAME=param.name))
      params_list = map(
          lambda param: "%s %s" % (self.GetConstType(param.kind), param.name),
          method.parameters)
      name = "k%s_%s_Name" % (interface.name, method.name)
      params_name = "%s_%s_Params" % (interface.name, method.name)

      implementations.Add(
          CLASS = interface.name,
          METHOD = method.name,
          NAME = name,
          PARAMS = params_name,
          PARAMS_LIST = ', '.join(params_list),
          COMPUTES = computes,
          SETS = '\n'.join(sets))
    stub_definition = self.GetTemplate("interface_stub_definition").substitute(
        CLASS = interface.name,
        CASES = '\n'.join(cases))
    return self.GetTemplate("interface_definition").substitute(
        CLASS = interface.name + "Proxy",
        PROXY_DEFINITIONS = implementations,
        STUB_DEFINITION = stub_definition)

  def GetInterfaceDefinitions(self):
    return '\n'.join(
        map(lambda i: self.GetInterfaceDefinition(i), self.module.interfaces))

  def GetStructSerializationDefinition(self, struct):
    ps = mojom_pack.PackedStruct(struct)
    param_name = CamelToUnderscores(struct.name)

    clones = Lines(self.struct_serialization_clone_template)
    sizes = "  return sizeof(*%s)" % param_name
    fields = self.GetSerializedFields(ps)
    for field in fields:
      substitutions = {'NAME': param_name, 'FIELD': field.name}
      sizes += \
          self.struct_serialization_compute_template.substitute(substitutions)
      clones.Add(substitutions)
    sizes += ";"
    serialization = self.GetStructSerialization(
      struct.name, param_name, ps, self.GetTemplate("struct_serialization"))
    return self.GetTemplate("struct_serialization_definition").substitute(
        NAME = param_name,
        CLASS = "%s::%s" % (self.module.namespace, struct.name),
        SIZES = sizes,
        CLONES = clones,
        SERIALIZATION = serialization)

  def GetStructSerializationDefinitions(self):
    return '\n'.join(
        map(lambda i: self.GetStructSerializationDefinition(i),
            self.module.structs))

  def GetInterfaceSerializationDefinitions(self):
    serializations = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        struct = GetStructFromMethod(interface, method)
        ps = mojom_pack.PackedStruct(struct)
        serializations.append(self.GetStructSerialization(
            struct.name,
            "params",
            ps,
            self.GetTemplate("params_serialization"),
            2))
    return '\n'.join(serializations)

  def GetParamsDefinitions(self):
    params_defs = []
    for interface in self.module.interfaces:
      next_id = 0
      for method in interface.methods:
        (params_def, next_id) = \
            self.GetParamsDefinition(interface, method, next_id)
        params_defs.append(params_def)
    return '\n'.join(params_defs)

  def GenerateModuleSource(self):
    self.WriteTemplateToFile("module.cc",
        HEADER = self.GetHeaderFile(self.module.name),
        INTERNAL_HEADER = self.GetHeaderFile(self.module.name, "internal"),
        PARAM_DEFINITIONS = self.GetParamsDefinitions(),
        STRUCT_DEFINITIONS = self.GetStructDefinitions(),
        INTERFACE_DEFINITIONS = self.GetInterfaceDefinitions(),
        STRUCT_SERIALIZATION_DEFINITIONS =
            self.GetStructSerializationDefinitions(),
        INTERFACE_SERIALIZATION_DEFINITIONS =
            self.GetInterfaceSerializationDefinitions())

  def GenerateModuleInternalHeader(self):
    traits = map(
      lambda s: self.GetTemplate("struct_serialization_traits").substitute(
          NAME = CamelToUnderscores(s.name),
          FULL_CLASS = "%s::%s" % (self.module.namespace, s.name)),
      self.module.structs);
    self.WriteTemplateToFile("module_internal.h",
        HEADER_GUARD = self.GetHeaderGuard(self.module.name + "_INTERNAL"),
        HEADER = self.GetHeaderFile(self.module.name),
        TRAITS = '\n'.join(traits))

  def GenerateFiles(self):
    self.GenerateModuleHeader()
    self.GenerateModuleInternalHeader()
    self.GenerateModuleSource()
