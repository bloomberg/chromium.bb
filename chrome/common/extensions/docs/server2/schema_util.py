# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict, Mapping
import traceback

from third_party.json_schema_compiler import json_parse, idl_schema, idl_parser


def RemoveNoDocs(item):
  '''Removes nodes that should not be rendered from an API schema.
  '''
  if json_parse.IsDict(item):
    if item.get('nodoc', False):
      return True
    for key, value in item.items():
      if RemoveNoDocs(value):
        del item[key]
  elif type(item) == list:
    to_remove = []
    for i in item:
      if RemoveNoDocs(i):
        to_remove.append(i)
    for i in to_remove:
      item.remove(i)
  return False


def DetectInlineableTypes(schema):
  '''Look for documents that are only referenced once and mark them as inline.
  Actual inlining is done by _InlineDocs.
  '''
  if not schema.get('types'):
    return

  ignore = frozenset(('value', 'choices'))
  refcounts = defaultdict(int)
  # Use an explicit stack instead of recursion.
  stack = [schema]

  while stack:
    node = stack.pop()
    if isinstance(node, list):
      stack.extend(node)
    elif isinstance(node, Mapping):
      if '$ref' in node:
        refcounts[node['$ref']] += 1
      stack.extend(v for k, v in node.iteritems() if k not in ignore)

  for type_ in schema['types']:
    if not 'noinline_doc' in type_:
      if refcounts[type_['id']] == 1:
        type_['inline_doc'] = True


def InlineDocs(schema, retain_inlined_types):
  '''Replace '$ref's that refer to inline_docs with the json for those docs.
  If |retain_inlined_types| is False, then the inlined nodes are removed
  from the schema.
  '''
  types = schema.get('types')
  if types is None:
    return

  inline_docs = {}
  types_without_inline_doc = []

  # Gather the types with inline_doc.
  for type_ in types:
    if type_.get('inline_doc'):
      inline_docs[type_['id']] = type_
      if not retain_inlined_types:
        for k in ('description', 'id', 'inline_doc'):
          type_.pop(k, None)
    else:
      types_without_inline_doc.append(type_)
  if not retain_inlined_types:
    schema['types'] = types_without_inline_doc

  def apply_inline(node):
    if isinstance(node, list):
      for i in node:
        apply_inline(i)
    elif isinstance(node, Mapping):
      ref = node.get('$ref')
      if ref and ref in inline_docs:
        node.update(inline_docs[ref])
        del node['$ref']
      for k, v in node.iteritems():
        apply_inline(v)

  apply_inline(schema)


def ProcessSchema(path, file_data, retain_inlined_types=False):
  '''Parses |file_data| using a method determined by checking the
  extension of the file at the given |path|. Then, trims 'nodoc' and if
  |retain_inlined_types| is given and False, removes inlineable types from
  the parsed schema data.
  '''
  def trim_and_inline(schema, is_idl=False):
    '''Modifies an API schema in place by removing nodes that shouldn't be
    documented and inlining schema types that are only referenced once.
    '''
    if RemoveNoDocs(schema):
      # A return of True signifies that the entire schema should not be
      # documented. Otherwise, only nodes that request 'nodoc' are removed.
      return None
    if is_idl:
      DetectInlineableTypes(schema)
    InlineDocs(schema, retain_inlined_types)
    return schema

  if path.endswith('.idl'):
    idl = idl_schema.IDLSchema(idl_parser.IDLParser().ParseData(file_data))
    # Wrap the result in a list so that it behaves like JSON API data.
    return [trim_and_inline(idl.process()[0], is_idl=True)]

  try:
    schemas = json_parse.Parse(file_data)
  except:
    raise ValueError('Cannot parse "%s" as JSON:\n%s' %
                     (path, traceback.format_exc()))
  for schema in schemas:
    # Schemas could consist of one API schema (data for a specific API file)
    # or multiple (data from extension_api.json).
    trim_and_inline(schema)
  return schemas
