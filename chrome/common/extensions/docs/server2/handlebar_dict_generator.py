# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
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

class HandlebarDictGenerator(object):
  """Uses a Model from the JSON Schema Compiler and generates a dict that
  a Handlebar template can use for a data source.
  """
  def __init__(self, json):
    clean_json = copy.deepcopy(json)
    _RemoveNoDocs(clean_json)
    self._namespace = model.Namespace(clean_json, clean_json['namespace'])

  def Generate(self):
    return {
      'name': self._namespace.name,
      'types': self._GenerateTypes(self._namespace.types),
      'functions': self._GenerateFunctions(self._namespace.functions)
    }

  def _GenerateTypes(self, types):
    types_list = []
    for type_name in types:
      types_list.append(self._GenerateType(types[type_name]))
    return types_list

  def _GenerateType(self, type_):
    type_dict = {
      'name': type_.name,
      'type': type_.type_.name.lower(),
      'description': type_.description,
      'properties': self._GenerateProperties(type_.properties),
      'functions': self._GenerateFunctions(type_.functions)
    }

    # Only Array types have 'item_type'.
    if type_.type_ == model.PropertyType.ARRAY:
      type_dict['array'] = self._GenerateType(type_.item_type)
    return type_dict

  def _GenerateFunctions(self, functions):
    functions_list = []
    for function_name in functions:
      functions_list.append(self._GenerateFunction(functions[function_name]))
    return functions_list

  def _GenerateFunction(self, function):
    function_dict = {
      'name': function.name,
      'description': function.description,
      'callback': self._GenerateCallback(function.callback),
      'parameters': []
    }

    for param in function.params:
      function_dict['parameters'].append(self._GenerateProperty(param))
    return function_dict

  def _GenerateCallback(self, callback):
    if not callback:
      return {}
    callback_dict = {
      'name': 'callback',
      'simple_type': {'type': 'function'},
      'optional': callback.optional,
      'parameters': []
    }
    for param in callback.params:
      callback_dict['parameters'].append(self._GenerateProperty(param))
    if (len(callback_dict['parameters']) > 0):
      callback_dict['parameters'][-1]['last_parameter'] = True
    return callback_dict

  def _GenerateProperties(self, properties):
    properties_list = []
    for property_name in properties:
      properties_list.append(self._GenerateProperty(properties[property_name]))
    return properties_list

  def _GenerateProperty(self, property_):
    property_dict = {
      'name': property_.name,
      'type': property_.type_.name.lower(),
      'optional': property_.optional,
      'description': property_.description,
      'properties': self._GenerateProperties(property_.properties)
    }

    if property_.type_ == model.PropertyType.CHOICES:
      property_dict['choices'] = []
      for choice_name in property_.choices:
        property_dict['choices'].append(self._GenerateProperty(
            property_.choices[choice_name]))
      # We keep track of which is last for knowing when to add "or" between
      # choices in templates.
      if len(property_dict['choices']) > 0:
        property_dict['choices'][-1]['last_choice'] = True
    elif property_.type_ == model.PropertyType.REF:
      property_dict['link'] = _GetLinkToRefType(self._namespace.name,
                                                property_.ref_type)
    elif property_.type_ == model.PropertyType.ARRAY:
      property_dict['array'] = self._GenerateProperty(property_.item_type)
    else:
      property_dict['simple_type'] = {'type': property_dict['type']}
    return property_dict
