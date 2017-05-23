# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast

from third_party import schema


# See https://github.com/keleshev/schema for docs how to configure schema.
_GCLIENT_HOOKS_SCHEMA = [{
    # Hook action: list of command-line arguments to invoke.
    'action': [basestring],

    # Name of the hook. Doesn't affect operation.
    schema.Optional('name'): basestring,

    # Hook pattern (regex). Originally intended to limit some hooks to run
    # only when files matching the pattern have changed. In practice, with git,
    # gclient runs all the hooks regardless of this field.
    schema.Optional('pattern'): basestring,
}]

_GCLIENT_SCHEMA = schema.Schema({
    # List of host names from which dependencies are allowed (whitelist).
    # NOTE: when not present, all hosts are allowed.
    # NOTE: scoped to current DEPS file, not recursive.
    schema.Optional('allowed_hosts'): [basestring],

    # Mapping from paths to repo and revision to check out under that path.
    # Applying this mapping to the on-disk checkout is the main purpose
    # of gclient, and also why the config file is called DEPS.
    #
    # The following functions are allowed:
    #
    #   Var(): allows variable substitution (either from 'vars' dict below,
    #          or command-line override)
    schema.Optional('deps'): {schema.Optional(basestring): basestring},

    # Similar to 'deps' (see above) - also keyed by OS (e.g. 'linux').
    schema.Optional('deps_os'): {basestring: {basestring: basestring}},

    # Hooks executed after gclient sync (unless suppressed), or explicitly
    # on gclient hooks. See _GCLIENT_HOOKS_SCHEMA for details.
    # Also see 'pre_deps_hooks'.
    schema.Optional('hooks'): _GCLIENT_HOOKS_SCHEMA,

    # Similar to 'hooks', also keyed by OS.
    schema.Optional('hooks_os'): {basestring: _GCLIENT_HOOKS_SCHEMA},

    # Rules which #includes are allowed in the directory.
    # Also see 'skip_child_includes' and 'specific_include_rules'.
    schema.Optional('include_rules'): [basestring],

    # Hooks executed before processing DEPS. See 'hooks' for more details.
    schema.Optional('pre_deps_hooks'): _GCLIENT_HOOKS_SCHEMA,

    # Whitelists deps for which recursion should be enabled.
    schema.Optional('recursedeps'): [
        schema.Or(basestring, (basestring, basestring))
    ],

    # Blacklists directories for checking 'include_rules'.
    schema.Optional('skip_child_includes'): [basestring],

    # Mapping from paths to include rules specific for that path.
    # See 'include_rules' for more details.
    schema.Optional('specific_include_rules'): {basestring: [basestring]},

    # For recursed-upon sub-dependencies, check out their own dependencies
    # relative to the paren't path, rather than relative to the .gclient file.
    schema.Optional('use_relative_paths'): bool,

    # Variables that can be referenced using Var() - see 'deps'.
    schema.Optional('vars'): {basestring: basestring},
})


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

  _GCLIENT_SCHEMA.validate(result_scope)
