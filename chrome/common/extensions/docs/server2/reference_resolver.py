# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileNotFoundError
import logging
import object_store
import re
import string

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

def _MakeKey(namespace, ref, title):
  return '%s.%s.%s' % (namespace, ref, title)

class ReferenceResolver(object):
  """Resolves references to $ref's by searching through the APIs to find the
  correct node.

  $ref's have two forms:

    $ref:api.node - Replaces the $ref with a link to node on the API page. The
                    title is set to the name of the node.

    $ref:[api.node The Title] - Same as the previous form but title is set to
                                "The Title".
  """

  # Matches after a $ref: that doesn't have []s.
  _bare_ref = re.compile('\w+(\.\w+)*')

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

  def _GetRefLink(self, ref, api_list, namespace, title):
    # Check nodes within each API the ref might refer to.
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
      if namespace is not None and text.startswith('%s.' % namespace):
        text = text[len('%s.' % namespace):]
      return {
        'href': '%s.html#%s-%s' % (api_name, category, name.replace('.', '-')),
        'text': title if title else text,
        'name': node_name
      }

    # If it's not a reference to an API node it might just be a reference to an
    # API. Check this last so that links within APIs take precedence over links
    # to other APIs.
    if ref in api_list:
      return {
        'href': '%s.html' % ref,
        'text': title if title else ref,
        'name': ref
      }

    return None

  def GetLink(self, ref, namespace=None, title=None):
    """Resolve $ref |ref| in namespace |namespace| if not None, returning None
    if it cannot be resolved.
    """
    link = self._object_store.Get(_MakeKey(namespace, ref, title),
                                  object_store.REFERENCE_RESOLVER).Get()
    if link is not None:
      return link

    api_list = self._api_list_data_source.GetAllNames()
    link = self._GetRefLink(ref, api_list, namespace, title)

    if link is None and namespace is not None:
      # Try to resolve the ref in the current namespace if there is one.
      link = self._GetRefLink('%s.%s' % (namespace, ref),
                              api_list,
                              namespace,
                              title)

    if link is not None:
      self._object_store.Set(_MakeKey(namespace, ref, title),
                             link,
                             object_store.REFERENCE_RESOLVER)
    return link

  def SafeGetLink(self, ref, namespace=None, title=None):
    """Resolve $ref |ref| in namespace |namespace|, or globally if None. If it
    cannot be resolved, pretend like it is a link to a type.
    """
    ref_data = self.GetLink(ref, namespace=namespace, title=title)
    if ref_data is not None:
      return ref_data
    logging.error('$ref %s could not be resolved in namespace %s.' %
                      (ref, namespace))
    type_name = ref.rsplit('.', 1)[-1]
    return {
      'href': '#type-%s' % type_name,
      'text': title if title else ref,
      'name': ref
    }

  def ResolveAllLinks(self, text, namespace=None):
    """This method will resolve all $ref links in |text| using namespace
    |namespace| if not None. Any links that cannot be resolved will be replaced
    using the default link format that |SafeGetLink| uses.
    """
    if text is None or '$ref:' not in text:
      return text
    split_text = text.split('$ref:')
    # |split_text| is an array of text chunks that all start with the
    # argument to '$ref:'.
    formatted_text = [split_text[0]]
    for ref_and_rest in split_text[1:]:
      title = None
      if ref_and_rest.startswith('[') and ']' in ref_and_rest:
        # Text was '$ref:[foo.bar maybe title] other stuff'.
        ref_with_title, rest = ref_and_rest[1:].split(']', 1)
        ref_with_title = ref_with_title.split(None, 1)
        if len(ref_with_title) == 1:
          # Text was '$ref:[foo.bar] other stuff'.
          ref = ref_with_title[0]
        else:
          # Text was '$ref:[foo.bar title] other stuff'.
          ref, title = ref_with_title
      else:
        # Text was '$ref:foo.bar other stuff'.
        match = self._bare_ref.match(ref_and_rest)
        if match is None:
          ref = ''
          rest = ref_and_rest
        else:
          ref = match.group()
          rest = ref_and_rest[match.end():]

      ref_dict = self.SafeGetLink(ref, namespace=namespace, title=title)
      formatted_text.append('<a href="%(href)s">%(text)s</a>%(rest)s' %
          { 'href': ref_dict['href'], 'text': ref_dict['text'], 'rest': rest })
    return ''.join(formatted_text)
