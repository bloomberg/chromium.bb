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

class DependentKinds(set):
  """Set subclass to find the unique set of non POD types."""
  def AddKind(self, kind):
    if isinstance(kind, mojom.Struct):
      self.add(kind)
    if isinstance(kind, mojom.Array):
      self.AddKind(kind.kind)


class Forwards(object):
  """Helper class to maintain unique set of forward declarations."""
  def __init__(self):
    self.kinds = DependentKinds()

  def Add(self, kind):
    self.kinds.AddKind(kind)

  def __repr__(self):
    return '\n'.join(
        sorted(map(lambda kind: "class %s;" % kind.name, self.kinds)))


class Lines(object):
  """Helper class to maintain list of template expanded lines."""
  def __init__(self, template):
    self.template = template
    self.lines = []

  def Add(self, map = {}, **substitutions):
    if len(substitutions) > 0:
      map = map.copy()
      map.update(substitutions)

    self.lines.append(self.template.substitute(map))

  def __repr__(self):
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
  name_template = \
      Template("const uint32_t k${INTERFACE}_${METHOD}_Name = $NAME;")
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
    return os.path.join(
      self.header_dir,
      "%s_%s.h" % (CamelToUnderscores(self.module.name), component_string))

  # Pass |output_dir| to emit files to disk. Omit |output_dir| to echo all files
  # to stdout.
  def __init__(self, module, header_dir, output_dir=None):
    self.module = module
    self.header_dir = header_dir
    self.output_dir = output_dir

  def WriteTemplateToFile(self, template_name, name, **substitutions):
    template_name = CamelToUnderscores(template_name)
    name = CamelToUnderscores(name)
    template = self.GetTemplate(template_name)
    filename = "%s_%s" % (CamelToUnderscores(self.module.name), template_name)
    filename = filename.replace("interface", name)
    filename = filename.replace("struct", name)
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

  def GetStructDeclaration(self, name, ps):
    params_template = self.GetTemplate("struct_declaration")
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
    return params_template.substitute(
        CLASS = name,
        SETTERS = '\n'.join(setters),
        GETTERS = '\n'.join(getters),
        FIELDS = '\n'.join(fields),
        SIZE = size + self.HEADER_SIZE)

  def GetStructImplementation(self, name, ps):
    return self.GetTemplate("struct_implementation").substitute(
        CLASS = name,
        NUM_FIELDS = len(ps.packed_fields))

  def GetStructSerialization(self, class_name, param_name, ps):
    struct = ps.struct
    encodes = Lines(self.struct_serialization_encode_template)
    encode_handles = Lines(self.struct_serialization_encode_handle_template)
    decodes = Lines(self.struct_serialization_decode_template)
    decode_handles = Lines(self.struct_serialization_decode_handle_template)
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
    return self.GetTemplate("struct_serialization").substitute(
        CLASS = "%s::%s" % (self.module.namespace, class_name),
        NAME = param_name,
        ENCODES = encodes,
        DECODES = decodes,
        ENCODE_HANDLES = encode_handles,
        DECODE_HANDLES = decode_handles)

  def GenerateStructHeader(self, ps):
    struct = ps.struct
    forwards = Forwards()
    for field in struct.fields:
      forwards.Add(field.kind)

    self.WriteTemplateToFile("struct.h", struct.name,
        HEADER_GUARD = self.GetHeaderGuard(struct.name),
        CLASS = struct.name,
        FORWARDS = forwards,
        DECLARATION = self.GetStructDeclaration(struct.name, ps))

  def GenerateStructSource(self, ps):
    struct = ps.struct
    header = self.GetHeaderFile(struct.name)
    implementation = self.GetStructImplementation(struct.name, ps)
    self.WriteTemplateToFile("struct.cc", struct.name,
        CLASS = struct.name,
        NUM_FIELDS = len(struct.fields),
        HEADER = header,
        IMPLEMENTATION = implementation)

  def GenerateStructSerializationHeader(self, ps):
    struct = ps.struct
    self.WriteTemplateToFile("struct_serialization.h", struct.name,
        HEADER_GUARD = self.GetHeaderGuard(struct.name + "_SERIALIZATION"),
        CLASS = struct.name,
        NAME = CamelToUnderscores(struct.name),
        FULL_CLASS = "%s::%s" % \
            (self.module.namespace, struct.name))

  def GenerateStructSerializationSource(self, ps):
    struct = ps.struct
    serialization_header = self.GetHeaderFile(struct.name, "serialization")
    param_name = CamelToUnderscores(struct.name)

    kinds = DependentKinds()
    for field in struct.fields:
      kinds.AddKind(field.kind)
    headers = \
        map(lambda kind: self.GetHeaderFile(kind.name, "serialization"), kinds)
    headers.append(self.GetHeaderFile(struct.name))
    includes = map(lambda header: "#include \"%s\"" % header, sorted(headers))

    class_header = self.GetHeaderFile(struct.name)
    clones = Lines(self.struct_serialization_clone_template)
    sizes = "  return sizeof(*%s)" % param_name
    fields = self.GetSerializedFields(ps)
    for field in fields:
      substitutions = {'NAME': param_name, 'FIELD': field.name}
      sizes += \
          self.struct_serialization_compute_template.substitute(substitutions)
      clones.Add(substitutions)
    sizes += ";"
    serialization = self.GetStructSerialization(struct.name, param_name, ps)
    self.WriteTemplateToFile("struct_serialization.cc", struct.name,
        NAME = param_name,
        CLASS = "%s::%s" % (self.module.namespace, struct.name),
        SERIALIZATION_HEADER = serialization_header,
        INCLUDES = '\n'.join(includes),
        SIZES = sizes,
        CLONES = clones,
        SERIALIZATION = serialization)

  def GenerateInterfaceHeader(self, interface):
    methods = []
    forwards = Forwards()
    for method in interface.methods:
      params = []
      for param in method.parameters:
        forwards.Add(param.kind)
        params.append("%s %s" % (self.GetConstType(param.kind), param.name))
      methods.append(
          "  virtual void %s(%s) = 0;" % (method.name, ", ".join(params)))

    self.WriteTemplateToFile("interface.h", interface.name,
        HEADER_GUARD = self.GetHeaderGuard(interface.name),
        CLASS = interface.name,
        FORWARDS = forwards,
        METHODS = '\n'.join(methods))

  def GenerateInterfaceStubHeader(self, interface):
    header = self.GetHeaderFile(interface.name)
    self.WriteTemplateToFile("interface_stub.h", interface.name,
        HEADER_GUARD = self.GetHeaderGuard(interface.name + "_STUB"),
        CLASS = interface.name,
        HEADER = header)

  def GenerateInterfaceStubSource(self, interface):
    stub_header = self.GetHeaderFile(interface.name, "stub")
    serialization_header = self.GetHeaderFile(interface.name, "serialization")
    cases = []
    for method in interface.methods:
      cases.append(self.GetCaseLine(interface, method))
    self.WriteTemplateToFile("interface_stub.cc", interface.name,
        CLASS = interface.name,
        CASES = '\n'.join(cases),
        STUB_HEADER = stub_header,
        SERIALIZATION_HEADER = serialization_header)

  def GenerateInterfaceSerializationHeader(self, interface):
    kinds = DependentKinds()
    for method in interface.methods:
      for param in method.parameters:
        kinds.AddKind(param.kind)
    headers = \
        map(lambda kind: self.GetHeaderFile(kind.name, "serialization"), kinds)
    headers.append(self.GetHeaderFile(interface.name))
    headers.append("mojo/public/bindings/lib/bindings_serialization.h")
    includes = map(lambda header: "#include \"%s\"" % header, sorted(headers))

    names = []
    name = 1
    param_classes = []
    param_templates = []
    template_declaration = self.GetTemplate("template_declaration")
    for method in interface.methods:
      names.append(self.name_template.substitute(
        INTERFACE = interface.name,
        METHOD = method.name,
        NAME = name))
      name += 1

      struct = GetStructFromMethod(interface, method)
      ps = mojom_pack.PackedStruct(struct)
      param_classes.append(self.GetStructDeclaration(struct.name, ps))
      param_templates.append(template_declaration.substitute(CLASS=struct.name))

    self.WriteTemplateToFile("interface_serialization.h", interface.name,
        HEADER_GUARD = self.GetHeaderGuard(interface.name + "_SERIALIZATION"),
        INCLUDES = '\n'.join(includes),
        NAMES = '\n'.join(names),
        PARAM_CLASSES = '\n'.join(param_classes),
        PARAM_TEMPLATES = '\n'.join(param_templates))

  def GenerateInterfaceSerializationSource(self, interface):
    implementations = []
    serializations = []
    for method in interface.methods:
      struct = GetStructFromMethod(interface, method)
      ps = mojom_pack.PackedStruct(struct)
      implementations.append(self.GetStructImplementation(struct.name, ps))
      serializations.append(
          self.GetStructSerialization("internal::" + struct.name, "params", ps))
    self.WriteTemplateToFile("interface_serialization.cc", interface.name,
        HEADER = self.GetHeaderFile(interface.name, "serialization"),
        IMPLEMENTATIONS = '\n'.join(implementations),
        SERIALIZATIONS = '\n'.join(serializations))

  def GenerateInterfaceProxyHeader(self, interface):
    methods = []
    for method in interface.methods:
      params = map(
          lambda param: "%s %s" % (self.GetConstType(param.kind), param.name),
          method.parameters)
      methods.append(
          "  virtual void %s(%s) MOJO_OVERRIDE;" \
              % (method.name, ", ".join(params)))

    self.WriteTemplateToFile("interface_proxy.h", interface.name,
        HEADER_GUARD = self.GetHeaderGuard(interface.name + "_PROXY"),
        HEADER = self.GetHeaderFile(interface.name),
        CLASS = interface.name,
        METHODS = '\n'.join(methods))

  def GenerateInterfaceProxySource(self, interface):
    implementations = Lines(self.GetTemplate("proxy_implementation"))
    for method in interface.methods:
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
      name = "internal::k%s_%s_Name" % (interface.name, method.name)
      params_name = "internal::%s_%s_Params" % (interface.name, method.name)

      implementations.Add(
          CLASS = interface.name,
          METHOD = method.name,
          NAME = name,
          PARAMS = params_name,
          PARAMS_LIST = ', '.join(params_list),
          COMPUTES = computes,
          SETS = '\n'.join(sets))
    self.WriteTemplateToFile("interface_proxy.cc", interface.name,
        HEADER = self.GetHeaderFile(interface.name, "proxy"),
        SERIALIZATION_HEADER = \
          self.GetHeaderFile(interface.name, "serialization"),
        CLASS = interface.name,
        IMPLEMENTATIONS = implementations)

  def GenerateFiles(self):
    for struct in self.module.structs:
      ps = mojom_pack.PackedStruct(struct)
      self.GenerateStructHeader(ps)
      self.GenerateStructSource(ps)
      self.GenerateStructSerializationHeader(ps)
      self.GenerateStructSerializationSource(ps)
    for interface in self.module.interfaces:
      self.GenerateInterfaceHeader(interface)
      self.GenerateInterfaceStubHeader(interface)
      self.GenerateInterfaceStubSource(interface)
      self.GenerateInterfaceSerializationHeader(interface)
      self.GenerateInterfaceSerializationSource(interface)
      self.GenerateInterfaceProxyHeader(interface)
      self.GenerateInterfaceProxySource(interface)
