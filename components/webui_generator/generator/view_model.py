# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import datetime
import util

H_FILE_TEMPLATE = \
"""// Copyright %(year)d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: this file is generated from "%(source)s". Do not modify directly.

#ifndef %(include_guard)s
#define %(include_guard)s

#include "base/macros.h"
#include "components/webui_generator/view_model.h"
#include "%(export_h_include_path)s"

namespace content {
class BrowserContext;
}

%(children_forward_declarations)s

namespace gen {

class %(export_macro)s %(class_name)s : public ::webui_generator::ViewModel {
 public:
  using FactoryFunction = %(class_name)s* (*)(content::BrowserContext*);

%(context_keys)s

  static %(class_name)s* Create(content::BrowserContext* context);
  static void SetFactory(FactoryFunction factory);

  %(class_name)s();

%(children_getters)s

%(context_getters)s

%(event_handlers)s

 void Initialize() override {}
 void OnAfterChildrenReady() override {}
 void OnViewBound() override {}

 private:
  void OnEvent(const std::string& event) final;

  static FactoryFunction factory_function_;
};

}  // namespace gen

#endif  // %(include_guard)s
"""

CHILD_FORWARD_DECLARATION_TEMPLATE = \
"""namespace gen {
  class %(child_type)s;
}
"""
CONTEXT_KEY_DECLARATION_TEMPLATE = \
"""  static const char %s[];"""

CHILD_GETTER_DECLARATION_TEMPLATE = \
"""  virtual gen::%(child_type)s* %(method_name)s() const;""";

CONTEXT_VALUE_GETTER_DECLARATION_TEMPLATE = \
"""  %(type)s %(method_name)s() const;"""

EVENT_HANDLER_DECLARATION_TEMPLATE = \
"""  virtual void %s() = 0;"""

CC_FILE_TEMPLATE = \
"""// Copyright %(year)d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: this file is generated from "%(source)s". Do not modify directly.

#include "%(header_path)s"

#include "base/logging.h"
#include "components/webui_generator/view.h"
%(children_includes)s

namespace gen {

%(context_keys)s

%(class_name)s::FactoryFunction %(class_name)s::factory_function_;

%(class_name)s* %(class_name)s::Create(content::BrowserContext* context) {
  CHECK(factory_function_) << "Factory function for %(class_name)s was not "
      "set.";
  return factory_function_(context);
}

void %(class_name)s::SetFactory(FactoryFunction factory) {
  factory_function_ = factory;
}

%(class_name)s::%(class_name)s() {
%(constructor_body)s
}

%(children_getters)s

%(context_getters)s

void %(class_name)s::OnEvent(const std::string& event) {
%(event_dispatcher_body)s
  LOG(ERROR) << "Unknown event '" << event << "'";
}

}  // namespace gen
"""

CONTEXT_KEY_DEFINITION_TEMPLATE = \
"""const char %(class_name)s::%(name)s[] = "%(value)s";"""

CHILD_GETTER_DEFINITION_TEMPLATE = \
"""gen::%(child_type)s* %(class_name)s::%(method_name)s() const {
  return static_cast<gen::%(child_type)s*>(
      view()->GetChild("%(child_id)s")->GetViewModel());
}
"""

CONTEXT_VALUE_GETTER_DEFINITION_TEMPLATE = \
"""%(type)s %(class_name)s::%(method_name)s() const {
    return context().%(context_getter)s(%(key_constant)s);
}
"""

FIELD_TYPE_TO_GETTER_TYPE = {
  'boolean': 'bool',
  'integer': 'int',
  'double': 'double',
  'string': 'std::string',
  'string_list': 'login::StringList'
}

DISPATCH_EVENT_TEMPLATE = \
"""  if (event == "%(event_id)s") {
    %(method_name)s();
    return;
  }"""

def GetCommonSubistitutions(declaration):
  subs = {}
  subs['year'] = datetime.date.today().year
  subs['class_name'] = declaration.view_model_class
  subs['source'] = declaration.path
  return subs

def FieldNameToConstantName(field_name):
  return 'kContextKey' + util.ToUpperCamelCase(field_name)

def GenContextKeysDeclarations(fields):
  return '\n'.join(
      (CONTEXT_KEY_DECLARATION_TEMPLATE % \
          FieldNameToConstantName(f.name) for f in fields))

def GenChildrenForwardDeclarations(children):
  lines = []
  for declaration in set(children.itervalues()):
    lines.append(CHILD_FORWARD_DECLARATION_TEMPLATE % {
                     'child_type': declaration.view_model_class
                 })
  return '\n'.join(lines)

def ChildIdToChildGetterName(id):
  return 'Get%s' % util.ToUpperCamelCase(id)

def GenChildrenGettersDeclarations(children):
  lines = []
  for id, declaration in children.iteritems():
    lines.append(CHILD_GETTER_DECLARATION_TEMPLATE % {
                     'child_type': declaration.view_model_class,
                     'method_name': ChildIdToChildGetterName(id)
                 })
  return '\n'.join(lines)

def FieldNameToGetterName(field_name):
  return 'Get%s' % util.ToUpperCamelCase(field_name)

def GenContextGettersDeclarations(context_fields):
  lines = []
  for field in context_fields:
    lines.append(CONTEXT_VALUE_GETTER_DECLARATION_TEMPLATE % {
                     'type': FIELD_TYPE_TO_GETTER_TYPE[field.type],
                     'method_name': FieldNameToGetterName(field.name)
                 })
  return '\n'.join(lines)

def EventIdToMethodName(event):
  return 'On' + util.ToUpperCamelCase(event)

def GenEventHandlersDeclarations(events):
  lines = []
  for event in events:
    lines.append(EVENT_HANDLER_DECLARATION_TEMPLATE %
                     EventIdToMethodName(event))
  return '\n'.join(lines)

def GenHFile(declaration):
  subs = GetCommonSubistitutions(declaration)
  subs['include_guard'] = \
      util.PathToIncludeGuard(declaration.view_model_include_path)
  subs['context_keys'] = GenContextKeysDeclarations(declaration.fields)
  subs['children_forward_declarations'] = \
      GenChildrenForwardDeclarations(declaration.children)
  subs['children_getters'] = \
      GenChildrenGettersDeclarations(declaration.children);
  subs['context_getters'] = \
      GenContextGettersDeclarations(declaration.fields);
  subs['event_handlers'] = GenEventHandlersDeclarations(declaration.events)
  subs['export_h_include_path'] = declaration.export_h_include_path
  subs['export_macro'] = declaration.component_export_macro
  return H_FILE_TEMPLATE % subs

def GenContextKeysDefinitions(declaration):
  lines = []
  for field in declaration.fields:
    definition = CONTEXT_KEY_DEFINITION_TEMPLATE % {
      'class_name': declaration.view_model_class,
      'name': FieldNameToConstantName(field.name),
      'value': field.id
    }
    lines.append(definition)
  return '\n'.join(lines)

def GenChildrenIncludes(children):
  lines = []
  for declaration in set(children.itervalues()):
    lines.append('#include "%s"' % declaration.view_model_include_path)
  return '\n'.join(lines)

def GenContextFieldInitialization(field):
  lines = []
  key_constant = FieldNameToConstantName(field.name)
  setter_method = 'Set' + util.ToUpperCamelCase(field.type)
  if field.type == 'string_list':
    lines.append('  {')
    lines.append('    std::vector<std::string> defaults;')
    for s in field.default_value:
      lines.append('    defaults.push_back("%s");' % s)
    lines.append('    context().%s(%s, defaults);' %
                     (setter_method, key_constant))
    lines.append('  }')
  else:
    setter = '  context().%s(%s, ' % (setter_method, key_constant)
    if field.type in ['integer', 'double']:
      setter += str(field.default_value)
    elif field.type == 'boolean':
      setter += 'true' if field.default_value else 'false'
    else:
      assert field.type == 'string'
      setter += '"%s"' % field.default_value
    setter += ");"
    lines.append(setter)
  return '\n'.join(lines)

def GenChildrenGettersDefenitions(declaration):
  lines = []
  for id, child in declaration.children.iteritems():
    lines.append(CHILD_GETTER_DEFINITION_TEMPLATE % {
                     'child_type': child.view_model_class,
                     'class_name': declaration.view_model_class,
                     'method_name': ChildIdToChildGetterName(id),
                     'child_id': id
                 });
  return '\n'.join(lines)

def GenContextGettersDefinitions(declaration):
  lines = []
  for field in declaration.fields:
    lines.append(CONTEXT_VALUE_GETTER_DEFINITION_TEMPLATE % {
                     'type': FIELD_TYPE_TO_GETTER_TYPE[field.type],
                     'class_name': declaration.view_model_class,
                     'method_name': FieldNameToGetterName(field.name),
                     'context_getter': 'Get' + util.ToUpperCamelCase(
                                                        field.type),
                     'key_constant': FieldNameToConstantName(field.name)
                 });
  return '\n'.join(lines)

def GenEventDispatcherBody(events):
  lines = []
  for event in events:
    lines.append(DISPATCH_EVENT_TEMPLATE % {
                     'event_id': util.ToLowerCamelCase(event),
                     'method_name': EventIdToMethodName(event)
                 });
  return '\n'.join(lines)

def GenCCFile(declaration):
  subs = GetCommonSubistitutions(declaration)
  subs['header_path'] = declaration.view_model_include_path
  subs['context_keys'] = GenContextKeysDefinitions(declaration)
  subs['children_includes'] = GenChildrenIncludes(declaration.children)
  initializations = [GenContextFieldInitialization(field) \
                         for field in declaration.fields]
  initializations.append('  base::DictionaryValue diff;');
  initializations.append('  context().GetChangesAndReset(&diff);');
  subs['constructor_body'] = '\n'.join(initializations)
  subs['children_getters'] = GenChildrenGettersDefenitions(declaration)
  subs['context_getters'] = GenContextGettersDefinitions(declaration)
  subs['event_dispatcher_body'] = GenEventDispatcherBody(declaration.events)
  return CC_FILE_TEMPLATE % subs

def ListOutputs(declaration, destination):
  dirname = os.path.join(destination, os.path.dirname(declaration.path))
  h_file_path = os.path.join(dirname, declaration.view_model_h_name)
  cc_file_path = os.path.join(dirname, declaration.view_model_cc_name)
  return [h_file_path, cc_file_path]

def Gen(declaration, destination):
  h_file_path, cc_file_path = ListOutputs(declaration, destination)
  util.CreateDirIfNotExists(os.path.dirname(h_file_path))
  open(h_file_path, 'w').write(GenHFile(declaration))
  open(cc_file_path, 'w').write(GenCCFile(declaration))

