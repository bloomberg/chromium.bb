# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os

from docs_server_utils import GetLinkToRefType
import third_party.json_schema_compiler.model as model

def _RemoveNoDocs(item):
  if type(item) == dict:
    if item.get('nodoc', False):
      return True
    for key, value in item.items():
      if _RemoveNoDocs(value):
        del item[key]
  elif type(item) == list:
    for i in item:
      if _RemoveNoDocs(i):
        item.remove(i)
  return False

def _FormatValue(value):
  """Inserts commas every three digits for integer values. It is magic.
  """
  s = str(value)
  return ','.join([s[max(0, i - 3):i] for i in range(len(s), 0, -3)][::-1])

class HandlebarDictGenerator(object):
  """Uses a Model from the JSON Schema Compiler and generates a dict that
  a Handlebar template can use for a data source.
  """
  def __init__(self, json):
    clean_json = copy.deepcopy(json)
    if _RemoveNoDocs(clean_json):
      self._namespace = None
    else:
      self._namespace = model.Namespace(clean_json, clean_json['namespace'])

  def _StripPrefix(self, name):
    if name.startswith(self._namespace.name + '.'):
      return name[len(self._namespace.name + '.'):]
    return name

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

  def Generate(self, samples):
    if self._namespace is None:
      return { 'samples': samples }
    return {
      'name': self._namespace.name,
      'types': map(self._GenerateType, self._namespace.types.values()),
      'functions': self._GenerateFunctions(self._namespace.functions),
      'events': map(self._GenerateEvent, self._namespace.events.values()),
      'properties': self._GenerateProperties(self._namespace.properties),
      'samples': samples,
    }

  def _GenerateType(self, type_):
    type_dict = {
      'name': self._StripPrefix(type_.name),
      'description': self._FormatDescription(type_.description),
      'properties': self._GenerateProperties(type_.properties),
      'functions': self._GenerateFunctions(type_.functions),
      'events': map(self._GenerateEvent, type_.events.values())
    }
    self._RenderTypeInformation(type_, type_dict)
    return type_dict

  def _GenerateFunctions(self, functions):
    return map(self._GenerateFunction, functions.values())

  def _GenerateFunction(self, function):
    function_dict = {
      'name': function.name,
      'description': self._FormatDescription(function.description),
      'callback': self._GenerateCallback(function.callback),
      'parameters': [],
      'returns': None
    }
    if function.returns:
      function_dict['returns'] = self._GenerateProperty(function.returns)
    for param in function.params:
      function_dict['parameters'].append(self._GenerateProperty(param))
    if function_dict['callback']:
      function_dict['parameters'].append(function_dict['callback'])
    if len(function_dict['parameters']) > 0:
      function_dict['parameters'][-1]['last'] = True
    return function_dict

  def _GenerateEvent(self, event):
    event_dict = {
      'name': self._StripPrefix(event.name),
      'description': self._FormatDescription(event.description),
      'parameters': map(self._GenerateProperty, event.params)
    }
    if len(event_dict['parameters']) > 0:
      event_dict['parameters'][-1]['last'] = True
    return event_dict

  def _GenerateCallback(self, callback):
    if not callback:
      return None
    callback_dict = {
      'name': callback.name,
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
    return map(self._GenerateProperty, properties.values())

  def _GenerateProperty(self, property_):
    property_dict = {
      'name': self._StripPrefix(property_.name),
      'optional': property_.optional,
      'description': self._FormatDescription(property_.description),
      'properties': self._GenerateProperties(property_.properties),
      'functions': self._GenerateFunctions(property_.functions)
    }
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
    elif property_.type_ == model.PropertyType.ADDITIONAL_PROPERTIES:
      dst_dict['additional_properties'] = True
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
    elif property_.instance_of:
      dst_dict['simple_type'] = property_.instance_of.lower()
    else:
      dst_dict['simple_type'] = property_.type_.name.lower()
