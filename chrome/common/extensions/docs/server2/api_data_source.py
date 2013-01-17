# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging
import os

import compiled_file_system as compiled_fs
from file_system import FileNotFoundError
import third_party.json_schema_compiler.json_parse as json_parse
import third_party.json_schema_compiler.model as model
import third_party.json_schema_compiler.idl_schema as idl_schema
import third_party.json_schema_compiler.idl_parser as idl_parser

# Increment this version when there are changes to the data stored in any of
# the caches used by APIDataSource. This would include changes to model.py in
# JSON schema compiler! This allows the cache to be invalidated without having
# to flush memcache on the production server.
_VERSION = 13

def _RemoveNoDocs(item):
  if json_parse.IsDict(item):
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

class _JSCModel(object):
  """Uses a Model from the JSON Schema Compiler and generates a dict that
  a Handlebar template can use for a data source.
  """
  def __init__(self, json, ref_resolver, disable_refs):
    self._ref_resolver = ref_resolver
    self._disable_refs = disable_refs
    clean_json = copy.deepcopy(json)
    if _RemoveNoDocs(clean_json):
      self._namespace = None
    else:
      self._namespace = model.Namespace(clean_json, clean_json['namespace'])

  def _FormatDescription(self, description):
    if self._disable_refs:
      return description
    return self._ref_resolver.ResolveAllLinks(description,
                                              namespace=self._namespace.name)

  def _GetLink(self, link):
    if self._disable_refs:
      type_name = link.split('.', 1)[-1]
      return { 'href': '#type-%s' % type_name, 'text': link, 'name': link }
    return self._ref_resolver.SafeGetLink(link, namespace=self._namespace.name)

  def ToDict(self):
    if self._namespace is None:
      return {}
    return {
      'name': self._namespace.name,
      'types': self._GenerateTypes(self._namespace.types.values()),
      'functions': self._GenerateFunctions(self._namespace.functions),
      'events': self._GenerateEvents(self._namespace.events),
      'properties': self._GenerateProperties(self._namespace.properties)
    }

  def _GenerateTypes(self, types):
    return [self._GenerateType(t) for t in types]

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
    if function.returns:
      function_dict['returns'] = self._GenerateProperty(function.returns)
    for param in function.params:
      function_dict['parameters'].append(self._GenerateProperty(param))
    if function.callback is not None:
      # Show the callback as an extra parameter.
      function_dict['parameters'].append(
          self._GenerateCallbackProperty(function.callback))
    if len(function_dict['parameters']) > 0:
      function_dict['parameters'][-1]['last'] = True
    return function_dict

  def _GenerateEvents(self, events):
    return [self._GenerateEvent(e) for e in events.values()]

  def _GenerateEvent(self, event):
    event_dict = {
      'name': event.simple_name,
      'description': self._FormatDescription(event.description),
      'parameters': [self._GenerateProperty(p) for p in event.params],
      'callback': self._GenerateCallback(event.callback),
      'filters': [self._GenerateProperty(f) for f in event.filters],
      'conditions': [self._GetLink(condition)
                     for condition in event.conditions],
      'actions': [self._GetLink(action) for action in event.actions],
      'supportsRules': event.supports_rules,
      'id': _CreateId(event, 'event')
    }
    if (event.parent is not None and
        not isinstance(event.parent, model.Namespace)):
      event_dict['parent_name'] = event.parent.simple_name
    if event.callback is not None:
      # Show the callback as an extra parameter.
      event_dict['parameters'].append(
          self._GenerateCallbackProperty(event.callback))
    if len(event_dict['parameters']) > 0:
      event_dict['parameters'][-1]['last'] = True
    return event_dict

  def _GenerateCallback(self, callback):
    if not callback:
      return None
    callback_dict = {
      'name': callback.simple_name,
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
    return [self._GenerateProperty(v) for v in properties.values()]

  def _GenerateProperty(self, property_):
    type_ = property_.type_

    # Make sure we generate property info for arrays, too.
    # TODO(kalman): what about choices?
    if type_.property_type == model.PropertyType.ARRAY:
      properties = type_.item_type.properties
    else:
      properties = type_.properties

    property_dict = {
      'name': property_.simple_name,
      'optional': property_.optional,
      'description': self._FormatDescription(property_.description),
      'properties': self._GenerateProperties(type_.properties),
      'functions': self._GenerateFunctions(type_.functions),
      'parameters': [],
      'returns': None,
      'id': _CreateId(property_, 'property')
    }

    if type_.property_type == model.PropertyType.FUNCTION:
      function = type_.function
      for param in function.params:
        property_dict['parameters'].append(self._GenerateProperty(param))
      if function.returns:
        property_dict['returns'] = self._GenerateProperty(function.returns)

    if (property_.parent is not None and
        not isinstance(property_.parent, model.Namespace)):
      property_dict['parent_name'] = property_.parent.simple_name

    value = property_.value
    if value is not None:
      if isinstance(value, int):
        property_dict['value'] = _FormatValue(value)
      else:
        property_dict['value'] = value
    else:
      self._RenderTypeInformation(type_, property_dict)

    return property_dict

  def _GenerateCallbackProperty(self, callback):
    property_dict = {
      'name': callback.simple_name,
      'description': self._FormatDescription(callback.description),
      'optional': callback.optional,
      'id': _CreateId(callback, 'property'),
      'simple_type': 'function',
    }
    if (callback.parent is not None and
        not isinstance(callback.parent, model.Namespace)):
      property_dict['parent_name'] = callback.parent.simple_name
    return property_dict

  def _RenderTypeInformation(self, type_, dst_dict):
    if type_.property_type == model.PropertyType.CHOICES:
      dst_dict['choices'] = self._GenerateTypes(type_.choices)
      # We keep track of which is last for knowing when to add "or" between
      # choices in templates.
      if len(dst_dict['choices']) > 0:
        dst_dict['choices'][-1]['last'] = True
    elif type_.property_type == model.PropertyType.REF:
      dst_dict['link'] = self._GetLink(type_.ref_type)
    elif type_.property_type == model.PropertyType.ARRAY:
      dst_dict['array'] = self._GenerateType(type_.item_type)
    elif type_.property_type == model.PropertyType.ENUM:
      dst_dict['enum_values'] = []
      for enum_value in type_.enum_values:
        dst_dict['enum_values'].append({'name': enum_value})
      if len(dst_dict['enum_values']) > 0:
        dst_dict['enum_values'][-1]['last'] = True
    elif type_.instance_of is not None:
      dst_dict['simple_type'] = type_.instance_of.lower()
    else:
      dst_dict['simple_type'] = type_.property_type.name.lower()

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
    def __init__(self,
                 cache_factory,
                 base_path):
      self._permissions_cache = cache_factory.Create(self._LoadPermissions,
                                                     compiled_fs.PERMS,
                                                     version=_VERSION)
      self._json_cache = cache_factory.Create(
          lambda api_name, api: self._LoadJsonAPI(api, False),
          compiled_fs.JSON,
          version=_VERSION)
      self._idl_cache = cache_factory.Create(
          lambda api_name, api: self._LoadIdlAPI(api, False),
          compiled_fs.IDL,
          version=_VERSION)

      # These caches are used if an APIDataSource does not want to resolve the
      # $refs in an API. This is needed to prevent infinite recursion in
      # ReferenceResolver.
      self._json_cache_no_refs = cache_factory.Create(
          lambda api_name, api: self._LoadJsonAPI(api, True),
          compiled_fs.JSON_NO_REFS,
          version=_VERSION)
      self._idl_cache_no_refs = cache_factory.Create(
          lambda api_name, api: self._LoadIdlAPI(api, True),
          compiled_fs.IDL_NO_REFS,
          version=_VERSION)
      self._idl_names_cache = cache_factory.Create(self._GetIDLNames,
                                                   compiled_fs.IDL_NAMES,
                                                   version=_VERSION)
      self._names_cache = cache_factory.Create(self._GetAllNames,
                                               compiled_fs.NAMES,
                                               version=_VERSION)
      self._base_path = base_path

      # These must be set later via the SetFooDataSourceFactory methods.
      self._ref_resolver_factory = None
      self._samples_data_source_factory = None

    def SetSamplesDataSourceFactory(self, samples_data_source_factory):
      self._samples_data_source_factory = samples_data_source_factory

    def SetReferenceResolverFactory(self, ref_resolver_factory):
      self._ref_resolver_factory = ref_resolver_factory

    def Create(self, request, disable_refs=False):
      """Create an APIDataSource. |disable_refs| specifies whether $ref's in
      APIs being processed by the |ToDict| method of _JSCModel follows $ref's
      in the API. This prevents endless recursion in ReferenceResolver.
      """
      if self._samples_data_source_factory is None:
        # Only error if there is a request, which means this APIDataSource is
        # actually being used to render a page.
        if request is not None:
          logging.error('SamplesDataSource.Factory was never set in '
                        'APIDataSource.Factory.')
        samples = None
      else:
        samples = self._samples_data_source_factory.Create(request)
      if not disable_refs and self._ref_resolver_factory is None:
        logging.error('ReferenceResolver.Factory was never set in '
                      'APIDataSource.Factory.')
      return APIDataSource(self._permissions_cache,
                           self._json_cache,
                           self._idl_cache,
                           self._json_cache_no_refs,
                           self._idl_cache_no_refs,
                           self._names_cache,
                           self._idl_names_cache,
                           self._base_path,
                           samples,
                           disable_refs)

    def _LoadPermissions(self, file_name, json_str):
      return json_parse.Parse(json_str)

    def _LoadJsonAPI(self, api, disable_refs):
      return _JSCModel(
          json_parse.Parse(api)[0],
          self._ref_resolver_factory.Create() if not disable_refs else None,
          disable_refs).ToDict()

    def _LoadIdlAPI(self, api, disable_refs):
      idl = idl_parser.IDLParser().ParseData(api)
      return _JSCModel(
          idl_schema.IDLSchema(idl).process()[0],
          self._ref_resolver_factory.Create() if not disable_refs else None,
          disable_refs).ToDict()

    def _GetIDLNames(self, base_dir, apis):
      return [
        model.UnixName(os.path.splitext(api[len('%s/' % self._base_path):])[0])
        for api in apis if api.endswith('.idl')
      ]

    def _GetAllNames(self, base_dir, apis):
      return [
        model.UnixName(os.path.splitext(api[len('%s/' % self._base_path):])[0])
        for api in apis
      ]

  def __init__(self,
               permissions_cache,
               json_cache,
               idl_cache,
               json_cache_no_refs,
               idl_cache_no_refs,
               names_cache,
               idl_names_cache,
               base_path,
               samples,
               disable_refs):
    self._base_path = base_path
    self._permissions_cache = permissions_cache
    self._json_cache = json_cache
    self._idl_cache = idl_cache
    self._json_cache_no_refs = json_cache_no_refs
    self._idl_cache_no_refs = idl_cache_no_refs
    self._names_cache = names_cache
    self._idl_names_cache = idl_names_cache
    self._samples = samples
    self._disable_refs = disable_refs

  def _GetFeatureFile(self, filename):
    try:
      perms = self._permissions_cache.GetFromFile('%s/%s' %
          (self._base_path, filename))
      return dict((model.UnixName(k), v) for k, v in perms.iteritems())
    except FileNotFoundError:
      return {}

  def _GetFeatureData(self, path):
    # Remove 'experimental_' from path name to match the keys in
    # _permissions_features.json.
    path = model.UnixName(path.replace('experimental_', ''))

    for filename in ['_permission_features.json', '_manifest_features.json']:
      feature_data = self._GetFeatureFile(filename).get(path, None)
      if feature_data is not None:
        break

    # There are specific cases in which the feature is actually a list of
    # features where only one needs to match; but currently these are only
    # used to whitelist features for specific extension IDs. Filter those out.
    if isinstance(feature_data, list):
      feature_list = feature_data
      feature_data = None
      for single_feature in feature_list:
        if 'whitelist' in single_feature:
          continue
        if feature_data is not None:
          # Note: if you are seeing the exception below, add more heuristics as
          # required to form a single feature.
          raise ValueError('Multiple potential features match %s. I can\'t '
                           'decide which one to use. Please help!' % path)
        feature_data = single_feature

    if feature_data and feature_data['channel'] in ('trunk', 'dev', 'beta'):
      feature_data[feature_data['channel']] = True
    return feature_data

  def _GenerateHandlebarContext(self, handlebar_dict, path):
    handlebar_dict['permissions'] = self._GetFeatureData(path)
    handlebar_dict['samples'] = _LazySamplesGetter(path, self._samples)
    return handlebar_dict

  def _GetAsSubdirectory(self, name):
    if name.startswith('experimental_'):
      parts = name[len('experimental_'):].split('_', 1)
      parts[1] = 'experimental_%s' % parts[1]
      return '/'.join(parts)
    return name.replace('_', '/', 1)

  def get(self, key):
    if key.endswith('.html') or key.endswith('.json') or key.endswith('.idl'):
      path, ext = os.path.splitext(key)
    else:
      path = key
    unix_name = model.UnixName(path)
    idl_names = self._idl_names_cache.GetFromFileListing(self._base_path)
    names = self._names_cache.GetFromFileListing(self._base_path)
    if unix_name not in names and self._GetAsSubdirectory(unix_name) in names:
      unix_name = self._GetAsSubdirectory(unix_name)

    if self._disable_refs:
      cache, ext = (
          (self._idl_cache_no_refs, '.idl') if (unix_name in idl_names) else
          (self._json_cache_no_refs, '.json'))
    else:
      cache, ext = ((self._idl_cache, '.idl') if (unix_name in idl_names) else
                    (self._json_cache, '.json'))
    return self._GenerateHandlebarContext(
        cache.GetFromFile('%s/%s%s' % (self._base_path, unix_name, ext)),
        path)
