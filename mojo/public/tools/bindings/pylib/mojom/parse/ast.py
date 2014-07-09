# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Node classes for the AST for a Mojo IDL file."""

# Note: For convenience of testing, you probably want to define __eq__() methods
# for all node types; it's okay to be slightly lax (e.g., not compare filename
# and lineno). You may also define __repr__() to help with analyzing test
# failures, especially for more complex types.


class NodeBase(object):
  """Base class for nodes in the AST."""

  def __init__(self, filename=None, lineno=None):
    self.filename = filename
    self.lineno = lineno


# TODO(vtl): Some of this is complicated enough that it should be tested.
class NodeListBase(NodeBase):
  """Represents a list of other nodes, all having the same type. (This is meant
  to be subclassed, with subclasses defining _list_item_type to be the class of
  the members of the list; _list_item_type should also be a NodeBase.)"""

  def __init__(self, item_or_items=None, **kwargs):
    assert issubclass(self._list_item_type, NodeBase)
    NodeBase.__init__(self, **kwargs)
    if item_or_items is None:
      self.elements = []
    elif isinstance(item_or_items, list):
      # TODO(vtl): Possibly we should assert that each element of the list is a
      # |_list_item_type|.
      self.elements = list(item_or_items)
    else:
      assert isinstance(item_or_items, self._list_item_type)
      self.elements = [item_or_items]
    self._UpdateFilenameAndLineno()

  # Support iteration. For everything else, users should just access |elements|
  # directly. (We intentionally do NOT supply |__len__()| or |__nonzero__()|, so
  # |bool(NodeListBase())| is true.)
  def __iter__(self):
    return self.elements.__iter__()

  def __eq__(self, other):
    return type(self) == type(other) and \
           len(self.elements) == len(other.elements) and \
           all(self.elements[i] == other.elements[i] \
                   for i in xrange(len(self.elements)))

  # Implement this so that on failure, we get slightly more sensible output.
  def __repr__(self):
    return self.__class__.__name__ + "([" + \
           ", ".join([repr(elem) for elem in self.elements]) + "])"

  def Append(self, item):
    assert isinstance(item, self._list_item_type)
    self.elements.append(item)
    self._UpdateFilenameAndLineno()

  def _UpdateFilenameAndLineno(self):
    if self.elements:
      self.filename = self.elements[0].filename
      self.lineno = self.elements[0].lineno


################################################################################


class Attribute(NodeBase):
  """Represents an attribute."""

  def __init__(self, key, value, **kwargs):
    assert isinstance(key, str)
    NodeBase.__init__(self, **kwargs)
    self.key = key
    self.value = value

  def __eq__(self, other):
    return self.key == other.key and self.value == other.value


class AttributeList(NodeListBase):
  """Represents a list attributes."""

  _list_item_type = Attribute


class Module(NodeBase):
  """Represents a module statement."""

  def __init__(self, name, attribute_list, **kwargs):
    # |name| is either none or a "wrapped identifier".
    assert name is None or isinstance(name, tuple)
    assert attribute_list is None or isinstance(attribute_list, AttributeList)
    NodeBase.__init__(self, **kwargs)
    self.name = name
    self.attribute_list = attribute_list

  def __eq__(self, other):
    return self.name == other.name and \
           self.attribute_list == other.attribute_list


class Ordinal(NodeBase):
  """Represents an ordinal value labeling, e.g., a struct field."""

  def __init__(self, value, **kwargs):
    assert value is None or isinstance(value, int)
    NodeBase.__init__(self, **kwargs)
    self.value = value

  def __eq__(self, other):
    return self.value == other.value


class Parameter(NodeBase):
  """Represents a method request or response parameter."""

  def __init__(self, typename, name, ordinal, **kwargs):
    assert isinstance(ordinal, Ordinal)
    NodeBase.__init__(self, **kwargs)
    self.typename = typename
    self.name = name
    self.ordinal = ordinal

  def __eq__(self, other):
    return self.typename == other.typename and \
           self.name == other.name and \
           self.ordinal == other.ordinal


class ParameterList(NodeListBase):
  """Represents a list of (method request or response) parameters."""

  _list_item_type = Parameter
