# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Contains logic to parse .isolate files.

This module doesn't touch the file system. It's the job of the client code to do
I/O on behalf of this module.

See more information at
  https://code.google.com/p/swarming/wiki/IsolateDesign
  https://code.google.com/p/swarming/wiki/IsolateUserGuide
"""

import ast
import copy
import itertools
import logging
import os
import re

import isolateserver

from utils import short_expression_finder

# Files that should be 0-length when mapped.
KEY_TOUCHED = 'isolate_dependency_touched'
# Files that should be tracked by the build tool.
KEY_TRACKED = 'isolate_dependency_tracked'
# Files that should not be tracked by the build tool.
KEY_UNTRACKED = 'isolate_dependency_untracked'

# Valid variable name.
VALID_VARIABLE = '[A-Za-z_][A-Za-z_0-9]*'


def determine_root_dir(relative_root, infiles):
  """For a list of infiles, determines the deepest root directory that is
  referenced indirectly.

  All arguments must be using os.path.sep.
  """
  # The trick used to determine the root directory is to look at "how far" back
  # up it is looking up.
  deepest_root = relative_root
  for i in infiles:
    x = relative_root
    while i.startswith('..' + os.path.sep):
      i = i[3:]
      assert not i.startswith(os.path.sep)
      x = os.path.dirname(x)
    if deepest_root.startswith(x):
      deepest_root = x
  logging.debug(
      'determine_root_dir(%s, %d files) -> %s' % (
          relative_root, len(infiles), deepest_root))
  return deepest_root


def replace_variable(part, variables):
  m = re.match(r'<\((' + VALID_VARIABLE + ')\)', part)
  if m:
    if m.group(1) not in variables:
      raise isolateserver.ConfigError(
        'Variable "%s" was not found in %s.\nDid you forget to specify '
        '--path-variable?' % (m.group(1), variables))
    return variables[m.group(1)]
  return part


def eval_variables(item, variables):
  """Replaces the .isolate variables in a string item.

  Note that the .isolate format is a subset of the .gyp dialect.
  """
  return ''.join(
      replace_variable(p, variables)
      for p in re.split(r'(<\(' + VALID_VARIABLE + '\))', item))


def split_touched(files):
  """Splits files that are touched vs files that are read."""
  tracked = []
  touched = []
  for f in files:
    if f.size:
      tracked.append(f)
    else:
      touched.append(f)
  return tracked, touched


def pretty_print(variables, stdout):
  """Outputs a gyp compatible list from the decoded variables.

  Similar to pprint.print() but with NIH syndrome.
  """
  # Order the dictionary keys by these keys in priority.
  ORDER = (
      'variables', 'condition', 'command', 'relative_cwd', 'read_only',
      KEY_TRACKED, KEY_UNTRACKED)

  def sorting_key(x):
    """Gives priority to 'most important' keys before the others."""
    if x in ORDER:
      return str(ORDER.index(x))
    return x

  def loop_list(indent, items):
    for item in items:
      if isinstance(item, basestring):
        stdout.write('%s\'%s\',\n' % (indent, item))
      elif isinstance(item, dict):
        stdout.write('%s{\n' % indent)
        loop_dict(indent + '  ', item)
        stdout.write('%s},\n' % indent)
      elif isinstance(item, list):
        # A list inside a list will write the first item embedded.
        stdout.write('%s[' % indent)
        for index, i in enumerate(item):
          if isinstance(i, basestring):
            stdout.write(
                '\'%s\', ' % i.replace('\\', '\\\\').replace('\'', '\\\''))
          elif isinstance(i, dict):
            stdout.write('{\n')
            loop_dict(indent + '  ', i)
            if index != len(item) - 1:
              x = ', '
            else:
              x = ''
            stdout.write('%s}%s' % (indent, x))
          else:
            assert False
        stdout.write('],\n')
      else:
        assert False

  def loop_dict(indent, items):
    for key in sorted(items, key=sorting_key):
      item = items[key]
      stdout.write("%s'%s': " % (indent, key))
      if isinstance(item, dict):
        stdout.write('{\n')
        loop_dict(indent + '  ', item)
        stdout.write(indent + '},\n')
      elif isinstance(item, list):
        stdout.write('[\n')
        loop_list(indent + '  ', item)
        stdout.write(indent + '],\n')
      elif isinstance(item, basestring):
        stdout.write(
            '\'%s\',\n' % item.replace('\\', '\\\\').replace('\'', '\\\''))
      elif item in (True, False, None):
        stdout.write('%s\n' % item)
      else:
        assert False, item

  stdout.write('{\n')
  loop_dict('  ', variables)
  stdout.write('}\n')


def print_all(comment, data, stream):
  """Prints a complete .isolate file and its top-level file comment into a
  stream.
  """
  if comment:
    stream.write(comment)
  pretty_print(data, stream)


def union(lhs, rhs):
  """Merges two compatible datastructures composed of dict/list/set."""
  assert lhs is not None or rhs is not None
  if lhs is None:
    return copy.deepcopy(rhs)
  if rhs is None:
    return copy.deepcopy(lhs)
  assert type(lhs) == type(rhs), (lhs, rhs)
  if hasattr(lhs, 'union'):
    # Includes set, ConfigSettings and Configs.
    return lhs.union(rhs)
  if isinstance(lhs, dict):
    return dict((k, union(lhs.get(k), rhs.get(k))) for k in set(lhs).union(rhs))
  elif isinstance(lhs, list):
    # Do not go inside the list.
    return lhs + rhs
  assert False, type(lhs)


def extract_comment(content):
  """Extracts file level comment."""
  out = []
  for line in content.splitlines(True):
    if line.startswith('#'):
      out.append(line)
    else:
      break
  return ''.join(out)


def eval_content(content):
  """Evaluates a python file and return the value defined in it.

  Used in practice for .isolate files.
  """
  globs = {'__builtins__': None}
  locs = {}
  try:
    value = eval(content, globs, locs)
  except TypeError as e:
    e.args = list(e.args) + [content]
    raise
  assert locs == {}, locs
  assert globs == {'__builtins__': None}, globs
  return value


def match_configs(expr, config_variables, all_configs):
  """Returns the configs from |all_configs| that match the |expr|, where
  the elements of |all_configs| are tuples of values for the |config_variables|.
  Example:
  >>> match_configs(expr = "(foo==1 or foo==2) and bar=='b'",
                    config_variables = ["foo", "bar"],
                    all_configs = [(1, 'a'), (1, 'b'), (2, 'a'), (2, 'b')])
  [(1, 'b'), (2, 'b')]
  """
  return [
    config for config in all_configs
    if eval(expr, dict(zip(config_variables, config)))
  ]


def verify_variables(variables):
  """Verifies the |variables| dictionary is in the expected format."""
  VALID_VARIABLES = [
    KEY_TOUCHED,
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  ]
  assert isinstance(variables, dict), variables
  assert set(VALID_VARIABLES).issuperset(set(variables)), variables.keys()
  for name, value in variables.iteritems():
    if name == 'read_only':
      assert value in (True, False, None), value
    else:
      assert isinstance(value, list), value
      assert all(isinstance(i, basestring) for i in value), value


def verify_ast(expr, variables_and_values):
  """Verifies that |expr| is of the form
  expr ::= expr ( "or" | "and" ) expr
         | identifier "==" ( string | int )
  Also collects the variable identifiers and string/int values in the dict
  |variables_and_values|, in the form {'var': set([val1, val2, ...]), ...}.
  """
  assert isinstance(expr, (ast.BoolOp, ast.Compare))
  if isinstance(expr, ast.BoolOp):
    assert isinstance(expr.op, (ast.And, ast.Or))
    for subexpr in expr.values:
      verify_ast(subexpr, variables_and_values)
  else:
    assert isinstance(expr.left.ctx, ast.Load)
    assert len(expr.ops) == 1
    assert isinstance(expr.ops[0], ast.Eq)
    var_values = variables_and_values.setdefault(expr.left.id, set())
    rhs = expr.comparators[0]
    assert isinstance(rhs, (ast.Str, ast.Num))
    var_values.add(rhs.n if isinstance(rhs, ast.Num) else rhs.s)


def verify_condition(condition, variables_and_values):
  """Verifies the |condition| dictionary is in the expected format.
  See verify_ast() for the meaning of |variables_and_values|.
  """
  VALID_INSIDE_CONDITION = ['variables']
  assert isinstance(condition, list), condition
  assert len(condition) == 2, condition
  expr, then = condition

  test_ast = compile(expr, '<condition>', 'eval', ast.PyCF_ONLY_AST)
  verify_ast(test_ast.body, variables_and_values)

  assert isinstance(then, dict), then
  assert set(VALID_INSIDE_CONDITION).issuperset(set(then)), then.keys()
  if not 'variables' in then:
    raise isolateserver.ConfigError('Missing \'variables\' in condition %s' %
        condition)
  verify_variables(then['variables'])


def verify_root(value, variables_and_values):
  """Verifies that |value| is the parsed form of a valid .isolate file.
  See verify_ast() for the meaning of |variables_and_values|.
  """
  VALID_ROOTS = ['includes', 'conditions']
  assert isinstance(value, dict), value
  assert set(VALID_ROOTS).issuperset(set(value)), value.keys()

  includes = value.get('includes', [])
  assert isinstance(includes, list), includes
  for include in includes:
    assert isinstance(include, basestring), include

  conditions = value.get('conditions', [])
  assert isinstance(conditions, list), conditions
  for condition in conditions:
    verify_condition(condition, variables_and_values)


def remove_weak_dependencies(values, key, item, item_configs):
  """Removes any configs from this key if the item is already under a
  strong key.
  """
  if key == KEY_TOUCHED:
    item_configs = set(item_configs)
    for stronger_key in (KEY_TRACKED, KEY_UNTRACKED):
      try:
        item_configs -= values[stronger_key][item]
      except KeyError:
        pass

  return item_configs


def remove_repeated_dependencies(folders, key, item, item_configs):
  """Removes any configs from this key if the item is in a folder that is
  already included."""

  if key in (KEY_UNTRACKED, KEY_TRACKED, KEY_TOUCHED):
    item_configs = set(item_configs)
    for (folder, configs) in folders.iteritems():
      if folder != item and item.startswith(folder):
        item_configs -= configs

  return item_configs


def get_folders(values_dict):
  """Returns a dict of all the folders in the given value_dict."""
  return dict(
    (item, configs) for (item, configs) in values_dict.iteritems()
    if item.endswith('/')
  )


def invert_map(variables):
  """Converts {config: {deptype: list(depvals)}} to
  {deptype: {depval: set(configs)}}.
  """
  KEYS = (
    KEY_TOUCHED,
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  )
  out = dict((key, {}) for key in KEYS)
  for config, values in variables.iteritems():
    for key in KEYS:
      if key == 'command':
        items = [tuple(values[key])] if key in values else []
      elif key == 'read_only':
        items = [values[key]] if key in values else []
      else:
        assert key in (KEY_TOUCHED, KEY_TRACKED, KEY_UNTRACKED)
        items = values.get(key, [])
      for item in items:
        out[key].setdefault(item, set()).add(config)
  return out


def reduce_inputs(values):
  """Reduces the output of invert_map() to the strictest minimum list.

  Looks at each individual file and directory, maps where they are used and
  reconstructs the inverse dictionary.

  Returns the minimized dictionary.
  """
  KEYS = (
    KEY_TOUCHED,
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  )

  # Folders can only live in KEY_UNTRACKED.
  folders = get_folders(values.get(KEY_UNTRACKED, {}))

  out = dict((key, {}) for key in KEYS)
  for key in KEYS:
    for item, item_configs in values.get(key, {}).iteritems():
      item_configs = remove_weak_dependencies(values, key, item, item_configs)
      item_configs = remove_repeated_dependencies(
          folders, key, item, item_configs)
      if item_configs:
        out[key][item] = item_configs
  return out


def convert_map_to_isolate_dict(values, config_variables):
  """Regenerates back a .isolate configuration dict from files and dirs
  mappings generated from reduce_inputs().
  """
  # Gather a list of configurations for set inversion later.
  all_mentioned_configs = set()
  for configs_by_item in values.itervalues():
    for configs in configs_by_item.itervalues():
      all_mentioned_configs.update(configs)

  # Invert the mapping to make it dict first.
  conditions = {}
  for key in values:
    for item, configs in values[key].iteritems():
      then = conditions.setdefault(frozenset(configs), {})
      variables = then.setdefault('variables', {})

      if item in (True, False):
        # One-off for read_only.
        variables[key] = item
      else:
        assert item
        if isinstance(item, tuple):
          # One-off for command.
          # Do not merge lists and do not sort!
          # Note that item is a tuple.
          assert key not in variables
          variables[key] = list(item)
        else:
          # The list of items (files or dirs). Append the new item and keep
          # the list sorted.
          l = variables.setdefault(key, [])
          l.append(item)
          l.sort()

  if all_mentioned_configs:
    config_values = map(set, zip(*all_mentioned_configs))
    sef = short_expression_finder.ShortExpressionFinder(
        zip(config_variables, config_values))

  conditions = sorted(
      [sef.get_expr(configs), then] for configs, then in conditions.iteritems())
  return {'conditions': conditions}


class ConfigSettings(object):
  """Represents the dependency variables for a single build configuration.
  The structure is immutable.
  """
  def __init__(self, config, values):
    self.config = config
    verify_variables(values)
    self.touched = sorted(values.get(KEY_TOUCHED, []))
    self.tracked = sorted(values.get(KEY_TRACKED, []))
    self.untracked = sorted(values.get(KEY_UNTRACKED, []))
    self.command = values.get('command', [])[:]
    self.read_only = values.get('read_only')

  def union(self, rhs):
    """Merges two config settings together.

    self has priority over rhs for 'command' variable.
    """
    assert not (self.config and rhs.config) or (self.config == rhs.config)
    var = {
      KEY_TOUCHED: sorted(self.touched + rhs.touched),
      KEY_TRACKED: sorted(self.tracked + rhs.tracked),
      KEY_UNTRACKED: sorted(self.untracked + rhs.untracked),
      'command': self.command or rhs.command,
      'read_only': rhs.read_only if self.read_only is None else self.read_only,
    }
    return ConfigSettings(self.config or rhs.config, var)

  def flatten(self):
    out = {}
    if self.command:
      out['command'] = self.command
    if self.touched:
      out[KEY_TOUCHED] = self.touched
    if self.tracked:
      out[KEY_TRACKED] = self.tracked
    if self.untracked:
      out[KEY_UNTRACKED] = self.untracked
    if self.read_only is not None:
      out['read_only'] = self.read_only
    return out


class Configs(object):
  """Represents a processed .isolate file.

  Stores the file in a processed way, split by configuration.
  """
  def __init__(self, file_comment):
    self.file_comment = file_comment
    # The keys of by_config are tuples of values for the configuration
    # variables. The names of the variables (which must be the same for
    # every by_config key) are kept in config_variables. Initially by_config
    # is empty and we don't know what configuration variables will be used,
    # so config_variables also starts out empty. It will be set by the first
    # call to union() or merge_dependencies().
    self.by_config = {}
    self.config_variables = ()

  def union(self, rhs):
    """Adds variables from rhs (a Configs) to the existing variables.
    """
    config_variables = self.config_variables
    if not config_variables:
      config_variables = rhs.config_variables
    else:
      # We can't proceed if this isn't true since we don't know the correct
      # default values for extra variables. The variables are sorted so we
      # don't need to worry about permutations.
      if rhs.config_variables and rhs.config_variables != config_variables:
        raise isolateserver.ConfigError(
            'Variables in merged .isolate files do not match: %r and %r' % (
                config_variables, rhs.config_variables))

    # Takes the first file comment, prefering lhs.
    out = Configs(self.file_comment or rhs.file_comment)
    out.config_variables = config_variables
    for config in set(self.by_config) | set(rhs.by_config):
      out.by_config[config] = union(
          self.by_config.get(config), rhs.by_config.get(config))
    return out

  def merge_dependencies(self, values, config_variables, configs):
    """Adds new dependencies to this object for the given configurations.
    Arguments:
      values: A variables dict as found in a .isolate file, e.g.,
          {KEY_TOUCHED: [...], 'command': ...}.
      config_variables: An ordered list of configuration variables, e.g.,
          ["OS", "chromeos"]. If this object already contains any dependencies,
          the configuration variables must match.
      configs: a list of tuples of values of the configuration variables,
          e.g., [("mac", 0), ("linux", 1)]. The dependencies in |values|
          are added to all of these configurations, and other configurations
          are unchanged.
    """
    if not values:
      return

    if not self.config_variables:
      self.config_variables = config_variables
    else:
      # See comment in Configs.union().
      assert self.config_variables == config_variables

    for config in configs:
      self.by_config[config] = union(
          self.by_config.get(config), ConfigSettings(config, values))

  def flatten(self):
    """Returns a flat dictionary representation of the configuration.
    """
    return dict((k, v.flatten()) for k, v in self.by_config.iteritems())

  def make_isolate_file(self):
    """Returns a dictionary suitable for writing to a .isolate file.
    """
    dependencies_by_config = self.flatten()
    configs_by_dependency = reduce_inputs(invert_map(dependencies_by_config))
    return convert_map_to_isolate_dict(configs_by_dependency,
                                       self.config_variables)


# TODO(benrg): Remove this function when no old-format files are left.
def convert_old_to_new_format(value):
  """Converts from the old .isolate format, which only has one variable (OS),
  always includes 'linux', 'mac' and 'win' in the set of valid values for OS,
  and allows conditions that depend on the set of all OSes, to the new format,
  which allows any set of variables, has no hardcoded values, and only allows
  explicit positive tests of variable values.
  """
  conditions = value.get('conditions', [])
  if 'variables' not in value and all(len(cond) == 2 for cond in conditions):
    return value  # Nothing to change

  def parse_condition(cond):
    m = re.match(r'OS=="(\w+)"\Z', cond[0])
    if not m:
      raise isolateserver.ConfigError('Invalid condition: %s' % cond[0])
    return m.group(1)

  oses = set(map(parse_condition, conditions))
  default_oses = set(['linux', 'mac', 'win'])
  oses = sorted(oses | default_oses)

  def if_not_os(not_os, then):
    expr = ' or '.join('OS=="%s"' % os for os in oses if os != not_os)
    return [expr, then]

  conditions = [
    cond[:2] for cond in conditions if cond[1]
  ] + [
    if_not_os(parse_condition(cond), cond[2])
    for cond in conditions if len(cond) == 3
  ]

  if 'variables' in value:
    conditions.append(if_not_os(None, {'variables': value.pop('variables')}))
  conditions.sort()

  value = value.copy()
  value['conditions'] = conditions
  return value


def load_isolate_as_config(isolate_dir, value, file_comment):
  """Parses one .isolate file and returns a Configs() instance.

  Arguments:
    isolate_dir: only used to load relative includes so it doesn't depend on
                 cwd.
    value: is the loaded dictionary that was defined in the gyp file.
    file_comment: comments found at the top of the file so it can be preserved.

  The expected format is strict, anything diverting from the format below will
  throw an assert:
  {
    'includes': [
      'foo.isolate',
    ],
    'conditions': [
      ['OS=="vms" and foo=42', {
        'variables': {
          'command': [
            ...
          ],
          'isolate_dependency_tracked': [
            ...
          ],
          'isolate_dependency_untracked': [
            ...
          ],
          'read_only': False,
        },
      }],
      ...
    ],
  }
  """
  value = convert_old_to_new_format(value)

  variables_and_values = {}
  verify_root(value, variables_and_values)
  if variables_and_values:
    config_variables, config_values = zip(
        *sorted(variables_and_values.iteritems()))
    all_configs = list(itertools.product(*config_values))
  else:
    config_variables = None
    all_configs = []

  isolate = Configs(file_comment)

  # Add configuration-specific variables.
  for expr, then in value.get('conditions', []):
    configs = match_configs(expr, config_variables, all_configs)
    isolate.merge_dependencies(then['variables'], config_variables, configs)

  # Load the includes. Process them in reverse so the last one take precedence.
  for include in reversed(value.get('includes', [])):
    if os.path.isabs(include):
      raise isolateserver.ConfigError(
          'Failed to load configuration; absolute include path \'%s\'' %
          include)
    included_isolate = os.path.normpath(os.path.join(isolate_dir, include))
    with open(included_isolate, 'r') as f:
      included_isolate = load_isolate_as_config(
          os.path.dirname(included_isolate),
          eval_content(f.read()),
          None)
    isolate = union(isolate, included_isolate)

  return isolate


def load_isolate_for_config(isolate_dir, content, config_variables):
  """Loads the .isolate file and returns the information unprocessed but
  filtered for the specific OS.

  Returns the command, dependencies and read_only flag. The dependencies are
  fixed to use os.path.sep.
  """
  # Load the .isolate file, process its conditions, retrieve the command and
  # dependencies.
  isolate = load_isolate_as_config(isolate_dir, eval_content(content), None)
  try:
    config_name = tuple(
        config_variables[var] for var in isolate.config_variables)
  except KeyError:
    raise isolateserver.ConfigError(
        'These configuration variables were missing from the command line: %s' %
        ', '.join(
            sorted(set(isolate.config_variables) - set(config_variables))))
  config = isolate.by_config.get(config_name)
  if not config:
    raise isolateserver.ConfigError(
        'Failed to load configuration for variable \'%s\' for config(s) \'%s\''
        '\nAvailable configs: %s' %
        (', '.join(isolate.config_variables),
        ', '.join(config_name),
        ', '.join(str(s) for s in isolate.by_config)))
  # Merge tracked and untracked variables, isolate.py doesn't care about the
  # trackability of the variables, only the build tool does.
  dependencies = [
    f.replace('/', os.path.sep) for f in config.tracked + config.untracked
  ]
  touched = [f.replace('/', os.path.sep) for f in config.touched]
  return config.command, dependencies, touched, config.read_only
