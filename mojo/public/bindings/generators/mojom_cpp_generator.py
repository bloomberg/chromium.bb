# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source files from a mojom.Module."""

import datetime
import mojom
import mojom_generator
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


class CPPGenerator(mojom_generator.Generator):

  struct_serialization_compute_template = \
    Template(" +\n      mojo::internal::ComputeSizeOf($NAME->$FIELD())")
  struct_serialization_clone_template = Template(
      "  clone->set_$FIELD(mojo::internal::Clone($NAME->$FIELD(), buf));")
  struct_serialization_handle_release_template = Template(
      "  (void) $NAME->$FIELD();")
  struct_serialization_encode_template = Template(
      "  Encode(&$NAME->${FIELD}_, handles);")
  struct_serialization_encode_handle_template = Template(
      "  EncodeHandle(&$NAME->${FIELD}_, handles);")
  struct_serialization_decode_template = Template(
      "  if (!Decode(&$NAME->${FIELD}_, message))\n"
      "    return false;")
  struct_serialization_decode_handle_template = Template(
      "  if (!DecodeHandle(&$NAME->${FIELD}_, &message->handles))\n"
      "    return false;")
  struct_destructor_body_template = Template(
      "mojo::MakeScopedHandle(data->$FIELD());")
  close_handles_template = Template(
      "  mojo::internal::CloseHandles($NAME->${FIELD}_.ptr);")
  param_set_template = Template(
      "  params->set_$NAME($NAME);")
  param_handle_set_template = Template(
      "  params->set_$NAME($NAME.release());")
  param_struct_set_template = Template(
      "  params->set_$NAME(\n"
      "      mojo::internal::Clone(mojo::internal::Unwrap($NAME),\n"
      "                            builder.buffer()));")
  param_struct_compute_template = Template(
      "  payload_size += mojo::internal::ComputeSizeOf(\n"
      "      mojo::internal::Unwrap($NAME));")
  field_template = Template("  $TYPE ${FIELD}_;")
  bool_field_template = Template("  uint8_t ${FIELD}_ : 1;")
  handle_field_template = Template("  mutable $TYPE ${FIELD}_;")
  setter_template = \
      Template("  void set_$FIELD($TYPE $FIELD) { ${FIELD}_ = $FIELD; }")
  ptr_setter_template = \
      Template("  void set_$FIELD($TYPE $FIELD) { ${FIELD}_.ptr = $FIELD; }")
  getter_template = \
      Template("  $TYPE $FIELD() const { return ${FIELD}_; }")
  ptr_getter_template = \
      Template("  const $TYPE $FIELD() const { return ${FIELD}_.ptr; }")
  handle_getter_template = \
      Template("  $TYPE $FIELD() const { " \
               "return mojo::internal::FetchAndReset(&${FIELD}_); }")
  wrapper_setter_template = \
      Template("    void set_$FIELD($TYPE $FIELD) { " \
               "data_->set_$FIELD($FIELD); }")
  wrapper_obj_setter_template = \
      Template("    void set_$FIELD($TYPE $FIELD) { " \
               "data_->set_$FIELD(mojo::internal::Unwrap($FIELD)); }")
  wrapper_handle_setter_template = \
      Template("    void set_$FIELD($TYPE $FIELD) { " \
               "data_->set_$FIELD($FIELD.release()); }")
  wrapper_getter_template = \
      Template("  $TYPE $FIELD() const { return data_->$FIELD(); }")
  wrapper_obj_getter_template = \
      Template("  const $TYPE $FIELD() const { " \
               "return mojo::internal::Wrap(data_->$FIELD()); }")
  wrapper_handle_getter_template = \
      Template("  $TYPE $FIELD() const { " \
               "return mojo::MakeScopedHandle(data_->$FIELD()); }")
  pad_template = Template("  uint8_t _pad${COUNT}_[$PAD];")
  templates  = {}
  HEADER_SIZE = 8

  kind_to_type = {
    mojom.BOOL:    "bool",
    mojom.INT8:    "int8_t",
    mojom.UINT8:   "uint8_t",
    mojom.INT16:   "int16_t",
    mojom.UINT16:  "uint16_t",
    mojom.INT32:   "int32_t",
    mojom.UINT32:  "uint32_t",
    mojom.FLOAT:   "float",
    mojom.HANDLE:  "mojo::Handle",
    mojom.MSGPIPE: "mojo::MessagePipeHandle",
    mojom.INT64:   "int64_t",
    mojom.UINT64:  "uint64_t",
    mojom.DOUBLE:  "double",
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
      return "%s_Data*" % kind.name
    if isinstance(kind, mojom.Array):
      return "mojo::internal::Array_Data<%s>*" % cls.GetType(kind.kind)
    if kind.spec == 's':
      return "mojo::internal::String_Data*"
    return cls.kind_to_type[kind]

  @classmethod
  def GetConstType(cls, kind):
    if isinstance(kind, mojom.Struct):
      return "const %s_Data*" % kind.name
    if isinstance(kind, mojom.Array):
      return "const mojo::internal::Array_Data<%s>*" % \
          cls.GetConstType(kind.kind)
    if kind.spec == 's':
      return "const mojo::internal::String_Data*"
    return cls.kind_to_type[kind]

  @classmethod
  def GetGetterLine(cls, field):
    subs = {'FIELD': field.name, 'TYPE': cls.GetType(field.kind)}
    if mojom_generator.IsObjectKind(field.kind):
      return cls.ptr_getter_template.substitute(subs)
    elif mojom_generator.IsHandleKind(field.kind):
      return cls.handle_getter_template.substitute(subs)
    else:
      return cls.getter_template.substitute(subs)

  @classmethod
  def GetSetterLine(cls, field):
    subs = {'FIELD': field.name, 'TYPE': cls.GetType(field.kind)}
    if mojom_generator.IsObjectKind(field.kind):
      return cls.ptr_setter_template.substitute(subs)
    else:
      return cls.setter_template.substitute(subs)

  @classmethod
  def GetWrapperType(cls, kind):
    if isinstance(kind, mojom.Struct):
      return "%s" % kind.name
    if isinstance(kind, mojom.Array):
      return "mojo::Array<%s >" % cls.GetWrapperType(kind.kind)
    if kind.spec == 's':
      return "mojo::String"
    if kind.spec == 'h':
      return "mojo::ScopedHandle"
    if kind.spec == 'h:m':
      return "mojo::ScopedMessagePipeHandle"
    return cls.kind_to_type[kind]

  @classmethod
  def GetConstWrapperType(cls, kind):
    if isinstance(kind, mojom.Struct):
      return "const %s&" % kind.name
    if isinstance(kind, mojom.Array):
      return "const mojo::Array<%s >&" % cls.GetWrapperType(kind.kind)
    if kind.spec == 's':
      return "const mojo::String&"
    if kind.spec == 'h':
      return "mojo::ScopedHandle"
    if kind.spec == 'h:m':
      return "mojo::ScopedMessagePipeHandle"
    return cls.kind_to_type[kind]

  @classmethod
  def GetWrapperGetterLine(cls, field):
    subs = {'FIELD': field.name, 'TYPE': cls.GetWrapperType(field.kind)}
    if mojom_generator.IsObjectKind(field.kind):
      return cls.wrapper_obj_getter_template.substitute(subs)
    elif mojom_generator.IsHandleKind(field.kind):
      return cls.wrapper_handle_getter_template.substitute(subs)
    else:
      return cls.wrapper_getter_template.substitute(subs)

  @classmethod
  def GetWrapperSetterLine(cls, field):
    subs = {'FIELD': field.name, 'TYPE': cls.GetConstWrapperType(field.kind)}
    if mojom_generator.IsObjectKind(field.kind):
      return cls.wrapper_obj_setter_template.substitute(subs)
    elif mojom_generator.IsHandleKind(field.kind):
      return cls.wrapper_handle_setter_template.substitute(subs)
    else:
      return cls.wrapper_setter_template.substitute(subs)

  @classmethod
  def GetFieldLine(cls, field):
    kind = field.kind
    if kind.spec == 'b':
      return cls.bool_field_template.substitute(FIELD=field.name)
    if mojom_generator.IsHandleKind(kind):
      return cls.handle_field_template.substitute(FIELD=field.name,
                                                  TYPE=cls.kind_to_type[kind])
    itype = None
    if isinstance(kind, mojom.Struct):
      itype = "mojo::internal::StructPointer<%s_Data>" % kind.name
    elif isinstance(kind, mojom.Array):
      itype = "mojo::internal::ArrayPointer<%s>" % cls.GetType(kind.kind)
    elif kind.spec == 's':
      itype = "mojo::internal::StringPointer"
    else:
      itype = cls.kind_to_type[kind]
    return cls.field_template.substitute(FIELD=field.name, TYPE=itype)

  @classmethod
  def GetParamLine(cls, name, kind):
    line = None
    if mojom_generator.IsObjectKind(kind):
      line = "mojo::internal::Wrap(params->%s())" % (name)
    elif mojom_generator.IsHandleKind(kind):
      line = "mojo::MakeScopedHandle(params->%s())" % (name)
    else:
      line = "params->%s()" % name
    return line

  @classmethod
  def GetCaseLine(cls, interface, method):
    params = map(
        lambda param: cls.GetParamLine(
            param.name,
            param.kind),
        method.parameters)
    method_call = "%s(%s);" % (method.name, ", ".join(params))

    return cls.GetTemplate("interface_stub_case").substitute(
        CLASS = interface.name,
        METHOD = method.name,
        METHOD_CALL = method_call);

  @classmethod
  def GetSerializedFields(cls, ps):
    fields = []
    for pf in ps.packed_fields:
      if mojom_generator.IsObjectKind(pf.field.kind):
        fields.append(pf.field)
    return fields

  @classmethod
  def GetHandleFields(cls, ps):
    fields = []
    for pf in ps.packed_fields:
      if mojom_generator.IsHandleKind(pf.field.kind):
        fields.append(pf.field)
    return fields

  @classmethod
  def IsStructWithHandles(cls, struct):
    for field in struct.fields:
      if mojom_generator.IsHandleKind(field.kind):
        return True
    return False

  @classmethod
  def GetStructsWithHandles(cls, structs):
    result = []
    for struct in structs:
      if cls.IsStructWithHandles(struct):
        result.append(struct)
    return result

  def GetHeaderGuard(self, name):
    return "MOJO_GENERATED_BINDINGS_%s_%s_H_" % \
        (self.module.name.upper(), name.upper())

  def GetHeaderFile(self, *components):
    components = map(mojom_generator.CamelToUnderscores, components)
    component_string = '_'.join(components)
    return os.path.join(self.header_dir, "%s.h" % component_string)

  def WriteTemplateToFile(self, template_name, **substitutions):
    template = self.GetTemplate(template_name)
    filename = template_name.replace("module", self.module.name)
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

    subs.update(
        CLASS = name + '_Data',
        WRAPPER = name,
        SETTERS = '\n'.join(setters),
        GETTERS = '\n'.join(getters),
        FIELDS = '\n'.join(fields),
        SIZE = ps.GetTotalSize() + self.HEADER_SIZE)
    return template.substitute(subs)

  def GetStructSerialization(
      self, class_name, param_name, ps, template, indent = None):
    struct = ps.struct
    closes = Lines(self.close_handles_template, indent)
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
      closes.Add(substitutions)
    for field in handle_fields:
      substitutions = {'NAME': param_name, 'FIELD': field.name}
      encode_handles.Add(substitutions)
      decode_handles.Add(substitutions)
    return template.substitute(
        CLASS = \
            "%s::internal::%s" % (self.module.namespace, class_name + '_Data'),
        NAME = param_name,
        CLOSES = closes,
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

  def GetWrapperDeclaration(self, name, ps, template, subs = {}):
    setters = []
    getters = []
    num_fields = len(ps.packed_fields)
    for i in xrange(num_fields):
      field = ps.packed_fields[i].field
      setters.append(self.GetWrapperSetterLine(field))
      getters.append(self.GetWrapperGetterLine(field))
    subs.update(
        CLASS = name,
        SETTERS = '\n'.join(setters),
        GETTERS = '\n'.join(getters))
    return template.substitute(subs)

  def GetWrapperClassDeclarations(self):
    wrapper_decls = map(
        lambda s: self.GetWrapperDeclaration(
            s.name,
            mojom_pack.PackedStruct(s),
            self.GetTemplate("wrapper_class_declaration"),
            {}),
        self.module.structs)
    return '\n'.join(wrapper_decls)

  def GetWrapperForwardDeclarations(self):
    wrapper_fwds = map(lambda s: "class " + s.name + ";", self.module.structs)
    return '\n'.join(wrapper_fwds)

  def GetInterfaceClassDeclaration(self, interface, template, method_postfix):
    methods = []
    for method in interface.methods:
      params = []
      for param in method.parameters:
        params.append("%s %s" %
                          (self.GetConstWrapperType(param.kind), param.name))
      methods.append(
          "  virtual void %s(%s) %s;" %
              (method.name, ", ".join(params), method_postfix))
    return template.substitute(
        CLASS=interface.name,
        PEER=interface.peer,
        METHODS='\n'.join(methods))

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
        INTERNAL_HEADER = self.GetHeaderFile(self.module.name, "internal"),
        WRAPPER_CLASS_DECLARATIONS = self.GetWrapperClassDeclarations(),
        INTERFACE_CLASS_DECLARATIONS = self.GetInterfaceClassDeclarations(),
        INTERFACE_PROXY_DECLARATIONS = self.GetInterfaceProxyDeclarations(),
        INTERFACE_STUB_DECLARATIONS = self.GetInterfaceStubDeclarations())

  def GetParamsDefinition(self, interface, method):
    struct = mojom_generator.GetStructFromMethod(interface, method)
    method_name = "k%s_%s_Name" % (interface.name, method.name)
    assert method.ordinal is not None
    params_def = self.GetStructDeclaration(
        struct.name,
        mojom_pack.PackedStruct(struct),
        self.GetTemplate("params_definition"),
        {'METHOD_NAME': method_name, 'METHOD_ID': method.ordinal})
    return params_def

  def GetStructDefinitions(self):
    template = self.GetTemplate("struct_definition")
    return '\n'.join(map(
        lambda s: template.substitute(
            CLASS = s.name + '_Data',
            NUM_FIELDS = len(s.fields)),
            self.module.structs));

  def GetStructDestructorBody(self, struct):
    body = Lines(self.struct_destructor_body_template)
    for field in struct.fields:
      if mojom_generator.IsHandleKind(field.kind):
        body.Add(FIELD=field.name)
    return body

  def GetStructDestructors(self):
    template = self.GetTemplate("struct_destructor")
    return '\n'.join(map(
        lambda s: template.substitute(
            CLASS = s.name,
            BODY = self.GetStructDestructorBody(s)),
            self.GetStructsWithHandles(self.module.structs)));

  def GetStructDestructorAddress(self, struct):
    if self.IsStructWithHandles(struct):
      return '&internal::' + struct.name + '_Data_Destructor';
    return 'NULL'

  def GetStructBuilderDefinitions(self):
    template = self.GetTemplate("struct_builder_definition")
    dtor = 'NULL'
    return '\n'.join(map(
        lambda s: template.substitute(
            CLASS = s.name,
            DTOR = self.GetStructDestructorAddress(s)),
            self.module.structs));

  def GetInterfaceDefinition(self, interface):
    cases = []
    implementations = Lines(self.GetTemplate("proxy_implementation"))

    for method in interface.methods:
      cases.append(self.GetCaseLine(interface, method))
      sets = []
      computes = Lines(self.param_struct_compute_template)
      for param in method.parameters:
        if mojom_generator.IsObjectKind(param.kind):
          sets.append(
              self.param_struct_set_template.substitute(NAME=param.name))
          computes.Add(NAME=param.name)
        elif mojom_generator.IsHandleKind(param.kind):
          sets.append(
              self.param_handle_set_template.substitute(NAME=param.name))
        else:
          sets.append(self.param_set_template.substitute(NAME=param.name))
      params_list = map(
          lambda param: "%s %s" %
                            (self.GetConstWrapperType(param.kind), param.name),
          method.parameters)
      name = "internal::k%s_%s_Name" % (interface.name, method.name)
      params_name = \
          "internal::%s_%s_Params_Data" % (interface.name, method.name)

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
    param_name = mojom_generator.CamelToUnderscores(struct.name)

    dtor = ''
    closes = Lines(self.close_handles_template)
    clones = Lines(self.struct_serialization_clone_template)
    handle_releases = Lines(self.struct_serialization_handle_release_template)
    sizes = "  return sizeof(*%s)" % param_name
    fields = self.GetSerializedFields(ps)
    for field in fields:
      substitutions = {'NAME': param_name, 'FIELD': field.name}
      sizes += \
          self.struct_serialization_compute_template.substitute(substitutions)
      clones.Add(substitutions)
      closes.Add(substitutions)
    handle_fields = self.GetHandleFields(ps)
    for field in handle_fields:
      substitutions = {'NAME': param_name, 'FIELD': field.name}
      handle_releases.Add(substitutions)
    if len(handle_fields) > 0:
      dtor = "  %s_Data_Destructor(%s);" % (struct.name, param_name)
    sizes += ";"
    serialization = self.GetStructSerialization(
      struct.name, param_name, ps, self.GetTemplate("struct_serialization"))
    return self.GetTemplate("struct_serialization_definition").substitute(
        NAME = param_name,
        CLASS = "%s::internal::%s_Data" % (self.module.namespace, struct.name),
        SIZES = sizes,
        CLONES = clones,
        HANDLE_RELEASES = handle_releases,
        CLOSES = closes,
        DTOR = dtor,
        SERIALIZATION = serialization)

  def GetStructSerializationDefinitions(self):
    return '\n'.join(
        map(lambda i: self.GetStructSerializationDefinition(i),
            self.module.structs))

  def GetInterfaceSerializationDefinitions(self):
    serializations = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        struct = mojom_generator.GetStructFromMethod(interface, method)
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
      for method in interface.methods:
        params_def = self.GetParamsDefinition(interface, method)
        params_defs.append(params_def)
    return '\n'.join(params_defs)

  def GenerateModuleSource(self):
    self.WriteTemplateToFile("module.cc",
        HEADER = self.GetHeaderFile(self.module.name),
        PARAM_DEFINITIONS = self.GetParamsDefinitions(),
        STRUCT_DEFINITIONS = self.GetStructDefinitions(),
        STRUCT_DESTRUCTORS = self.GetStructDestructors(),
        STRUCT_BUILDER_DEFINITIONS = self.GetStructBuilderDefinitions(),
        INTERFACE_DEFINITIONS = self.GetInterfaceDefinitions(),
        STRUCT_SERIALIZATION_DEFINITIONS =
            self.GetStructSerializationDefinitions(),
        INTERFACE_SERIALIZATION_DEFINITIONS =
            self.GetInterfaceSerializationDefinitions())

  def GenerateModuleInternalHeader(self):
    traits = map(
      lambda s: self.GetTemplate("struct_serialization_traits").substitute(
          NAME = mojom_generator.CamelToUnderscores(s.name),
          FULL_CLASS = \
              "%s::internal::%s" % (self.module.namespace, s.name + '_Data')),
      self.module.structs);
    self.WriteTemplateToFile("module_internal.h",
        HEADER_GUARD = self.GetHeaderGuard(self.module.name + "_INTERNAL"),
        WRAPPER_FORWARD_DECLARATIONS = self.GetWrapperForwardDeclarations(),
        STRUCT_CLASS_DECLARATIONS = self.GetStructClassDeclarations(),
        TRAITS = '\n'.join(traits))

  def GenerateFiles(self):
    self.GenerateModuleHeader()
    self.GenerateModuleInternalHeader()
    self.GenerateModuleSource()
