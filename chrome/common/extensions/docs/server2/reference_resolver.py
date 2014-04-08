# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import copy
import logging
import re

from file_system import FileNotFoundError
from third_party.json_schema_compiler.model import PropertyType


def _ClassifySchemaNode(node_name, node):
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
    for item in getattr(node, key, {}).itervalues():
      if item.simple_name == node_name:
        if rest is not None:
          ret = _ClassifySchemaNode(rest, item)
          if ret is not None:
            return ret
        else:
          return group, node_name
  return None


def _MakeKey(namespace, ref):
  key = '%s/%s' % (namespace, ref)
  # AppEngine doesn't like keys > 500, but there will be some other stuff
  # that goes into this key, so truncate it earlier.  This shoudn't be
  # happening anyway unless there's a bug, such as http://crbug.com/314102.
  max_size = 256
  if len(key) > max_size:
    logging.error('Key was >%s characters: %s' % (max_size, key))
    key = key[:max_size]
  return key


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

  def __init__(self, api_models, object_store):
    self._api_models = api_models
    self._object_store = object_store

  def _GetRefLink(self, ref, api_list, namespace):
    # Check nodes within each API the ref might refer to.
    parts = ref.split('.')
    for i, part in enumerate(parts):
      api_name = '.'.join(parts[:i])
      if api_name not in api_list:
        continue
      try:
        api_model = self._api_models.GetModel(api_name).Get()
      except FileNotFoundError:
        continue
      name = '.'.join(parts[i:])
      # Attempt to find |name| in the API.
      node_info = _ClassifySchemaNode(name, api_model)
      if node_info is None:
        # Check to see if this ref is a property. If it is, we want the ref to
        # the underlying type the property is referencing.
        for prop in api_model.properties.itervalues():
          # If the name of this property is in the ref text, replace the
          # property with its type, and attempt to classify it.
          if prop.name in name and prop.type_.property_type == PropertyType.REF:
            name_as_prop_type = name.replace(prop.name, prop.type_.ref_type)
            node_info = _ClassifySchemaNode(name_as_prop_type, api_model)
            if node_info is not None:
              name = name_as_prop_type
              text = ref.replace(prop.name, prop.type_.ref_type)
              break
        if node_info is None:
          continue
      else:
        text = ref
      category, node_name = node_info
      if namespace is not None and text.startswith('%s.' % namespace):
        text = text[len('%s.' % namespace):]
      api_model = self._api_models.GetModel(api_name).Get()
      filename = api_model.documentation_options.get('documented_in', api_name)
      return {
        'href': '%s#%s-%s' % (filename, category, name.replace('.', '-')),
        'text': text,
        'name': node_name
      }

    # If it's not a reference to an API node it might just be a reference to an
    # API. Check this last so that links within APIs take precedence over links
    # to other APIs.
    if ref in api_list:
      return {
        'href': '%s' % ref,
        'text': ref,
        'name': ref
      }

    return None

  def GetLink(self, ref, namespace=None, title=None):
    """Resolve $ref |ref| in namespace |namespace| if not None, returning None
    if it cannot be resolved.
    """
    db_key = _MakeKey(namespace, ref)
    link = self._object_store.Get(db_key).Get()
    if link is None:
      api_list = self._api_models.GetNames()
      link = self._GetRefLink(ref, api_list, namespace)
      if link is None and namespace is not None:
        # Try to resolve the ref in the current namespace if there is one.
        link = self._GetRefLink('%s.%s' % (namespace, ref), api_list, namespace)
      if link is None:
        return None
      self._object_store.Set(db_key, link)

    if title is not None:
      link = copy(link)
      link['text'] = title

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
      'text': title or ref,
      'name': ref
    }

  # TODO(ahernandez.miralles): This function is no longer needed,
  # and uses a deprecated style of ref
  def ResolveAllLinks(self, text, relative_to='', namespace=None):
    """This method will resolve all $ref links in |text| using namespace
    |namespace| if not None. Any links that cannot be resolved will be replaced
    using the default link format that |SafeGetLink| uses.
    The links will be generated relative to |relative_to|.
    """
    if text is None or '$ref:' not in text:
      return text

    # requestPath should be of the form (apps|extensions)/...../page.html.
    # link_prefix should  that the target will point to
    # (apps|extensions)/target.html. Note multiplying a string by a negative
    # number gives the empty string.
    link_prefix = '../' * (relative_to.count('/') - 1)
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
      formatted_text.append('<a href="%s%s">%s</a>%s' %
          (link_prefix, ref_dict['href'], ref_dict['text'], rest))
    return ''.join(formatted_text)
