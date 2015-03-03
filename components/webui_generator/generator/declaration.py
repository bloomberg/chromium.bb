# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import json
import os.path
import re

import util

# Schema is described as follows:
# * for simple type:
#     %simple_type%
# * for list:
#     (list, %items_schema%)
# * for dict:
#     (dict, { '%key_name%': (%is_key_required%, %value_schema%),
#            ...
#            })
DECLARATION_SCHEMA = (dict, {
  'imports': (False, (list, unicode)),
  'type': (True, unicode),
  'children': (False, (list, (dict, {
                'id': (True, unicode),
                'type': (True, unicode),
                'comment': (False, unicode)
              }))),
  'context': (False, (list, (dict, {
               'name': (True, unicode),
               'type': (False, unicode),
               'default': (False, object),
               'comment': (False, unicode)
             }))),
  'events': (False, (list, (dict, {
              'name': (True, unicode),
              'comment': (False, unicode)
            }))),
  'strings': (False, (list, (dict, {
               'name': (True, unicode),
               'comment': (False, unicode)
             }))),
  'comment': (False, unicode)
})

# Returns (True,) if |obj| matches |schema|.
# Otherwise returns (False, msg), where |msg| is a diagnostic message.
def MatchSchema(obj, schema):
  expected_type = schema[0] if isinstance(schema, tuple) else schema
  if not isinstance(obj, expected_type):
    return (False, 'Wrong type, expected %s, got %s.' %
                       (expected_type, type(obj)))
  if expected_type == dict:
    obj_keys = set(obj.iterkeys())
    allowed_keys = set(schema[1].iterkeys())
    required_keys = set(kv[0] for kv in schema[1].iteritems() if kv[1][0])
    if not obj_keys.issuperset(required_keys):
      missing_keys = required_keys.difference(obj_keys)
      return (False, 'Missing required key%s %s.' %
                ('s' if len(missing_keys) > 1 else '',
                 ', '.join('\'%s\'' % k for k in missing_keys)))
    if not obj_keys.issubset(allowed_keys):
      unknown_keys = obj_keys.difference(allowed_keys)
      return (False, 'Unknown key%s %s.' %
                ('s' if len(unknown_keys) > 1 else '',
                 ', '.join('\'%s\'' % k for k in unknown_keys)))
    for key in obj:
      match = MatchSchema(obj[key], schema[1][key][1])
      if not match[0]:
        return (False, ('[\'%s\'] ' % key) + match[1])
  elif expected_type == list:
    for i, item in enumerate(obj):
      match = MatchSchema(item, schema[1])
      if not match[0]:
        return (False, ('[%d] ' % i) + match[1])
  return (True,)

def CheckDeclarationIsValid(declaration):
  match = MatchSchema(declaration, DECLARATION_SCHEMA)
  if not match[0]:
    raise Exception('Declaration is not valid: ' + match[1])

def CheckFieldIsUnique(list_of_dicts, field):
  seen = {}
  for i, d in enumerate(list_of_dicts):
    value = d[field]
    if value in seen:
      raise Exception(
          '[%d] Object with "%s" equal to "%s" already exists. See [%d].' %
              (i, field, value, seen[value]))
    else:
      seen[value] = i

class FieldDeclaration(object):
  ALLOWED_TYPES = [
    'boolean',
    'integer',
    'double',
    'string',
    'string_list'
  ]

  DEFAULTS = {
    'boolean': False,
    'integer': 0,
    'double': 0.0,
    'string': u'',
    'string_list': []
  }

  def __init__(self, declaration):
    self.name = declaration['name']
    self.id = util.ToLowerCamelCase(self.name)
    self.type = declaration['type'] if 'type' in declaration else 'string'
    if self.type not in self.ALLOWED_TYPES:
      raise Exception('Unknown type of context field "%s": "%s"' %
                      (self.name, self.type))
    self.default_value = declaration['default'] if 'default' in declaration \
            else self.DEFAULTS[self.type]
    if type(self.default_value) != type(self.DEFAULTS[self.type]):
      raise Exception('Wrong type of default for field "%s": '
                      'expected "%s", got "%s"' %
                          (self.name,
                           type(self.DEFAULTS[self.type]),
                           type(self.default_value)))

class Declaration(object):
  class InvalidDeclaration(Exception):
    def __init__(self, path, message):
      super(Exception, self).__init__(
          'Invalid declaration file "%s": %s' % (path, message))

  class DeclarationsStorage(object):
    def __init__(self):
      self.by_path = {}
      self.by_type = {}

    def Add(self, declaration):
      assert declaration.path not in self.by_path
      self.by_path[declaration.path] = declaration
      if declaration.type in self.by_type:
        raise Exception(
            'Redefinition of type "%s". ' \
            'Previous definition was in "%s".' % \
                (declaration.type, self.by_type[declaration.type].path))
      self.by_type[declaration.type] = declaration

    def HasPath(self, path):
      return path in self.by_path

    def HasType(self, type):
      return type in self.by_type

    def GetByPath(self, path):
      return self.by_path[path]

    def GetByType(self, type):
      return self.by_type[type]

    def GetKnownPathes(self):
      return self.by_path.keys()


  def __init__(self, path, known_declarations=None):
    if known_declarations is None:
      known_declarations = Declaration.DeclarationsStorage()
    self.path = path
    try:
      self.data = json.load(open(path, 'r'))
      CheckDeclarationIsValid(self.data)
      filename_wo_ext = os.path.splitext(os.path.basename(self.path))[0]
      if self.data['type'] != filename_wo_ext:
        raise Exception(
          'File with declaration of type "%s" should be named "%s.json"' %
              (self.data['type'], filename_wo_ext))

      known_declarations.Add(self)
      if 'imports' in self.data:
        for import_path in self.data['imports']:
          #TODO(dzhioev): detect circular dependencies.
          if not known_declarations.HasPath(import_path):
            Declaration(import_path, known_declarations)

      for key in ['children', 'strings', 'context', 'events']:
        if key in self.data:
          unique_field = 'id' if key == 'children' else 'name'
          try:
            CheckFieldIsUnique(self.data[key], unique_field)
          except Exception as e:
            raise Exception('["%s"] %s' % (key, e.message))
        else:
          self.data[key] = []

      self.children = {}
      for child_data in self.data['children']:
        child_type = child_data['type']
        child_id = child_data['id']
        if not known_declarations.HasType(child_type):
          raise Exception('Unknown type "%s" for child "%s"' %
                              (child_type, child_id))
        self.children[child_id] = known_declarations.GetByType(child_type)

      self.fields = [FieldDeclaration(d) for d in self.data['context']]
      fields_names = [field.name for field in self.fields]
      if len(fields_names) > len(set(fields_names)):
        raise Exception('Duplicate fields in declaration.')
      self.known_declarations = known_declarations
    except Declaration.InvalidDeclaration:
      raise
    except Exception as e:
      raise Declaration.InvalidDeclaration(self.path, e.message)

  @property
  def type(self):
    return self.data['type']

  @property
  def strings(self):
    return (string['name'] for string in self.data['strings'])

  @property
  def events(self):
    return (event['name'] for event in self.data['events'])

  @property
  def webui_view_basename(self):
    return self.type + '_web_ui_view'

  @property
  def webui_view_h_name(self):
    return self.webui_view_basename + '.h'

  @property
  def webui_view_cc_name(self):
    return self.webui_view_basename + '.cc'

  @property
  def webui_view_include_path(self):
    return os.path.join(os.path.dirname(self.path), self.webui_view_h_name)

  @property
  def export_h_name(self):
    return self.type + '_export.h'

  @property
  def component_export_macro(self):
    return "WUG_" + self.type.upper() + "_EXPORT"

  @property
  def component_impl_macro(self):
    return "WUG_" + self.type.upper() + "_IMPLEMENTATION"

  @property
  def export_h_include_path(self):
    return os.path.join(os.path.dirname(self.path), self.export_h_name)

  @property
  def view_model_basename(self):
    return self.type + '_view_model'

  @property
  def view_model_h_name(self):
    return self.view_model_basename + '.h'

  @property
  def view_model_cc_name(self):
    return self.view_model_basename + '.cc'

  @property
  def view_model_include_path(self):
    return os.path.join(os.path.dirname(self.path), self.view_model_h_name)

  @property
  def html_view_basename(self):
    return '%s-view' % self.type.replace('_', '-')

  @property
  def html_view_html_name(self):
    return self.html_view_basename + '.html'

  @property
  def html_view_js_name(self):
    return self.html_view_basename + '.js'

  @property
  def html_view_html_include_path(self):
    return os.path.join(os.path.dirname(self.path), self.html_view_html_name)

  @property
  def html_view_js_include_path(self):
    return os.path.join(os.path.dirname(self.path), self.html_view_js_name)

  @property
  def build_target(self):
    return self.type + "_wug_generated"

  @property
  def webui_view_class(self):
    return util.ToUpperCamelCase(self.type) + 'WebUIView'

  @property
  def view_model_class(self):
    return util.ToUpperCamelCase(self.type) + 'ViewModel'

  @property
  def imports(self):
    res = set(self.known_declarations.GetKnownPathes())
    res.remove(self.path)
    return res



