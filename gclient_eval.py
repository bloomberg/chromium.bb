# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast


def _gclient_eval(node_or_string, global_scope, filename='<unknown>'):
  """Safely evaluates a single expression. Returns the result."""
  _allowed_names = {'None': None, 'True': True, 'False': False}
  if isinstance(node_or_string, basestring):
    node_or_string = ast.parse(node_or_string, filename=filename, mode='eval')
  if isinstance(node_or_string, ast.Expression):
    node_or_string = node_or_string.body
  def _convert(node):
    if isinstance(node, ast.Str):
      return node.s
    elif isinstance(node, ast.Tuple):
      return tuple(map(_convert, node.elts))
    elif isinstance(node, ast.List):
      return list(map(_convert, node.elts))
    elif isinstance(node, ast.Dict):
      return dict((_convert(k), _convert(v))
                  for k, v in zip(node.keys, node.values))
    elif isinstance(node, ast.Name):
      if node.id not in _allowed_names:
        raise ValueError(
            'invalid name %r (file %r, line %s)' % (
                node.id, filename, getattr(node, 'lineno', '<unknown>')))
      return _allowed_names[node.id]
    elif isinstance(node, ast.Call):
      if not isinstance(node.func, ast.Name):
        raise ValueError(
            'invalid call: func should be a name (file %r, line %s)' % (
                filename, getattr(node, 'lineno', '<unknown>')))
      if node.keywords or node.starargs or node.kwargs:
        raise ValueError(
            'invalid call: use only regular args (file %r, line %s)' % (
                filename, getattr(node, 'lineno', '<unknown>')))
      args = map(_convert, node.args)
      return global_scope[node.func.id](*args)
    elif isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
      return _convert(node.left) + _convert(node.right)
    else:
      raise ValueError(
          'unexpected AST node: %s (file %r, line %s)' % (
              node, filename, getattr(node, 'lineno', '<unknown>')))
  return _convert(node_or_string)


def _gclient_exec(node_or_string, global_scope, filename='<unknown>'):
  """Safely execs a set of assignments. Returns resulting scope."""
  result_scope = {}

  if isinstance(node_or_string, basestring):
    node_or_string = ast.parse(node_or_string, filename=filename, mode='exec')
  if isinstance(node_or_string, ast.Expression):
    node_or_string = node_or_string.body

  def _visit_in_module(node):
    if isinstance(node, ast.Assign):
      if len(node.targets) != 1:
        raise ValueError(
            'invalid assignment: use exactly one target (file %r, line %s)' % (
                filename, getattr(node, 'lineno', '<unknown>')))
      target = node.targets[0]
      if not isinstance(target, ast.Name):
        raise ValueError(
            'invalid assignment: target should be a name (file %r, line %s)' % (
                filename, getattr(node, 'lineno', '<unknown>')))
      value = _gclient_eval(node.value, global_scope, filename=filename)

      if target.id in result_scope:
        raise ValueError(
            'invalid assignment: overrides var %r (file %r, line %s)' % (
                target.id, filename, getattr(node, 'lineno', '<unknown>')))

      result_scope[target.id] = value
    else:
      raise ValueError(
          'unexpected AST node: %s (file %r, line %s)' % (
              node, filename, getattr(node, 'lineno', '<unknown>')))

  if isinstance(node_or_string, ast.Module):
    for stmt in node_or_string.body:
      _visit_in_module(stmt)
  else:
    raise ValueError(
        'unexpected AST node: %s (file %r, line %s)' % (
            node_or_string,
            filename,
            getattr(node_or_string, 'lineno', '<unknown>')))

  return result_scope


class CheckFailure(Exception):
  """Contains details of a check failure."""
  def __init__(self, msg, path, exp, act):
    super(CheckFailure, self).__init__(msg)
    self.path = path
    self.exp = exp
    self.act = act


def Check(content, path, global_scope, expected_scope):
  """Cross-checks the old and new gclient eval logic.

  Safely execs |content| (backed by file |path|) using |global_scope|,
  and compares with |expected_scope|.

  Throws CheckFailure if any difference between |expected_scope| and scope
  returned by new gclient eval code is detected.
  """
  def fail(prefix, exp, act):
    raise CheckFailure(
        'gclient check for %s:  %s exp %s, got %s' % (
            path, prefix, repr(exp), repr(act)), prefix, exp, act)

  def compare(expected, actual, var_path, actual_scope):
    if isinstance(expected, dict):
      exp = set(expected.keys())
      act = set(actual.keys())
      if exp != act:
        fail(var_path, exp, act)
      for k in expected:
        compare(expected[k], actual[k], var_path + '["%s"]' % k, actual_scope)
      return
    elif isinstance(expected, list):
      exp = len(expected)
      act = len(actual)
      if exp != act:
        fail('len(%s)' % var_path, expected_scope, actual_scope)
      for i in range(exp):
        compare(expected[i], actual[i], var_path + '[%d]' % i, actual_scope)
    else:
      if expected != actual:
        fail(var_path, expected_scope, actual_scope)

  result_scope = _gclient_exec(content, global_scope, filename=path)

  compare(expected_scope, result_scope, '', result_scope)
