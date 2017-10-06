# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import collections

from third_party import schema


# See https://github.com/keleshev/schema for docs how to configure schema.
_GCLIENT_DEPS_SCHEMA = {
    schema.Optional(basestring): schema.Or(
        None,
        basestring,
        {
            # Repo and revision to check out under the path
            # (same as if no dict was used).
            'url': basestring,

            # Optional condition string. The dep will only be processed
            # if the condition evaluates to True.
            schema.Optional('condition'): basestring,
        },
    ),
}

_GCLIENT_HOOKS_SCHEMA = [{
    # Hook action: list of command-line arguments to invoke.
    'action': [basestring],

    # Name of the hook. Doesn't affect operation.
    schema.Optional('name'): basestring,

    # Hook pattern (regex). Originally intended to limit some hooks to run
    # only when files matching the pattern have changed. In practice, with git,
    # gclient runs all the hooks regardless of this field.
    schema.Optional('pattern'): basestring,

    # Working directory where to execute the hook.
    schema.Optional('cwd'): basestring,

    # Optional condition string. The hook will only be run
    # if the condition evaluates to True.
    schema.Optional('condition'): basestring,
}]

_GCLIENT_SCHEMA = schema.Schema({
    # List of host names from which dependencies are allowed (whitelist).
    # NOTE: when not present, all hosts are allowed.
    # NOTE: scoped to current DEPS file, not recursive.
    schema.Optional('allowed_hosts'): [schema.Optional(basestring)],

    # Mapping from paths to repo and revision to check out under that path.
    # Applying this mapping to the on-disk checkout is the main purpose
    # of gclient, and also why the config file is called DEPS.
    #
    # The following functions are allowed:
    #
    #   Var(): allows variable substitution (either from 'vars' dict below,
    #          or command-line override)
    schema.Optional('deps'): _GCLIENT_DEPS_SCHEMA,

    # Similar to 'deps' (see above) - also keyed by OS (e.g. 'linux').
    # Also see 'target_os'.
    schema.Optional('deps_os'): {
        schema.Optional(basestring): _GCLIENT_DEPS_SCHEMA,
    },

    # Path to GN args file to write selected variables.
    schema.Optional('gclient_gn_args_file'): basestring,

    # Subset of variables to write to the GN args file (see above).
    schema.Optional('gclient_gn_args'): [schema.Optional(basestring)],

    # Hooks executed after gclient sync (unless suppressed), or explicitly
    # on gclient hooks. See _GCLIENT_HOOKS_SCHEMA for details.
    # Also see 'pre_deps_hooks'.
    schema.Optional('hooks'): _GCLIENT_HOOKS_SCHEMA,

    # Similar to 'hooks', also keyed by OS.
    schema.Optional('hooks_os'): {
        schema.Optional(basestring): _GCLIENT_HOOKS_SCHEMA
    },

    # Rules which #includes are allowed in the directory.
    # Also see 'skip_child_includes' and 'specific_include_rules'.
    schema.Optional('include_rules'): [schema.Optional(basestring)],

    # Hooks executed before processing DEPS. See 'hooks' for more details.
    schema.Optional('pre_deps_hooks'): _GCLIENT_HOOKS_SCHEMA,

    # Recursion limit for nested DEPS.
    schema.Optional('recursion'): int,

    # Whitelists deps for which recursion should be enabled.
    schema.Optional('recursedeps'): [
        schema.Optional(schema.Or(
            basestring,
            (basestring, basestring),
            [basestring, basestring]
        )),
    ],

    # Blacklists directories for checking 'include_rules'.
    schema.Optional('skip_child_includes'): [schema.Optional(basestring)],

    # Mapping from paths to include rules specific for that path.
    # See 'include_rules' for more details.
    schema.Optional('specific_include_rules'): {
        schema.Optional(basestring): [basestring]
    },

    # List of additional OS names to consider when selecting dependencies
    # from deps_os.
    schema.Optional('target_os'): [schema.Optional(basestring)],

    # For recursed-upon sub-dependencies, check out their own dependencies
    # relative to the paren't path, rather than relative to the .gclient file.
    schema.Optional('use_relative_paths'): bool,

    # Variables that can be referenced using Var() - see 'deps'.
    schema.Optional('vars'): {
        schema.Optional(basestring): schema.Or(basestring, bool),
    },
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
    elif isinstance(node, ast.Num):
      return node.n
    elif isinstance(node, ast.Tuple):
      return tuple(map(_convert, node.elts))
    elif isinstance(node, ast.List):
      return list(map(_convert, node.elts))
    elif isinstance(node, ast.Dict):
      return collections.OrderedDict(
          (_convert(k), _convert(v))
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
    elif isinstance(node, ast.BinOp) and isinstance(node.op, ast.Mod):
      return _convert(node.left) % _convert(node.right)
    else:
      raise ValueError(
          'unexpected AST node: %s %s (file %r, line %s)' % (
              node, ast.dump(node), filename,
              getattr(node, 'lineno', '<unknown>')))
  return _convert(node_or_string)


def Exec(content, global_scope, local_scope, filename='<unknown>'):
  """Safely execs a set of assignments. Mutates |local_scope|."""
  node_or_string = ast.parse(content, filename=filename, mode='exec')
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

      if target.id in local_scope:
        raise ValueError(
            'invalid assignment: overrides var %r (file %r, line %s)' % (
                target.id, filename, getattr(node, 'lineno', '<unknown>')))

      local_scope[target.id] = value
    else:
      raise ValueError(
          'unexpected AST node: %s %s (file %r, line %s)' % (
              node, ast.dump(node), filename,
              getattr(node, 'lineno', '<unknown>')))

  if isinstance(node_or_string, ast.Module):
    for stmt in node_or_string.body:
      _visit_in_module(stmt)
  else:
    raise ValueError(
        'unexpected AST node: %s %s (file %r, line %s)' % (
            node_or_string,
            ast.dump(node_or_string),
            filename,
            getattr(node_or_string, 'lineno', '<unknown>')))

  _GCLIENT_SCHEMA.validate(local_scope)


def EvaluateCondition(condition, variables, referenced_variables=None):
  """Safely evaluates a boolean condition. Returns the result."""
  if not referenced_variables:
    referenced_variables = set()
  _allowed_names = {'None': None, 'True': True, 'False': False}
  main_node = ast.parse(condition, mode='eval')
  if isinstance(main_node, ast.Expression):
    main_node = main_node.body
  def _convert(node):
    if isinstance(node, ast.Str):
      return node.s
    elif isinstance(node, ast.Name):
      if node.id in referenced_variables:
        raise ValueError(
            'invalid cyclic reference to %r (inside %r)' % (
                node.id, condition))
      elif node.id in _allowed_names:
        return _allowed_names[node.id]
      elif node.id in variables:
        value = variables[node.id]

        # Allow using "native" types, without wrapping everything in strings.
        # Note that schema constraints still apply to variables.
        if not isinstance(value, basestring):
          return value

        # Recursively evaluate the variable reference.
        return EvaluateCondition(
            variables[node.id],
            variables,
            referenced_variables.union([node.id]))
      else:
        # Implicitly convert unrecognized names to strings.
        # If we want to change this, we'll need to explicitly distinguish
        # between arguments for GN to be passed verbatim, and ones to
        # be evaluated.
        return node.id
    elif isinstance(node, ast.BoolOp) and isinstance(node.op, ast.Or):
      if len(node.values) != 2:
        raise ValueError(
            'invalid "or": exactly 2 operands required (inside %r)' % (
                condition))
      left = _convert(node.values[0])
      right = _convert(node.values[1])
      if not isinstance(left, bool):
        raise ValueError(
            'invalid "or" operand %r (inside %r)' % (left, condition))
      if not isinstance(right, bool):
        raise ValueError(
            'invalid "or" operand %r (inside %r)' % (right, condition))
      return left or right
    elif isinstance(node, ast.BoolOp) and isinstance(node.op, ast.And):
      if len(node.values) != 2:
        raise ValueError(
            'invalid "and": exactly 2 operands required (inside %r)' % (
                condition))
      left = _convert(node.values[0])
      right = _convert(node.values[1])
      if not isinstance(left, bool):
        raise ValueError(
            'invalid "and" operand %r (inside %r)' % (left, condition))
      if not isinstance(right, bool):
        raise ValueError(
            'invalid "and" operand %r (inside %r)' % (right, condition))
      return left and right
    elif isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not):
      value = _convert(node.operand)
      if not isinstance(value, bool):
        raise ValueError(
            'invalid "not" operand %r (inside %r)' % (value, condition))
      return not value
    elif isinstance(node, ast.Compare):
      if len(node.ops) != 1:
        raise ValueError(
            'invalid compare: exactly 1 operator required (inside %r)' % (
                condition))
      if len(node.comparators) != 1:
        raise ValueError(
            'invalid compare: exactly 1 comparator required (inside %r)' % (
                condition))

      left = _convert(node.left)
      right = _convert(node.comparators[0])

      if isinstance(node.ops[0], ast.Eq):
        return left == right
      if isinstance(node.ops[0], ast.NotEq):
        return left != right

      raise ValueError(
          'unexpected operator: %s %s (inside %r)' % (
              node.ops[0], ast.dump(node), condition))
    else:
      raise ValueError(
          'unexpected AST node: %s %s (inside %r)' % (
              node, ast.dump(node), condition))
  return _convert(main_node)
