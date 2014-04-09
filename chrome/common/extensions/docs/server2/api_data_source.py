# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import copy
import logging
import os
import posixpath

from data_source import DataSource
from environment import IsPreviewServer
from extensions_paths import JSON_TEMPLATES, PRIVATE_TEMPLATES
from file_system import FileNotFoundError
from future import Future, Collect
import third_party.json_schema_compiler.json_parse as json_parse
import third_party.json_schema_compiler.model as model
from environment import IsPreviewServer
from third_party.json_schema_compiler.memoize import memoize


def _CreateId(node, prefix):
  if node.parent is not None and not isinstance(node.parent, model.Namespace):
    return '-'.join([prefix, node.parent.simple_name, node.simple_name])
  return '-'.join([prefix, node.simple_name])


def _FormatValue(value):
  '''Inserts commas every three digits for integer values. It is magic.
  '''
  s = str(value)
  return ','.join([s[max(0, i - 3):i] for i in range(len(s), 0, -3)][::-1])


def _GetByNameDict(namespace):
  '''Returns a dictionary mapping names to named items from |namespace|.

  This lets us render specific API entities rather than the whole thing at once,
  for example {{apis.manifestTypes.byName.ExternallyConnectable}}.

  Includes items from namespace['types'], namespace['functions'],
  namespace['events'], and namespace['properties'].
  '''
  by_name = {}
  for item_type in ('types', 'functions', 'events', 'properties'):
    if item_type in namespace:
      old_size = len(by_name)
      by_name.update(
          (item['name'], item) for item in namespace[item_type])
      assert len(by_name) == old_size + len(namespace[item_type]), (
          'Duplicate name in %r' % namespace)
  return by_name


def _GetEventByNameFromEvents(events):
  '''Parses the dictionary |events| to find the definitions of members of the
  type Event.  Returns a dictionary mapping the name of a member to that
  member's definition.
  '''
  assert 'types' in events, \
      'The dictionary |events| must contain the key "types".'
  event_list = [t for t in events['types'] if t.get('name') == 'Event']
  assert len(event_list) == 1, 'Exactly one type must be called "Event".'
  return _GetByNameDict(event_list[0])


class _JSCModel(object):
  '''Uses a Model from the JSON Schema Compiler and generates a dict that
  a Handlebar template can use for a data source.
  '''

  def __init__(self,
               api_name,
               api_models,
               availability_finder,
               json_cache,
               template_cache,
               features_bundle,
               event_byname_function):
    self._availability_finder = availability_finder
    self._api_availabilities = json_cache.GetFromFile(
        posixpath.join(JSON_TEMPLATES, 'api_availabilities.json'))
    self._intro_tables = json_cache.GetFromFile(
        posixpath.join(JSON_TEMPLATES, 'intro_tables.json'))
    self._api_features = features_bundle.GetAPIFeatures()
    self._template_cache = template_cache
    self._event_byname_function = event_byname_function
    self._namespace = api_models.GetModel(api_name).Get()

  def _GetLink(self, link):
    ref = link if '.' in link else (self._namespace.name + '.' + link)
    return { 'ref': ref, 'text': link, 'name': link }

  def ToDict(self):
    if self._namespace is None:
      return {}
    chrome_dot_name = 'chrome.%s' % self._namespace.name
    as_dict = {
      'name': self._namespace.name,
      'namespace': self._namespace.documentation_options.get('namespace',
                                                             chrome_dot_name),
      'title': self._namespace.documentation_options.get('title',
                                                         chrome_dot_name),
      'documentationOptions': self._namespace.documentation_options,
      'types': self._GenerateTypes(self._namespace.types.values()),
      'functions': self._GenerateFunctions(self._namespace.functions),
      'events': self._GenerateEvents(self._namespace.events),
      'domEvents': self._GenerateDomEvents(self._namespace.events),
      'properties': self._GenerateProperties(self._namespace.properties),
      'introList': self._GetIntroTableList(),
      'channelWarning': self._GetChannelWarning(),
    }
    if self._namespace.deprecated:
      as_dict['deprecated'] = self._namespace.deprecated

    as_dict['byName'] = _GetByNameDict(as_dict)
    return as_dict

  def _GetApiAvailability(self):
    return self._availability_finder.GetApiAvailability(self._namespace.name)

  def _GetChannelWarning(self):
    if not self._IsExperimental():
      return { self._GetApiAvailability().channel: True }
    return None

  def _IsExperimental(self):
    return self._namespace.name.startswith('experimental')

  def _GenerateTypes(self, types):
    return [self._GenerateType(t) for t in types]

  def _GenerateType(self, type_):
    type_dict = {
      'name': type_.simple_name,
      'description': type_.description,
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
      'description': function.description,
      'callback': self._GenerateCallback(function.callback),
      'parameters': [],
      'returns': None,
      'id': _CreateId(function, 'method')
    }
    self._AddCommonProperties(function_dict, function)
    if function.returns:
      function_dict['returns'] = self._GenerateType(function.returns)
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
    return [self._GenerateEvent(e) for e in events.values()
            if not e.supports_dom]

  def _GenerateDomEvents(self, events):
    return [self._GenerateEvent(e) for e in events.values()
            if e.supports_dom]

  def _GenerateEvent(self, event):
    event_dict = {
      'name': event.simple_name,
      'description': event.description,
      'filters': [self._GenerateProperty(f) for f in event.filters],
      'conditions': [self._GetLink(condition)
                     for condition in event.conditions],
      'actions': [self._GetLink(action) for action in event.actions],
      'supportsRules': event.supports_rules,
      'supportsListeners': event.supports_listeners,
      'properties': [],
      'id': _CreateId(event, 'event'),
      'byName': {},
    }
    self._AddCommonProperties(event_dict, event)
    # Add the Event members to each event in this object.
    if self._event_byname_function:
      event_dict['byName'].update(self._event_byname_function())
    # We need to create the method description for addListener based on the
    # information stored in |event|.
    if event.supports_listeners:
      callback_object = model.Function(parent=event,
                                       name='callback',
                                       json={},
                                       namespace=event.parent,
                                       origin='')
      callback_object.params = event.params
      if event.callback:
        callback_object.callback = event.callback
      callback_parameters = self._GenerateCallbackProperty(callback_object)
      callback_parameters['last'] = True
      event_dict['byName']['addListener'] = {
        'name': 'addListener',
        'callback': self._GenerateFunction(callback_object),
        'parameters': [callback_parameters]
      }
    if event.supports_dom:
      # Treat params as properties of the custom Event object associated with
      # this DOM Event.
      event_dict['properties'] += [self._GenerateProperty(param)
                                   for param in event.params]
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
    if not hasattr(property_, 'type_'):
      for d in dir(property_):
        if not d.startswith('_'):
          print ('%s -> %s' % (d, getattr(property_, d)))
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
      'description': property_.description,
      'properties': self._GenerateProperties(type_.properties),
      'functions': self._GenerateFunctions(type_.functions),
      'parameters': [],
      'returns': None,
      'id': _CreateId(property_, 'property')
    }
    self._AddCommonProperties(property_dict, property_)

    if type_.property_type == model.PropertyType.FUNCTION:
      function = type_.function
      for param in function.params:
        property_dict['parameters'].append(self._GenerateProperty(param))
      if function.returns:
        property_dict['returns'] = self._GenerateType(function.returns)

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
      'description': callback.description,
      'optional': callback.optional,
      'is_callback': True,
      'id': _CreateId(callback, 'property'),
      'simple_type': 'function',
    }
    if (callback.parent is not None and
        not isinstance(callback.parent, model.Namespace)):
      property_dict['parentName'] = callback.parent.simple_name
    return property_dict

  def _RenderTypeInformation(self, type_, dst_dict):
    dst_dict['is_object'] = type_.property_type == model.PropertyType.OBJECT
    if type_.property_type == model.PropertyType.CHOICES:
      dst_dict['choices'] = self._GenerateTypes(type_.choices)
      # We keep track of which == last for knowing when to add "or" between
      # choices in templates.
      if len(dst_dict['choices']) > 0:
        dst_dict['choices'][-1]['last'] = True
    elif type_.property_type == model.PropertyType.REF:
      dst_dict['link'] = self._GetLink(type_.ref_type)
    elif type_.property_type == model.PropertyType.ARRAY:
      dst_dict['array'] = self._GenerateType(type_.item_type)
    elif type_.property_type == model.PropertyType.ENUM:
      dst_dict['enum_values'] = [
          {'name': value.name, 'description': value.description}
          for value in type_.enum_values]
      if len(dst_dict['enum_values']) > 0:
        dst_dict['enum_values'][-1]['last'] = True
    elif type_.instance_of is not None:
      dst_dict['simple_type'] = type_.instance_of
    else:
      dst_dict['simple_type'] = type_.property_type.name

  def _GetIntroTableList(self):
    '''Create a generic data structure that can be traversed by the templates
    to create an API intro table.
    '''
    intro_rows = [
      self._GetIntroDescriptionRow(),
      self._GetIntroAvailabilityRow()
    ] + self._GetIntroDependencyRows()

    # Add rows using data from intro_tables.json, overriding any existing rows
    # if they share the same 'title' attribute.
    row_titles = [row['title'] for row in intro_rows]
    for misc_row in self._GetMiscIntroRows():
      if misc_row['title'] in row_titles:
        intro_rows[row_titles.index(misc_row['title'])] = misc_row
      else:
        intro_rows.append(misc_row)

    return intro_rows

  def _GetIntroDescriptionRow(self):
    ''' Generates the 'Description' row data for an API intro table.
    '''
    return {
      'title': 'Description',
      'content': [
        { 'text': self._namespace.description }
      ]
    }

  def _GetIntroAvailabilityRow(self):
    ''' Generates the 'Availability' row data for an API intro table.
    '''
    if self._IsExperimental():
      status = 'experimental'
      version = None
    else:
      availability = self._GetApiAvailability()
      status = availability.channel
      version = availability.version
    return {
      'title': 'Availability',
      'content': [{
        'partial': self._template_cache.GetFromFile(
          posixpath.join(PRIVATE_TEMPLATES,
                         'intro_tables',
                         '%s_message.html' % status)).Get(),
        'version': version
      }]
    }

  def _GetIntroDependencyRows(self):
    # Devtools aren't in _api_features. If we're dealing with devtools, bail.
    if 'devtools' in self._namespace.name:
      return []

    api_feature = self._api_features.Get().get(self._namespace.name)
    if not api_feature:
      logging.error('"%s" not found in _api_features.json' %
                    self._namespace.name)
      return []

    permissions_content = []
    manifest_content = []

    def categorize_dependency(dependency):
      def make_code_node(text):
        return { 'class': 'code', 'text': text }

      context, name = dependency.split(':', 1)
      if context == 'permission':
        permissions_content.append(make_code_node('"%s"' % name))
      elif context == 'manifest':
        manifest_content.append(make_code_node('"%s": {...}' % name))
      elif context == 'api':
        transitive_dependencies = (
            self._api_features.Get().get(name, {}).get('dependencies', []))
        for transitive_dependency in transitive_dependencies:
          categorize_dependency(transitive_dependency)
      else:
        logging.error('Unrecognized dependency for %s: %s' %
                      (self._namespace.name, context))

    for dependency in api_feature.get('dependencies', ()):
      categorize_dependency(dependency)

    dependency_rows = []
    if permissions_content:
      dependency_rows.append({
        'title': 'Permissions',
        'content': permissions_content
      })
    if manifest_content:
      dependency_rows.append({
        'title': 'Manifest',
        'content': manifest_content
      })
    return dependency_rows

  def _GetMiscIntroRows(self):
    ''' Generates miscellaneous intro table row data, such as 'Permissions',
    'Samples', and 'Learn More', using intro_tables.json.
    '''
    misc_rows = []
    # Look up the API name in intro_tables.json, which is structured
    # similarly to the data structure being created. If the name is found, loop
    # through the attributes and add them to this structure.
    table_info = self._intro_tables.Get().get(self._namespace.name)
    if table_info is None:
      return misc_rows

    for category in table_info.iterkeys():
      content = []
      for node in table_info[category]:
        # If there is a 'partial' argument and it hasn't already been
        # converted to a Handlebar object, transform it to a template.
        if 'partial' in node:
          # Note: it's enough to copy() not deepcopy() because only a single
          # top-level key is being modified.
          node = copy(node)
          node['partial'] = self._template_cache.GetFromFile(
              posixpath.join(PRIVATE_TEMPLATES, node['partial'])).Get()
        content.append(node)
      misc_rows.append({ 'title': category, 'content': content })
    return misc_rows

  def _AddCommonProperties(self, target, src):
    if src.deprecated is not None:
      target['deprecated'] = src.deprecated
    if (src.parent is not None and
        not isinstance(src.parent, model.Namespace)):
      target['parentName'] = src.parent.simple_name


class _LazySamplesGetter(object):
  '''This class is needed so that an extensions API page does not have to fetch
  the apps samples page and vice versa.
  '''

  def __init__(self, api_name, samples):
    self._api_name = api_name
    self._samples = samples

  def get(self, key):
    return self._samples.FilterSamples(key, self._api_name)


class APIDataSource(DataSource):
  '''This class fetches and loads JSON APIs from the FileSystem passed in with
  |compiled_fs_factory|, so the APIs can be plugged into templates.
  '''
  def __init__(self, server_instance, request):
    file_system = server_instance.host_file_system_provider.GetTrunk()
    self._json_cache = server_instance.compiled_fs_factory.ForJson(file_system)
    self._template_cache = server_instance.compiled_fs_factory.ForTemplates(
        file_system)
    self._availability_finder = server_instance.availability_finder
    self._api_models = server_instance.api_models
    self._features_bundle = server_instance.features_bundle
    self._model_cache = server_instance.object_store_creator.Create(
        APIDataSource)

    # This caches the result of _LoadEventByName.
    self._event_byname = None
    self._samples = server_instance.samples_data_source_factory.Create(request)

  def _LoadEventByName(self):
    '''All events have some members in common. We source their description
    from Event in events.json.
    '''
    if self._event_byname is None:
      self._event_byname = _GetEventByNameFromEvents(
          self._GetSchemaModel('events'))
    return self._event_byname

  def _GetSchemaModel(self, api_name):
    jsc_model = self._model_cache.Get(api_name).Get()
    if jsc_model is not None:
      return jsc_model

    jsc_model = _JSCModel(
        api_name,
        self._api_models,
        self._availability_finder,
        self._json_cache,
        self._template_cache,
        self._features_bundle,
        self._LoadEventByName).ToDict()

    self._model_cache.Set(api_name, jsc_model)
    return jsc_model


  def _GenerateHandlebarContext(self, handlebar_dict):
    # Parsing samples on the preview server takes seconds and doesn't add
    # anything. Don't do it.
    if not IsPreviewServer():
      handlebar_dict['samples'] = _LazySamplesGetter(
          handlebar_dict['name'],
          self._samples)
    return handlebar_dict

  def get(self, api_name):
    return self._GenerateHandlebarContext(self._GetSchemaModel(api_name))

  def Cron(self):
    return Future(value=())
