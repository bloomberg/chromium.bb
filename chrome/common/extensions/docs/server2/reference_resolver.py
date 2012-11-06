# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileNotFoundError

def _ClassifySchemaNode(node_name, api):
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
          return group
  return None

class ReferenceResolver(object):
  """Resolves references to $ref's by searching through the APIs to find the
  correct node.
  """
  class Factory(object):
    def __init__(self, api_data_source, api_list_data_source):
      self._api_data_source = api_data_source
      self._api_list_data_source = api_list_data_source

    def Create(self):
      return ReferenceResolver(self._api_data_source,
                               self._api_list_data_source)

  def __init__(self, api_data_source, api_list_data_source):
    self._api_data_source = api_data_source
    self._api_list_data_source = api_list_data_source

  def GetLink(self, namespace_name, ref):
    # Try to resolve the ref in the current namespace first.
    try:
      api = self._api_data_source.get(namespace_name)
      category = _ClassifySchemaNode(ref, api)
      if category is not None:
        return {
          'href': '#%s-%s' % (category, ref.replace('.', '-')),
          'text': ref
        }
    except FileNotFoundError:
      pass
    parts = ref.split('.')
    api_list = self._api_list_data_source.GetAllNames()
    for i, part in enumerate(parts):
      if '.'.join(parts[:i]) not in api_list:
        continue
      try:
        api_name = '.'.join(parts[:i])
        api = self._api_data_source.get(api_name)
      except FileNotFoundError:
        continue
      name = '.'.join(parts[i:])
      category = _ClassifySchemaNode(name, api)
      if category is None:
        continue
      text = ref
      if text.startswith('%s.' % namespace_name):
        text = text[len('%s.' % namespace_name):]
      return {
        'href': '%s.html#%s-%s' % (api_name, category, name.replace('.', '-')),
        'text': text
      }
    return None
