# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json


def _NameForNode(node):
  '''Creates a unique id for an object in an API schema, depending on
  what type of attribute the object is a member of.
  '''
  if 'namespace' in node: return node['namespace']
  if 'name' in node: return node['name']
  if 'id' in node: return node['id']
  if 'type' in node: return node['type']
  assert False, 'Problems with naming node: %s' % json.dumps(node, indent=3)


def _IsObjectList(value):
  '''Determines whether or not |value| is a list made up entirely of
  dict-like objects.
  '''
  return (isinstance(value, collections.Iterable) and
          all(isinstance(node, collections.Mapping) for node in value))


def _CreateGraph(root):
  '''Recursively moves through an API schema, replacing lists of objects
  and non-object values with objects.
  '''
  schema_graph = {}
  if _IsObjectList(root):
    for node in root:
      name = _NameForNode(node)
      assert name not in schema_graph, 'Duplicate name in availability graph.'
      schema_graph[name] = dict((key, _CreateGraph(value)) for key, value
                                in node.iteritems())
  elif isinstance(root, collections.Mapping):
    for name, node in root.iteritems():
      schema_graph[name] = dict((key, _CreateGraph(value)) for key, value
                                in node.iteritems())
  return schema_graph


def _Subtract(minuend, subtrahend):
  ''' A Set Difference adaptation for graphs. Returns a |difference|,
  which contains key-value pairs found in |minuend| but not in
  |subtrahend|.
  '''
  difference = {}
  for key in minuend:
    if key not in subtrahend:
      # Record all of this key's children as being part of the difference.
      difference[key] = _Subtract(minuend[key], {})
    else:
      # Note that |minuend| and |subtrahend| are assumed to be graphs, and
      # therefore should have no lists present, only keys and nodes.
      rest = _Subtract(minuend[key], subtrahend[key])
      if rest:
        # Record a difference if children of this key differed at some point.
        difference[key] = rest
  return difference


class APISchemaGraph(object):
  '''Provides an interface for interacting with an API schema graph, a
  nested dict structure that allows for simpler lookups of schema data.
  '''

  def __init__(self, api_schema):
    self._graph = _CreateGraph(api_schema)

  def Subtract(self, other):
    '''Returns an APISchemaGraph instance representing keys that are in
    this graph but not in |other|.
    '''
    return APISchemaGraph(_Subtract(self._graph, other._graph))

  def Lookup(self, *path):
    '''Given a list of path components, |path|, checks if the
    APISchemaGraph instance contains |path|.
    '''
    node = self._graph
    for path_piece in path:
      node = node.get(path_piece)
      if node is None:
        return False
    return True

  def IsEmpty(self):
    '''Checks for an empty schema graph.
    '''
    return not self._graph
