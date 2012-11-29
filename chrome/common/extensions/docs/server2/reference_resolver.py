# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileNotFoundError
import object_store

def _ClassifySchemaNode(node_name, api):
  """Attempt to classify |node_name| in an API, determining whether |node_name|
  refers to a type, function, event, or property in |api|.
  """
  if '.' in node_name:
    node_name, rest = node_name.split('.', 1)
  else:
    rest = None
  for key, group in [('types', 'type'),
                     ('functions', 'method'),
                     ('events', 'event'),
                     ('properties', 'property')]:
    for item in api.get(key, []):
      if item['name'] == node_name:
        if rest is not None:
          ret = _ClassifySchemaNode(rest, item)
          if ret is not None:
            return ret
        else:
          return group, node_name
  return None

def _MakeKey(namespace_name, ref):
  return '%s.%s' % (namespace_name, ref)

class ReferenceResolver(object):
  """Resolves references to $ref's by searching through the APIs to find the
  correct node.
  """
  class Factory(object):
    def __init__(self,
                 api_data_source_factory,
                 api_list_data_source_factory,
                 object_store):
      self._api_data_source_factory = api_data_source_factory
      self._api_list_data_source_factory = api_list_data_source_factory
      self._object_store = object_store

    def Create(self):
      return ReferenceResolver(
          self._api_data_source_factory.Create(None, disable_refs=True),
          self._api_list_data_source_factory.Create(),
          self._object_store)

  def __init__(self, api_data_source, api_list_data_source, object_store):
    self._api_data_source = api_data_source
    self._api_list_data_source = api_list_data_source
    self._object_store = object_store

  def _GetRefLink(self, ref, api_list, namespace_name):
    parts = ref.split('.')
    for i, part in enumerate(parts):
      api_name = '.'.join(parts[:i])
      if api_name not in api_list:
        continue
      try:
        api = self._api_data_source.get(api_name)
      except FileNotFoundError:
        continue
      name = '.'.join(parts[i:])
      # Attempt to find |name| in the API.
      node_info = _ClassifySchemaNode(name, api)
      if node_info is None:
        # Check to see if this ref is a property. If it is, we want the ref to
        # the underlying type the property is referencing.
        for prop in api.get('properties', []):
          # If the name of this property is in the ref text, replace the
          # property with its type, and attempt to classify it.
          if prop['name'] in name and 'link' in prop:
            name_as_prop_type = name.replace(prop['name'], prop['link']['name'])
            node_info = _ClassifySchemaNode(name_as_prop_type, api)
            if node_info is not None:
              name = name_as_prop_type
              text = ref.replace(prop['name'], prop['link']['name'])
              break
        if node_info is None:
          continue
      else:
        text = ref
      category, node_name = node_info
      if text.startswith('%s.' % namespace_name):
        text = text[len('%s.' % namespace_name):]
      return {
        'href': '%s.html#%s-%s' % (api_name, category, name.replace('.', '-')),
        'text': text,
        'name': node_name
      }
    return None

  def GetLink(self, namespace_name, ref):
    link = self._object_store.Get(_MakeKey(namespace_name, ref),
                                  object_store.REFERENCE_RESOLVER).Get()
    if link is not None:
      return link

    api_list = self._api_list_data_source.GetAllNames()
    link = self._GetRefLink(ref, api_list, namespace_name)

    if link is None:
      # Try to resolve the ref in the current namespace.
      link = self._GetRefLink('%s.%s' % (namespace_name, ref),
                              api_list,
                              namespace_name)

    if link is not None:
      self._object_store.Set(_MakeKey(namespace_name, ref),
                             link,
                             object_store.REFERENCE_RESOLVER)
    return link
