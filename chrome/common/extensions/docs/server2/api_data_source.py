# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import os

from docs_server_utils import GetLinkToRefType
import compiled_file_system as compiled_fs
from file_system import FileNotFoundError
import third_party.json_schema_compiler.json_comment_eater as json_comment_eater
import third_party.json_schema_compiler.model as model
import third_party.json_schema_compiler.idl_schema as idl_schema
import third_party.json_schema_compiler.idl_parser as idl_parser

# Increment this version when there are changes to the data stored in any of
# the caches used by APIDataSource. This allows the cache to be invalidated
# without having to flush memcache on the production server.
_VERSION = 2

def _RemoveNoDocs(item):
  if type(item) == dict:
    if item.get('nodoc', False):
      return True
    to_remove = []
    for key, value in item.items():
      if _RemoveNoDocs(value):
        to_remove.append(key)
    for k in to_remove:
      del item[k]
  elif type(item) == list:
    to_remove = []
    for i in item:
      if _RemoveNoDocs(i):
        to_remove.append(i)
    for i in to_remove:
      item.remove(i)
  return False

def _CreateId(node, prefix):
  if node.parent is not None and not isinstance(node.parent, model.Namespace):
    return '-'.join([prefix, node.parent.simple_name, node.simple_name])
  return '-'.join([prefix, node.simple_name])

def _FormatValue(value):
  """Inserts commas every three digits for integer values. It is magic.
  """
  s = str(value)
  return ','.join([s[max(0, i - 3):i] for i in range(len(s), 0, -3)][::-1])

class _JscModel(object):
  """Uses a Model from the JSON Schema Compiler and generates a dict that
  a Handlebar template can use for a data source.
  """
  def __init__(self, json):
    clean_json = copy.deepcopy(json)
    if _RemoveNoDocs(clean_json):
      self._namespace = None
    else:
      self._namespace = model.Namespace(clean_json, clean_json['namespace'])

  def _FormatDescription(self, description):
    if description is None or '$ref:' not in description:
      return description
    refs = description.split('$ref:')
    formatted_description = [refs[0]]
    for ref in refs[1:]:
      parts = ref.split(' ', 1)
      if len(parts) == 1:
        ref = parts[0]
        rest = ''
      else:
        ref, rest = parts
        rest = ' ' + rest
      if not ref[-1].isalnum():
        rest = ref[-1] + rest
        ref = ref[:-1]
      ref_dict = GetLinkToRefType(self._namespace.name, ref)
      formatted_description.append('<a href="%(href)s">%(text)s</a>%(rest)s' %
          { 'href': ref_dict['href'], 'text': ref_dict['text'], 'rest': rest })
    return ''.join(formatted_description)

  def ToDict(self):
    if self._namespace is None:
      return {}
    return {
      'name': self._namespace.name,
      'types':  [self._GenerateType(t) for t in self._namespace.types.values()
                 if t.type_ != model.PropertyType.ADDITIONAL_PROPERTIES],
      'functions': self._GenerateFunctions(self._namespace.functions),
      'events': self._GenerateEvents(self._namespace.events),
      'properties': self._GenerateProperties(self._namespace.properties)
    }

  def _GenerateType(self, type_):
    type_dict = {
      'name': type_.simple_name,
      'description': self._FormatDescription(type_.description),
      'properties': self._GenerateProperties(type_.properties),
      'functions': self._GenerateFunctions(type_.functions),
      'events': self._GenerateEvents(type_.events),
      'id': _CreateId(type_, 'type')
    }
    self._RenderTypeInformation(type_, type_dict)
    return type_dict

  def _GenerateFunctions(self, functions):
    return [self._GenerateFunction(f) for f in functions.values()]

  def _GenerateFunction(self, function):
    function_dict = {
      'name': function.simple_name,
      'description': self._FormatDescription(function.description),
      'callback': self._GenerateCallback(function.callback),
      'parameters': [],
      'returns': None,
      'id': _CreateId(function, 'method')
    }
    if (function.parent is not None and
        not isinstance(function.parent, model.Namespace)):
      function_dict['parent_name'] = function.parent.simple_name
    else:
      function_dict['parent_name'] = None
    if function.returns:
      function_dict['returns'] = self._GenerateProperty(function.returns)
    for param in function.params:
      function_dict['parameters'].append(self._GenerateProperty(param))
    if function_dict['callback']:
      function_dict['parameters'].append(function_dict['callback'])
    if len(function_dict['parameters']) > 0:
      function_dict['parameters'][-1]['last'] = True
    return function_dict

  def _GenerateEvents(self, events):
    return [self._GenerateEvent(e) for e in events.values()]

  def _GenerateEvent(self, event):
    event_dict = {
      'name': event.simple_name,
      'description': self._FormatDescription(event.description),
      'parameters': map(self._GenerateProperty, event.params),
      'callback': self._GenerateCallback(event.callback),
      'conditions': [GetLinkToRefType(self._namespace.name, c)
                     for c in event.conditions],
      'actions': [GetLinkToRefType(self._namespace.name, a)
                     for a in event.actions],
      'filters': map(self._GenerateProperty, event.filters),
      'supportsRules': event.supports_rules,
      'id': _CreateId(event, 'event')
    }
    if (event.parent is not None and
        not isinstance(event.parent, model.Namespace)):
      event_dict['parent_name'] = event.parent.simple_name
    else:
      event_dict['parent_name'] = None
    if event_dict['callback']:
      event_dict['parameters'].append(event_dict['callback'])
    if len(event_dict['parameters']) > 0:
      event_dict['parameters'][-1]['last'] = True
    return event_dict

  def _GenerateCallback(self, callback):
    if not callback:
      return None
    callback_dict = {
      'name': callback.simple_name,
      'description': self._FormatDescription(callback.description),
      'simple_type': {'simple_type': 'function'},
      'optional': callback.optional,
      'parameters': []
    }
    for param in callback.params:
      callback_dict['parameters'].append(self._GenerateProperty(param))
    if (len(callback_dict['parameters']) > 0):
      callback_dict['parameters'][-1]['last'] = True
    return callback_dict

  def _GenerateProperties(self, properties):
    return [self._GenerateProperty(v) for v in properties.values()
            if v.type_ != model.PropertyType.ADDITIONAL_PROPERTIES]

  def _GenerateProperty(self, property_):
    property_dict = {
      'name': property_.simple_name,
      'optional': property_.optional,
      'description': self._FormatDescription(property_.description),
      'properties': self._GenerateProperties(property_.properties),
      'parameters': [],
      'functions': self._GenerateFunctions(property_.functions),
      'returns': None,
      'id': _CreateId(property_, 'property')
    }
    for param in property_.params:
      property_dict['parameters'].append(self._GenerateProperty(param))
    if property_.returns:
      property_dict['returns'] = self._GenerateProperty(property_.returns)
    if (property_.parent is not None and
        not isinstance(property_.parent, model.Namespace)):
      property_dict['parent_name'] = property_.parent.simple_name
    else:
      property_dict['parent_name'] = None
    if property_.has_value:
      if isinstance(property_.value, int):
        property_dict['value'] = _FormatValue(property_.value)
      else:
        property_dict['value'] = property_.value
    else:
      self._RenderTypeInformation(property_, property_dict)
    return property_dict

  def _RenderTypeInformation(self, property_, dst_dict):
    if property_.type_ == model.PropertyType.CHOICES:
      dst_dict['choices'] = map(self._GenerateProperty,
                                property_.choices.values())
      # We keep track of which is last for knowing when to add "or" between
      # choices in templates.
      if len(dst_dict['choices']) > 0:
        dst_dict['choices'][-1]['last'] = True
    elif property_.type_ == model.PropertyType.REF:
      dst_dict['link'] = GetLinkToRefType(self._namespace.name,
                                          property_.ref_type)
    elif property_.type_ == model.PropertyType.ARRAY:
      dst_dict['array'] = self._GenerateProperty(property_.item_type)
    elif property_.type_ == model.PropertyType.ENUM:
      dst_dict['enum_values'] = []
      for enum_value in property_.enum_values:
        dst_dict['enum_values'].append({'name': enum_value})
      if len(dst_dict['enum_values']) > 0:
        dst_dict['enum_values'][-1]['last'] = True
    elif property_.instance_of is not None:
      dst_dict['simple_type'] = property_.instance_of.lower()
    else:
      dst_dict['simple_type'] = property_.type_.name.lower()

class _LazySamplesGetter(object):
  """This class is needed so that an extensions API page does not have to fetch
  the apps samples page and vice versa.
  """
  def __init__(self, api_name, samples):
    self._api_name = api_name
    self._samples = samples

  def get(self, key):
    return self._samples.FilterSamples(key, self._api_name)

class APIDataSource(object):
  """This class fetches and loads JSON APIs from the FileSystem passed in with
  |cache_factory|, so the APIs can be plugged into templates.
  """
  class Factory(object):
    def __init__(self, cache_factory, base_path, samples_factory):
      self._permissions_cache = cache_factory.Create(self._LoadPermissions,
                                                     compiled_fs.PERMS,
                                                     version=_VERSION)
      self._json_cache = cache_factory.Create(self._LoadJsonAPI,
                                              compiled_fs.JSON,
                                              version=_VERSION)
      self._idl_cache = cache_factory.Create(self._LoadIdlAPI,
                                             compiled_fs.IDL,
                                             version=_VERSION)
      self._idl_names_cache = cache_factory.Create(self._GetIDLNames,
                                                   compiled_fs.IDL_NAMES,
                                                   version=_VERSION)
      self._samples_factory = samples_factory
      self._base_path = base_path

    def Create(self, request):
      return APIDataSource(self._permissions_cache,
                           self._json_cache,
                           self._idl_cache,
                           self._idl_names_cache,
                           self._base_path,
                           self._samples_factory.Create(request))

    def _LoadPermissions(self, json_str):
      return json.loads(json_comment_eater.Nom(json_str))

    def _LoadJsonAPI(self, api):
      return _JscModel(json.loads(json_comment_eater.Nom(api))[0])

    def _LoadIdlAPI(self, api):
      idl = idl_parser.IDLParser().ParseData(api)
      return _JscModel(idl_schema.IDLSchema(idl).process()[0])

    def _GetIDLNames(self, apis):
      return [model.UnixName(os.path.splitext(api.split('/')[-1])[0])
              for api in apis if api.endswith('.idl')]

  def __init__(self,
               permissions_cache,
               json_cache,
               idl_cache,
               idl_names_cache,
               base_path,
               samples):
    self._base_path = base_path
    self._permissions_cache = permissions_cache
    self._json_cache = json_cache
    self._idl_cache = idl_cache
    self._idl_names_cache = idl_names_cache
    self._samples = samples

  def _GetPermsFromFile(self, filename):
    try:
      perms = self._permissions_cache.GetFromFile('%s/%s' %
          (self._base_path, filename))
      return dict((model.UnixName(k), v) for k, v in perms.iteritems())
    except FileNotFoundError:
      return {}

  def _GetFeature(self, path):
    # Remove 'experimental_' from path name to match the keys in
    # _permissions_features.json.
    path = model.UnixName(path.replace('experimental_', ''))
    for filename in ['_permission_features.json', '_manifest_features.json']:
      api_perms = self._GetPermsFromFile(filename).get(path, None)
      if api_perms is not None:
        break
    if api_perms and api_perms['channel'] in ('trunk', 'dev', 'beta'):
      api_perms[api_perms['channel']] = True
    return api_perms

  def _GenerateHandlebarContext(self, handlebar, path):
    return_dict = {
      'permissions': self._GetFeature(path),
      'samples': _LazySamplesGetter(path, self._samples)
    }
    return_dict.update(handlebar.ToDict())
    return return_dict

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    path, ext = os.path.splitext(key)
    unix_name = model.UnixName(path)
    idl_names = self._idl_names_cache.GetFromFileListing(self._base_path)
    cache, ext = ((self._idl_cache, '.idl') if (unix_name in idl_names) else
                  (self._json_cache, '.json'))
    return self._GenerateHandlebarContext(
        cache.GetFromFile('%s/%s%s' % (self._base_path, unix_name, ext)),
        path)
