# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging
import os

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

def _GetLinkToRefType(namespace_name, ref_type):
  terms = ref_type.split('.')
  if len(terms) > 1:
    text = '.'.join(terms[1:])
    href = terms[0] + '.html' + '#type-' + text
  else:
    href = namespace_name + '.html' + '#type-' +ref_type
    text = ref_type
  return ({
    "href": href,
    "text": text
  })

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
    _RemoveNoDocs(clean_json)
    try:
      self._namespace = model.Namespace(clean_json, clean_json['namespace'])
    except Exception as e:
      logging.info(e)

  def Generate(self):
    try:
      return {
        'name': self._namespace.name,
        'types': map(self._GenerateType, self._namespace.types.values()),
        'functions': self._GenerateFunctions(self._namespace.functions),
        'events': map(self._GenerateEvent, self._namespace.events.values()),
        'properties': self._GenerateProperties(self._namespace.properties)
      }
    except Exception as e:
      logging.info(e)

  def _GenerateType(self, type_):
    type_dict = {
      'name': type_.name,
      'description': type_.description,
      'properties': self._GenerateProperties(type_.properties),
      'functions': self._GenerateFunctions(type_.functions)
    }
    self._RenderTypeInformation(type_, type_dict)
    return type_dict

  def _GenerateFunctions(self, functions):
    return map(self._GenerateFunction, functions.values())

  def _GenerateFunction(self, function):
    function_dict = {
      'name': function.name,
      'description': function.description,
      'callback': self._GenerateCallback(function.callback),
      'parameters': []
    }
    for param in function.params:
      function_dict['parameters'].append(self._GenerateProperty(param))
    if function_dict['callback']:
      function_dict['parameters'].append(function_dict['callback'])
    if len(function_dict['parameters']) > 0:
      function_dict['parameters'][-1]['last'] = True
    return function_dict

  def _GenerateEvent(self, event):
    event_dict = {
      'name': event.name,
      'description': event.description,
      'parameters': map(self._GenerateProperty, event.params)
    }
    if len(event_dict['parameters']) > 0:
      event_dict['parameters'][-1]['last'] = True
    return event_dict

  def _GenerateCallback(self, callback):
    if not callback:
      return {}
    callback_dict = {
      'name': 'callback',
      'description': callback.description,
      'simple_type': {'type': 'function'},
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
      'name': property_.name,
      'optional': property_.optional,
      'description': property_.description,
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
    dst_dict['type'] = property_.type_.name.lower()
    if property_.type_ == model.PropertyType.CHOICES:
      dst_dict['choices'] = []
      for choice_name in property_.choices:
        dst_dict['choices'].append(self._GenerateProperty(
            property_.choices[choice_name]))
      # We keep track of which is last for knowing when to add "or" between
      # choices in templates.
      if len(dst_dict['choices']) > 0:
        dst_dict['choices'][-1]['last'] = True
    elif property_.type_ == model.PropertyType.REF:
      dst_dict['link'] = _GetLinkToRefType(self._namespace.name,
                                        property_.ref_type)
    elif property_.type_ == model.PropertyType.ARRAY:
      dst_dict['array'] = self._GenerateProperty(property_.item_type)
    else:
      dst_dict['simple_type'] = {'type': dst_dict['type']}
