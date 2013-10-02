# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
from collections import Iterable, Mapping

class LookupResult(object):
  '''Returned from APISchemaGraph.Lookup(), and relays whether or not
  some element was found and what annotation object was associated with it,
  if any.
  '''

  def __init__(self, found=None, annotation=None):
    assert found is not None, 'LookupResult was given None value for |found|.'
    self.found = found
    self.annotation = annotation

  def __eq__(self, other):
    return self.__dict__ == other.__dict__

  def __ne__(self, other):
    return not (self == other)

  def __repr__(self):
    return '%s%s' % (type(self).__name__, repr(self.__dict__))

  def __str__(self):
    return repr(self)


class _GraphNode(dict):
  '''Represents some element of an API schema, and allows extra information
  about that element to be stored on the |_annotation| object.
  '''

  def __init__(self, *args, **kwargs):
    # Use **kwargs here since Python is picky with ordering of default args
    # and variadic args in the method signature. The only keyword arg we care
    # about here is 'annotation'. Intentionally don't pass |**kwargs| into the
    # superclass' __init__().
    dict.__init__(self, *args)
    self._annotation = kwargs.get('annotation')

  def __eq__(self, other):
    # _GraphNode inherits __eq__() from dict, which will not take annotation
    # objects into account when comparing.
    return dict.__eq__(self, other)

  def __ne__(self, other):
    return not (self == other)


def _NameForNode(node):
  '''Creates a unique id for an object in an API schema, depending on
  what type of attribute the object is a member of.
  '''
  if 'namespace' in node: return node['namespace']
  if 'name' in node: return node['name']
  if 'id' in node: return node['id']
  if 'type' in node: return node['type']
  if '$ref' in node: return node['$ref']
  assert False, 'Problems with naming node: %s' % json.dumps(node, indent=3)


def _IsObjectList(value):
  '''Determines whether or not |value| is a list made up entirely of
  dict-like objects.
  '''
  return (isinstance(value, Iterable) and
          all(isinstance(node, Mapping) for node in value))


def _CreateGraph(root):
  '''Recursively moves through an API schema, replacing lists of objects
  and non-object values with objects.
  '''
  schema_graph = _GraphNode()
  if _IsObjectList(root):
    for node in root:
      name = _NameForNode(node)
      assert name not in schema_graph, 'Duplicate name in API schema graph.'
      schema_graph[name] = _GraphNode((key, _CreateGraph(value)) for
                                      key, value in node.iteritems())

  elif isinstance(root, Mapping):
    for name, node in root.iteritems():
      if not isinstance(node, Mapping):
        schema_graph[name] = _GraphNode()
      else:
        schema_graph[name] = _GraphNode((key, _CreateGraph(value)) for
                                        key, value in node.iteritems())
  return schema_graph


def _Subtract(minuend, subtrahend):
  ''' A Set Difference adaptation for graphs. Returns a |difference|,
  which contains key-value pairs found in |minuend| but not in
  |subtrahend|.
  '''
  difference = _GraphNode()
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


def _Update(base, addend, annotation=None):
  '''A Set Union adaptation for graphs. Returns a graph which contains
  the key-value pairs from |base| combined with any key-value pairs
  from |addend| that are not present in |base|.
  '''
  for key in addend:
    if key not in base:
      # Add this key and the rest of its children.
      base[key] = _Update(_GraphNode(annotation=annotation),
                          addend[key],
                          annotation=annotation)
    else:
      # The key is already in |base|, but check its children.
       _Update(base[key], addend[key], annotation=annotation)
  return base



class APISchemaGraph(object):
  '''Provides an interface for interacting with an API schema graph, a
  nested dict structure that allows for simpler lookups of schema data.
  '''

  def __init__(self, api_schema=None, _graph=None):
    self._graph = _graph if _graph is not None else _CreateGraph(api_schema)

  def __eq__(self, other):
    return self._graph == other._graph

  def __ne__(self, other):
    return not (self == other)

  def Subtract(self, other):
    '''Returns an APISchemaGraph instance representing keys that are in
    this graph but not in |other|.
    '''
    return APISchemaGraph(_graph=_Subtract(self._graph, other._graph))

  def Update(self, other, annotation=None):
    '''Modifies this graph by adding keys from |other| that are not
    already present in this graph.
    '''
    _Update(self._graph, other._graph, annotation=annotation)

  def Lookup(self, *path):
    '''Given a list of path components, |path|, checks if the
    APISchemaGraph instance contains |path|.
    '''
    node = self._graph
    for path_piece in path:
      node = node.get(path_piece)
      if node is None:
        return LookupResult(found=False, annotation=None)
    return LookupResult(found=True, annotation=node._annotation)

  def IsEmpty(self):
    '''Checks for an empty schema graph.
    '''
    return not self._graph
