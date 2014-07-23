#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Front end tool to operate on .isolate files.

This includes creating, merging or compiling them to generate a .isolated file.

See more information at
  https://code.google.com/p/swarming/wiki/IsolateDesign
  https://code.google.com/p/swarming/wiki/IsolateUserGuide
"""
# Run ./isolate.py --help for more detailed information.

__version__ = '0.3.2'

import datetime
import logging
import optparse
import os
import posixpath
import re
import subprocess
import sys

import auth
import isolate_format
import isolateserver
import run_isolated
import trace_inputs

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import file_path
from utils import tools


class ExecutionError(Exception):
  """A generic error occurred."""
  def __str__(self):
    return self.args[0]


### Path handling code.


def expand_directories_and_symlinks(indir, infiles, blacklist,
                                    follow_symlinks, ignore_broken_items):
  """Expands the directories and the symlinks, applies the blacklist and
  verifies files exist.

  Files are specified in os native path separator.
  """
  outfiles = []
  for relfile in infiles:
    try:
      outfiles.extend(
          isolateserver.expand_directory_and_symlink(
              indir, relfile, blacklist, follow_symlinks))
    except isolateserver.MappingError as e:
      if ignore_broken_items:
        logging.info('warning: %s', e)
      else:
        raise
  return outfiles


def recreate_tree(outdir, indir, infiles, action, as_hash):
  """Creates a new tree with only the input files in it.

  Arguments:
    outdir:    Output directory to create the files in.
    indir:     Root directory the infiles are based in.
    infiles:   dict of files to map from |indir| to |outdir|.
    action:    One of accepted action of run_isolated.link_file().
    as_hash:   Output filename is the hash instead of relfile.
  """
  logging.info(
      'recreate_tree(outdir=%s, indir=%s, files=%d, action=%s, as_hash=%s)' %
      (outdir, indir, len(infiles), action, as_hash))

  assert os.path.isabs(outdir) and outdir == os.path.normpath(outdir), outdir
  if not os.path.isdir(outdir):
    logging.info('Creating %s' % outdir)
    os.makedirs(outdir)

  for relfile, metadata in infiles.iteritems():
    infile = os.path.join(indir, relfile)
    if as_hash:
      # Do the hashtable specific checks.
      if 'l' in metadata:
        # Skip links when storing a hashtable.
        continue
      outfile = os.path.join(outdir, metadata['h'])
      if os.path.isfile(outfile):
        # Just do a quick check that the file size matches. No need to stat()
        # again the input file, grab the value from the dict.
        if not 's' in metadata:
          raise isolateserver.MappingError(
              'Misconfigured item %s: %s' % (relfile, metadata))
        if metadata['s'] == os.stat(outfile).st_size:
          continue
        else:
          logging.warn('Overwritting %s' % metadata['h'])
          os.remove(outfile)
    else:
      outfile = os.path.join(outdir, relfile)
      outsubdir = os.path.dirname(outfile)
      if not os.path.isdir(outsubdir):
        os.makedirs(outsubdir)

    # TODO(csharp): Fix crbug.com/150823 and enable the touched logic again.
    # if metadata.get('T') == True:
    #   open(outfile, 'ab').close()
    if 'l' in metadata:
      pointed = metadata['l']
      logging.debug('Symlink: %s -> %s' % (outfile, pointed))
      # symlink doesn't exist on Windows.
      os.symlink(pointed, outfile)  # pylint: disable=E1101
    else:
      run_isolated.link_file(outfile, infile, action)


### Variable stuff.


def _normalize_path_variable(cwd, relative_base_dir, key, value):
  """Normalizes a path variable into a relative directory.
  """
  # Variables could contain / or \ on windows. Always normalize to
  # os.path.sep.
  x = os.path.join(cwd, value.strip().replace('/', os.path.sep))
  normalized = file_path.get_native_path_case(os.path.normpath(x))
  if not os.path.isdir(normalized):
    raise ExecutionError('%s=%s is not a directory' % (key, normalized))

  # All variables are relative to the .isolate file.
  normalized = os.path.relpath(normalized, relative_base_dir)
  logging.debug(
      'Translated variable %s from %s to %s', key, value, normalized)
  return normalized


def normalize_path_variables(cwd, path_variables, relative_base_dir):
  """Processes path variables as a special case and returns a copy of the dict.

  For each 'path' variable: first normalizes it based on |cwd|, verifies it
  exists then sets it as relative to relative_base_dir.
  """
  logging.info(
      'normalize_path_variables(%s, %s, %s)', cwd, path_variables,
      relative_base_dir)
  assert isinstance(cwd, unicode), cwd
  assert isinstance(relative_base_dir, unicode), relative_base_dir
  relative_base_dir = file_path.get_native_path_case(relative_base_dir)
  return dict(
      (k, _normalize_path_variable(cwd, relative_base_dir, k, v))
      for k, v in path_variables.iteritems())


### Internal state files.


def isolatedfile_to_state(filename):
  """For a '.isolate' file, returns the path to the saved '.state' file."""
  return filename + '.state'


def classify_files(root_dir, tracked, untracked):
  """Converts the list of files into a .isolate 'variables' dictionary.

  Arguments:
  - tracked: list of files names to generate a dictionary out of that should
             probably be tracked.
  - untracked: list of files names that must not be tracked.
  """
  new_tracked = []
  new_untracked = list(untracked)

  def should_be_tracked(filepath):
    """Returns True if it is a file without whitespace in a non-optional
    directory that has no symlink in its path.
    """
    if filepath.endswith('/'):
      return False
    if ' ' in filepath:
      return False
    # Look if any element in the path is a symlink.
    split = filepath.split('/')
    for i in range(len(split)):
      if os.path.islink(os.path.join(root_dir, '/'.join(split[:i+1]))):
        return False
    return True

  for filepath in sorted(tracked):
    if should_be_tracked(filepath):
      new_tracked.append(filepath)
    else:
      # Anything else.
      new_untracked.append(filepath)

  variables = {}
  if new_tracked:
    variables[isolate_format.KEY_TRACKED] = sorted(new_tracked)
  if new_untracked:
    variables[isolate_format.KEY_UNTRACKED] = sorted(new_untracked)
  return variables


def chromium_fix(f, variables):
  """Fixes an isolate dependency with Chromium-specific fixes."""
  # Blacklist logs and other unimportant files.
  # - 'First Run' is not created by the compile but by the test itself.
  # - Skip log in PRODUCT_DIR. Note that these are applied on '/' style path
  #   separator at this point.
  if (re.match(r'^\<\(PRODUCT_DIR\)\/[^\/]+\.log$', f) or
      f == '<(PRODUCT_DIR)/First Run'):
    logging.debug('Ignoring %s', f)
    return None

  EXECUTABLE = re.compile(
      r'^(\<\(PRODUCT_DIR\)\/[^\/\.]+)' +
      re.escape(variables.get('EXECUTABLE_SUFFIX', '')) +
      r'$')
  match = EXECUTABLE.match(f)
  if match:
    return match.group(1) + '<(EXECUTABLE_SUFFIX)'

  if sys.platform == 'darwin':
    # On OSX, the name of the output is dependent on gyp define, it can be
    # 'Google Chrome.app' or 'Chromium.app', same for 'XXX
    # Framework.framework'. Furthermore, they are versioned with a gyp
    # variable.  To lower the complexity of the .isolate file, remove all the
    # individual entries that show up under any of the 4 entries and replace
    # them with the directory itself. Overall, this results in a bit more
    # files than strictly necessary.
    OSX_BUNDLES = (
      '<(PRODUCT_DIR)/Chromium Framework.framework/',
      '<(PRODUCT_DIR)/Chromium.app/',
      '<(PRODUCT_DIR)/Google Chrome Framework.framework/',
      '<(PRODUCT_DIR)/Google Chrome.app/',
    )
    for prefix in OSX_BUNDLES:
      if f.startswith(prefix):
        # Note this result in duplicate values, so the a set() must be used to
        # remove duplicates.
        return prefix
  return f


def generate_simplified(
    tracked, untracked, touched, root_dir, path_variables, extra_variables,
    relative_cwd, trace_blacklist):
  """Generates a clean and complete .isolate 'variables' dictionary.

  Cleans up and extracts only files from within root_dir then processes
  variables and relative_cwd.
  """
  root_dir = os.path.realpath(root_dir)
  logging.info(
      'generate_simplified(%d files, %s, %s, %s, %s)' %
      (len(tracked) + len(untracked) + len(touched),
        root_dir, path_variables, extra_variables, relative_cwd))

  # Preparation work.
  relative_cwd = file_path.cleanup_path(relative_cwd)
  assert not os.path.isabs(relative_cwd), relative_cwd

  # Normalizes to posix path. .isolate files are using posix paths on all OSes
  # for coherency.
  path_variables = dict(
      (k, v.replace(os.path.sep, '/')) for k, v in path_variables.iteritems())
  # Contains normalized path_variables plus extra_variables.
  total_variables = path_variables.copy()
  total_variables.update(extra_variables)

  # Actual work: Process the files.
  # TODO(maruel): if all the files in a directory are in part tracked and in
  # part untracked, the directory will not be extracted. Tracked files should be
  # 'promoted' to be untracked as needed.
  tracked = trace_inputs.extract_directories(
      root_dir, tracked, trace_blacklist)
  untracked = trace_inputs.extract_directories(
      root_dir, untracked, trace_blacklist)
  # touched is not compressed, otherwise it would result in files to be archived
  # that we don't need.

  root_dir_posix = root_dir.replace(os.path.sep, '/')
  def fix(f):
    """Bases the file on the most restrictive variable."""
    # Important, GYP stores the files with / and not \.
    f = f.replace(os.path.sep, '/')
    logging.debug('fix(%s)' % f)
    # If it's not already a variable.
    if not f.startswith('<'):
      # relative_cwd is usually the directory containing the gyp file. It may be
      # empty if the whole directory containing the gyp file is needed.
      # Use absolute paths in case cwd_dir is outside of root_dir.
      # Convert the whole thing to / since it's isolate's speak.
      f = file_path.posix_relpath(
          posixpath.join(root_dir_posix, f),
          posixpath.join(root_dir_posix, relative_cwd)) or './'

      # Use the longest value first.
      for key, value in sorted(
          path_variables.iteritems(), key=lambda x: -len(x[1])):
        if f.startswith(value):
          f = '<(%s)%s' % (key, f[len(value):])
          logging.debug('Converted to %s' % f)
          break
    return f

  def fix_all(items):
    """Reduces the items to convert variables, removes unneeded items, apply
    chromium-specific fixes and only return unique items.
    """
    variables_converted = (fix(f.path) for f in items)
    chromium_fixed = (
        chromium_fix(f, total_variables) for f in variables_converted)
    return set(f for f in chromium_fixed if f)

  tracked = fix_all(tracked)
  untracked = fix_all(untracked)
  touched = fix_all(touched)
  out = classify_files(root_dir, tracked, untracked)
  if touched:
    out[isolate_format.KEY_TOUCHED] = sorted(touched)
  return out


def generate_isolate(
    tracked, untracked, touched, root_dir, path_variables, config_variables,
    extra_variables, relative_cwd, trace_blacklist):
  """Generates a clean and complete .isolate file."""
  dependencies = generate_simplified(
      tracked, untracked, touched, root_dir, path_variables, extra_variables,
      relative_cwd, trace_blacklist)
  config_variable_names, config_values = zip(
      *sorted(config_variables.iteritems()))
  out = isolate_format.Configs(None, config_variable_names)
  out.set_config(
      config_values,
      isolate_format.ConfigSettings(
          dependencies, os.path.abspath(relative_cwd)))
  return out.make_isolate_file()


def chromium_save_isolated(isolated, data, path_variables, algo):
  """Writes one or many .isolated files.

  This slightly increases the cold cache cost but greatly reduce the warm cache
  cost by splitting low-churn files off the master .isolated file. It also
  reduces overall isolateserver memcache consumption.
  """
  slaves = []

  def extract_into_included_isolated(prefix):
    new_slave = {
      'algo': data['algo'],
      'files': {},
      'version': data['version'],
    }
    for f in data['files'].keys():
      if f.startswith(prefix):
        new_slave['files'][f] = data['files'].pop(f)
    if new_slave['files']:
      slaves.append(new_slave)

  # Split test/data/ in its own .isolated file.
  extract_into_included_isolated(os.path.join('test', 'data', ''))

  # Split everything out of PRODUCT_DIR in its own .isolated file.
  if path_variables.get('PRODUCT_DIR'):
    extract_into_included_isolated(path_variables['PRODUCT_DIR'])

  files = []
  for index, f in enumerate(slaves):
    slavepath = isolated[:-len('.isolated')] + '.%d.isolated' % index
    tools.write_json(slavepath, f, True)
    data.setdefault('includes', []).append(
        isolateserver.hash_file(slavepath, algo))
    files.append(os.path.basename(slavepath))

  files.extend(isolateserver.save_isolated(isolated, data))
  return files


class Flattenable(object):
  """Represents data that can be represented as a json file."""
  MEMBERS = ()

  def flatten(self):
    """Returns a json-serializable version of itself.

    Skips None entries.
    """
    items = ((member, getattr(self, member)) for member in self.MEMBERS)
    return dict((member, value) for member, value in items if value is not None)

  @classmethod
  def load(cls, data, *args, **kwargs):
    """Loads a flattened version."""
    data = data.copy()
    out = cls(*args, **kwargs)
    for member in out.MEMBERS:
      if member in data:
        # Access to a protected member XXX of a client class
        # pylint: disable=W0212
        out._load_member(member, data.pop(member))
    if data:
      raise ValueError(
          'Found unexpected entry %s while constructing an object %s' %
            (data, cls.__name__), data, cls.__name__)
    return out

  def _load_member(self, member, value):
    """Loads a member into self."""
    setattr(self, member, value)

  @classmethod
  def load_file(cls, filename, *args, **kwargs):
    """Loads the data from a file or return an empty instance."""
    try:
      out = cls.load(tools.read_json(filename), *args, **kwargs)
      logging.debug('Loaded %s(%s)', cls.__name__, filename)
    except (IOError, ValueError) as e:
      # On failure, loads the default instance.
      out = cls(*args, **kwargs)
      logging.warn('Failed to load %s: %s', filename, e)
    return out


class SavedState(Flattenable):
  """Describes the content of a .state file.

  This file caches the items calculated by this script and is used to increase
  the performance of the script. This file is not loaded by run_isolated.py.
  This file can always be safely removed.

  It is important to note that the 'files' dict keys are using native OS path
  separator instead of '/' used in .isolate file.
  """
  MEMBERS = (
    # Value of sys.platform so that the file is rejected if loaded from a
    # different OS. While this should never happen in practice, users are ...
    # "creative".
    'OS',
    # Algorithm used to generate the hash. The only supported value is at the
    # time of writting 'sha-1'.
    'algo',
    # List of included .isolated files. Used to support/remember 'slave'
    # .isolated files. Relative path to isolated_basedir.
    'child_isolated_files',
    # Cache of the processed command. This value is saved because .isolated
    # files are never loaded by isolate.py so it's the only way to load the
    # command safely.
    'command',
    # GYP variables that are used to generate conditions. The most frequent
    # example is 'OS'.
    'config_variables',
    # GYP variables that will be replaced in 'command' and paths but will not be
    # considered a relative directory.
    'extra_variables',
    # Cache of the files found so the next run can skip hash calculation.
    'files',
    # Path of the original .isolate file. Relative path to isolated_basedir.
    'isolate_file',
    # GYP variables used to generate the .isolated files paths based on path
    # variables. Frequent examples are DEPTH and PRODUCT_DIR.
    'path_variables',
    # If the generated directory tree should be read-only.
    'read_only',
    # Relative cwd to use to start the command.
    'relative_cwd',
    # Root directory the files are mapped from.
    'root_dir',
    # Version of the saved state file format. Any breaking change must update
    # the value.
    'version',
  )

  # Bump this version whenever the saved state changes. It is also keyed on the
  # .isolated file version so any change in the generator will invalidate .state
  # files.
  EXPECTED_VERSION = isolateserver.ISOLATED_FILE_VERSION + '.2'

  def __init__(self, isolated_basedir):
    """Creates an empty SavedState.

    Arguments:
      isolated_basedir: the directory where the .isolated and .isolated.state
          files are saved.
    """
    super(SavedState, self).__init__()
    assert os.path.isabs(isolated_basedir), isolated_basedir
    assert os.path.isdir(isolated_basedir), isolated_basedir
    self.isolated_basedir = isolated_basedir

    # The default algorithm used.
    self.OS = sys.platform
    self.algo = isolateserver.SUPPORTED_ALGOS['sha-1']
    self.child_isolated_files = []
    self.command = []
    self.config_variables = {}
    self.extra_variables = {}
    self.files = {}
    self.isolate_file = None
    self.path_variables = {}
    self.read_only = None
    self.relative_cwd = None
    self.root_dir = None
    self.version = self.EXPECTED_VERSION

  def update_config(self, config_variables):
    """Updates the saved state with only config variables."""
    self.config_variables.update(config_variables)

  def update(self, isolate_file, path_variables, extra_variables):
    """Updates the saved state with new data to keep GYP variables and internal
    reference to the original .isolate file.
    """
    assert os.path.isabs(isolate_file)
    # Convert back to a relative path. On Windows, if the isolate and
    # isolated files are on different drives, isolate_file will stay an absolute
    # path.
    isolate_file = file_path.safe_relpath(isolate_file, self.isolated_basedir)

    # The same .isolate file should always be used to generate the .isolated and
    # .isolated.state.
    assert isolate_file == self.isolate_file or not self.isolate_file, (
        isolate_file, self.isolate_file)
    self.extra_variables.update(extra_variables)
    self.isolate_file = isolate_file
    self.path_variables.update(path_variables)

  def update_isolated(self, command, infiles, touched, read_only, relative_cwd):
    """Updates the saved state with data necessary to generate a .isolated file.

    The new files in |infiles| are added to self.files dict but their hash is
    not calculated here.
    """
    self.command = command
    # Add new files.
    for f in infiles:
      self.files.setdefault(f, {})
    for f in touched:
      self.files.setdefault(f, {})['T'] = True
    # Prune extraneous files that are not a dependency anymore.
    for f in set(self.files).difference(set(infiles).union(touched)):
      del self.files[f]
    if read_only is not None:
      self.read_only = read_only
    self.relative_cwd = relative_cwd

  def to_isolated(self):
    """Creates a .isolated dictionary out of the saved state.

    https://code.google.com/p/swarming/wiki/IsolatedDesign
    """
    def strip(data):
      """Returns a 'files' entry with only the whitelisted keys."""
      return dict((k, data[k]) for k in ('h', 'l', 'm', 's') if k in data)

    out = {
      'algo': isolateserver.SUPPORTED_ALGOS_REVERSE[self.algo],
      'files': dict(
          (filepath, strip(data)) for filepath, data in self.files.iteritems()),
      # The version of the .state file is different than the one of the
      # .isolated file.
      'version': isolateserver.ISOLATED_FILE_VERSION,
    }
    if self.command:
      out['command'] = self.command
    if self.read_only is not None:
      out['read_only'] = self.read_only
    if self.relative_cwd:
      out['relative_cwd'] = self.relative_cwd
    return out

  @property
  def isolate_filepath(self):
    """Returns the absolute path of self.isolate_file."""
    return os.path.normpath(
        os.path.join(self.isolated_basedir, self.isolate_file))

  # Arguments number differs from overridden method
  @classmethod
  def load(cls, data, isolated_basedir):  # pylint: disable=W0221
    """Special case loading to disallow different OS.

    It is not possible to load a .isolated.state files from a different OS, this
    file is saved in OS-specific format.
    """
    out = super(SavedState, cls).load(data, isolated_basedir)
    if data.get('OS') != sys.platform:
      raise isolateserver.ConfigError('Unexpected OS %s', data.get('OS'))

    # Converts human readable form back into the proper class type.
    algo = data.get('algo')
    if not algo in isolateserver.SUPPORTED_ALGOS:
      raise isolateserver.ConfigError('Unknown algo \'%s\'' % out.algo)
    out.algo = isolateserver.SUPPORTED_ALGOS[algo]

    # Refuse the load non-exact version, even minor difference. This is unlike
    # isolateserver.load_isolated(). This is because .isolated.state could have
    # changed significantly even in minor version difference.
    if out.version != cls.EXPECTED_VERSION:
      raise isolateserver.ConfigError(
          'Unsupported version \'%s\'' % out.version)

    # The .isolate file must be valid. If it is not present anymore, zap the
    # value as if it was not noted, so .isolate_file can safely be overriden
    # later.
    if out.isolate_file and not os.path.isfile(out.isolate_filepath):
      out.isolate_file = None
    if out.isolate_file:
      # It could be absolute on Windows if the drive containing the .isolate and
      # the drive containing the .isolated files differ, .e.g .isolate is on
      # C:\\ and .isolated is on D:\\   .
      assert not os.path.isabs(out.isolate_file) or sys.platform == 'win32'
      assert os.path.isfile(out.isolate_filepath), out.isolate_filepath
    return out

  def flatten(self):
    """Makes sure 'algo' is in human readable form."""
    out = super(SavedState, self).flatten()
    out['algo'] = isolateserver.SUPPORTED_ALGOS_REVERSE[out['algo']]
    return out

  def __str__(self):
    def dict_to_str(d):
      return ''.join('\n    %s=%s' % (k, d[k]) for k in sorted(d))

    out = '%s(\n' % self.__class__.__name__
    out += '  command: %s\n' % self.command
    out += '  files: %d\n' % len(self.files)
    out += '  isolate_file: %s\n' % self.isolate_file
    out += '  read_only: %s\n' % self.read_only
    out += '  relative_cwd: %s\n' % self.relative_cwd
    out += '  child_isolated_files: %s\n' % self.child_isolated_files
    out += '  path_variables: %s\n' % dict_to_str(self.path_variables)
    out += '  config_variables: %s\n' % dict_to_str(self.config_variables)
    out += '  extra_variables: %s\n' % dict_to_str(self.extra_variables)
    return out


class CompleteState(object):
  """Contains all the state to run the task at hand."""
  def __init__(self, isolated_filepath, saved_state):
    super(CompleteState, self).__init__()
    assert isolated_filepath is None or os.path.isabs(isolated_filepath)
    self.isolated_filepath = isolated_filepath
    # Contains the data to ease developer's use-case but that is not strictly
    # necessary.
    self.saved_state = saved_state

  @classmethod
  def load_files(cls, isolated_filepath):
    """Loads state from disk."""
    assert os.path.isabs(isolated_filepath), isolated_filepath
    isolated_basedir = os.path.dirname(isolated_filepath)
    return cls(
        isolated_filepath,
        SavedState.load_file(
            isolatedfile_to_state(isolated_filepath), isolated_basedir))

  def load_isolate(
      self, cwd, isolate_file, path_variables, config_variables,
      extra_variables, ignore_broken_items):
    """Updates self.isolated and self.saved_state with information loaded from a
    .isolate file.

    Processes the loaded data, deduce root_dir, relative_cwd.
    """
    # Make sure to not depend on os.getcwd().
    assert os.path.isabs(isolate_file), isolate_file
    isolate_file = file_path.get_native_path_case(isolate_file)
    logging.info(
        'CompleteState.load_isolate(%s, %s, %s, %s, %s, %s)',
        cwd, isolate_file, path_variables, config_variables, extra_variables,
        ignore_broken_items)

    # Config variables are not affected by the paths and must be used to
    # retrieve the paths, so update them first.
    self.saved_state.update_config(config_variables)

    with open(isolate_file, 'r') as f:
      # At that point, variables are not replaced yet in command and infiles.
      # infiles may contain directory entries and is in posix style.
      command, infiles, touched, read_only, isolate_cmd_dir = (
          isolate_format.load_isolate_for_config(
              os.path.dirname(isolate_file), f.read(),
              self.saved_state.config_variables))

    # Processes the variables with the new found relative root. Note that 'cwd'
    # is used when path variables are used.
    path_variables = normalize_path_variables(
        cwd, path_variables, isolate_cmd_dir)
    # Update the rest of the saved state.
    self.saved_state.update(isolate_file, path_variables, extra_variables)

    total_variables = self.saved_state.path_variables.copy()
    total_variables.update(self.saved_state.config_variables)
    total_variables.update(self.saved_state.extra_variables)
    command = [
        isolate_format.eval_variables(i, total_variables) for i in command
    ]

    total_variables = self.saved_state.path_variables.copy()
    total_variables.update(self.saved_state.extra_variables)
    infiles = [
        isolate_format.eval_variables(f, total_variables) for f in infiles
    ]
    touched = [
        isolate_format.eval_variables(f, total_variables) for f in touched
    ]
    # root_dir is automatically determined by the deepest root accessed with the
    # form '../../foo/bar'. Note that path variables must be taken in account
    # too, add them as if they were input files.
    self.saved_state.root_dir = isolate_format.determine_root_dir(
        isolate_cmd_dir, infiles + touched +
        self.saved_state.path_variables.values())
    # The relative directory is automatically determined by the relative path
    # between root_dir and the directory containing the .isolate file,
    # isolate_base_dir.
    relative_cwd = os.path.relpath(isolate_cmd_dir, self.saved_state.root_dir)
    # Now that we know where the root is, check that the path_variables point
    # inside it.
    for k, v in self.saved_state.path_variables.iteritems():
      dest = os.path.join(isolate_cmd_dir, relative_cwd, v)
      if not file_path.path_starts_with(self.saved_state.root_dir, dest):
        raise isolateserver.MappingError(
            'Path variable %s=%r points outside the inferred root directory '
            '%s; %s'
            % (k, v, self.saved_state.root_dir, dest))
    # Normalize the files based to self.saved_state.root_dir. It is important to
    # keep the trailing os.path.sep at that step.
    infiles = [
      file_path.relpath(
          file_path.normpath(os.path.join(isolate_cmd_dir, f)),
          self.saved_state.root_dir)
      for f in infiles
    ]
    touched = [
      file_path.relpath(
          file_path.normpath(os.path.join(isolate_cmd_dir, f)),
          self.saved_state.root_dir)
      for f in touched
    ]
    follow_symlinks = sys.platform != 'win32'
    # Expand the directories by listing each file inside. Up to now, trailing
    # os.path.sep must be kept. Do not expand 'touched'.
    infiles = expand_directories_and_symlinks(
        self.saved_state.root_dir,
        infiles,
        lambda x: re.match(r'.*\.(git|svn|pyc)$', x),
        follow_symlinks,
        ignore_broken_items)

    # If we ignore broken items then remove any missing touched items.
    if ignore_broken_items:
      original_touched_count = len(touched)
      touched = [touch for touch in touched if os.path.exists(touch)]

      if len(touched) != original_touched_count:
        logging.info('Removed %d invalid touched entries',
                     len(touched) - original_touched_count)

    # Finally, update the new data to be able to generate the foo.isolated file,
    # the file that is used by run_isolated.py.
    self.saved_state.update_isolated(
        command, infiles, touched, read_only, relative_cwd)
    logging.debug(self)

  def process_inputs(self, subdir):
    """Updates self.saved_state.files with the files' mode and hash.

    If |subdir| is specified, filters to a subdirectory. The resulting .isolated
    file is tainted.

    See isolateserver.process_input() for more information.
    """
    for infile in sorted(self.saved_state.files):
      if subdir and not infile.startswith(subdir):
        self.saved_state.files.pop(infile)
      else:
        filepath = os.path.join(self.root_dir, infile)
        self.saved_state.files[infile] = isolateserver.process_input(
            filepath,
            self.saved_state.files[infile],
            self.saved_state.read_only,
            self.saved_state.algo)

  def save_files(self):
    """Saves self.saved_state and creates a .isolated file."""
    logging.debug('Dumping to %s' % self.isolated_filepath)
    self.saved_state.child_isolated_files = chromium_save_isolated(
        self.isolated_filepath,
        self.saved_state.to_isolated(),
        self.saved_state.path_variables,
        self.saved_state.algo)
    total_bytes = sum(
        i.get('s', 0) for i in self.saved_state.files.itervalues())
    if total_bytes:
      # TODO(maruel): Stats are missing the .isolated files.
      logging.debug('Total size: %d bytes' % total_bytes)
    saved_state_file = isolatedfile_to_state(self.isolated_filepath)
    logging.debug('Dumping to %s' % saved_state_file)
    tools.write_json(saved_state_file, self.saved_state.flatten(), True)

  @property
  def root_dir(self):
    return self.saved_state.root_dir

  def __str__(self):
    def indent(data, indent_length):
      """Indents text."""
      spacing = ' ' * indent_length
      return ''.join(spacing + l for l in str(data).splitlines(True))

    out = '%s(\n' % self.__class__.__name__
    out += '  root_dir: %s\n' % self.root_dir
    out += '  saved_state: %s)' % indent(self.saved_state, 2)
    return out


def load_complete_state(options, cwd, subdir, skip_update):
  """Loads a CompleteState.

  This includes data from .isolate and .isolated.state files. Never reads the
  .isolated file.

  Arguments:
    options: Options instance generated with OptionParserIsolate. For either
             options.isolate and options.isolated, if the value is set, it is an
             absolute path.
    cwd: base directory to be used when loading the .isolate file.
    subdir: optional argument to only process file in the subdirectory, relative
            to CompleteState.root_dir.
    skip_update: Skip trying to load the .isolate file and processing the
                 dependencies. It is useful when not needed, like when tracing.
  """
  assert not options.isolate or os.path.isabs(options.isolate)
  assert not options.isolated or os.path.isabs(options.isolated)
  cwd = file_path.get_native_path_case(unicode(cwd))
  if options.isolated:
    # Load the previous state if it was present. Namely, "foo.isolated.state".
    # Note: this call doesn't load the .isolate file.
    complete_state = CompleteState.load_files(options.isolated)
  else:
    # Constructs a dummy object that cannot be saved. Useful for temporary
    # commands like 'run'. There is no directory containing a .isolated file so
    # specify the current working directory as a valid directory.
    complete_state = CompleteState(None, SavedState(os.getcwd()))

  if not options.isolate:
    if not complete_state.saved_state.isolate_file:
      if not skip_update:
        raise ExecutionError('A .isolate file is required.')
      isolate = None
    else:
      isolate = complete_state.saved_state.isolate_filepath
  else:
    isolate = options.isolate
    if complete_state.saved_state.isolate_file:
      rel_isolate = file_path.safe_relpath(
          options.isolate, complete_state.saved_state.isolated_basedir)
      if rel_isolate != complete_state.saved_state.isolate_file:
        # This happens if the .isolate file was moved for example. In this case,
        # discard the saved state.
        logging.warning(
            '--isolated %s != %s as saved in %s. Discarding saved state',
            rel_isolate,
            complete_state.saved_state.isolate_file,
            isolatedfile_to_state(options.isolated))
        complete_state = CompleteState(
            options.isolated,
            SavedState(complete_state.saved_state.isolated_basedir))

  if not skip_update:
    # Then load the .isolate and expands directories.
    complete_state.load_isolate(
        cwd, isolate, options.path_variables, options.config_variables,
        options.extra_variables, options.ignore_broken_items)

  # Regenerate complete_state.saved_state.files.
  if subdir:
    subdir = unicode(subdir)
    # This is tricky here. If it is a path, take it from the root_dir. If
    # it is a variable, it must be keyed from the directory containing the
    # .isolate file. So translate all variables first.
    translated_path_variables = dict(
        (k,
          os.path.normpath(os.path.join(complete_state.saved_state.relative_cwd,
            v)))
        for k, v in complete_state.saved_state.path_variables.iteritems())
    subdir = isolate_format.eval_variables(subdir, translated_path_variables)
    subdir = subdir.replace('/', os.path.sep)

  if not skip_update:
    complete_state.process_inputs(subdir)
  return complete_state


def read_trace_as_isolate_dict(complete_state, trace_blacklist):
  """Reads a trace and returns the .isolate dictionary.

  Returns exceptions during the log parsing so it can be re-raised.
  """
  api = trace_inputs.get_api()
  logfile = complete_state.isolated_filepath + '.log'
  if not os.path.isfile(logfile):
    raise ExecutionError(
        'No log file \'%s\' to read, did you forget to \'trace\'?' % logfile)
  try:
    data = api.parse_log(logfile, trace_blacklist, None)
    exceptions = [i['exception'] for i in data if 'exception' in i]
    results = (i['results'] for i in data if 'results' in i)
    results_stripped = (i.strip_root(complete_state.root_dir) for i in results)
    files = set(sum((result.existent for result in results_stripped), []))
    tracked, touched = isolate_format.split_touched(files)
    value = generate_isolate(
        tracked,
        [],
        touched,
        complete_state.root_dir,
        complete_state.saved_state.path_variables,
        complete_state.saved_state.config_variables,
        complete_state.saved_state.extra_variables,
        complete_state.saved_state.relative_cwd,
        trace_blacklist)
    return value, exceptions
  except trace_inputs.TracingFailure, e:
    raise ExecutionError(
        'Reading traces failed for: %s\n%s' %
          (' '.join(complete_state.saved_state.command), str(e)))


def merge(complete_state, trace_blacklist):
  """Reads a trace and merges it back into the source .isolate file."""
  value, exceptions = read_trace_as_isolate_dict(
      complete_state, trace_blacklist)

  # Now take that data and union it into the original .isolate file.
  with open(complete_state.saved_state.isolate_filepath, 'r') as f:
    prev_content = f.read()
  isolate_dir = os.path.dirname(complete_state.saved_state.isolate_filepath)
  prev_config = isolate_format.load_isolate_as_config(
      isolate_dir,
      isolate_format.eval_content(prev_content),
      isolate_format.extract_comment(prev_content))
  new_config = isolate_format.load_isolate_as_config(isolate_dir, value, '')
  config = prev_config.union(new_config)
  data = config.make_isolate_file()
  print('Updating %s' % complete_state.saved_state.isolate_file)
  with open(complete_state.saved_state.isolate_filepath, 'wb') as f:
    isolate_format.print_all(config.file_comment, data, f)
  if exceptions:
    # It got an exception, raise the first one.
    raise \
        exceptions[0][0], \
        exceptions[0][1], \
        exceptions[0][2]


def create_isolate_tree(outdir, root_dir, files, relative_cwd, read_only):
  """Creates a isolated tree usable for test execution.

  Returns the current working directory where the isolated command should be
  started in.
  """
  # Forcibly copy when the tree has to be read only. Otherwise the inode is
  # modified, and this cause real problems because the user's source tree
  # becomes read only. On the other hand, the cost of doing file copy is huge.
  if read_only not in (0, None):
    action = run_isolated.COPY
  else:
    action = run_isolated.HARDLINK_WITH_FALLBACK

  recreate_tree(
      outdir=outdir,
      indir=root_dir,
      infiles=files,
      action=action,
      as_hash=False)
  cwd = os.path.normpath(os.path.join(outdir, relative_cwd))
  if not os.path.isdir(cwd):
    # It can happen when no files are mapped from the directory containing the
    # .isolate file. But the directory must exist to be the current working
    # directory.
    os.makedirs(cwd)
  run_isolated.change_tree_read_only(outdir, read_only)
  return cwd


def prepare_for_archival(options, cwd):
  """Loads the isolated file and create 'infiles' for archival."""
  complete_state = load_complete_state(
      options, cwd, options.subdir, False)
  # Make sure that complete_state isn't modified until save_files() is
  # called, because any changes made to it here will propagate to the files
  # created (which is probably not intended).
  complete_state.save_files()

  infiles = complete_state.saved_state.files
  # Add all the .isolated files.
  isolated_hash = []
  isolated_files = [
    options.isolated,
  ] + complete_state.saved_state.child_isolated_files
  for item in isolated_files:
    item_path = os.path.join(
        os.path.dirname(complete_state.isolated_filepath), item)
    # Do not use isolateserver.hash_file() here because the file is
    # likely smallish (under 500kb) and its file size is needed.
    with open(item_path, 'rb') as f:
      content = f.read()
    isolated_hash.append(
        complete_state.saved_state.algo(content).hexdigest())
    isolated_metadata = {
      'h': isolated_hash[-1],
      's': len(content),
      'priority': '0'
    }
    infiles[item_path] = isolated_metadata
  return complete_state, infiles, isolated_hash


### Commands.


def CMDarchive(parser, args):
  """Creates a .isolated file and uploads the tree to an isolate server.

  All the files listed in the .isolated file are put in the isolate server
  cache via isolateserver.py.
  """
  add_subdir_option(parser)
  isolateserver.add_isolate_server_options(parser, False)
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth.process_auth_options(parser, options)
  isolateserver.process_isolate_server_options(parser, options)
  if args:
    parser.error('Unsupported argument: %s' % args)
  if file_path.is_url(options.isolate_server):
    auth.ensure_logged_in(options.isolate_server)
  cwd = os.getcwd()
  with tools.Profiler('GenerateHashtable'):
    success = False
    try:
      complete_state, infiles, isolated_hash = prepare_for_archival(
          options, cwd)
      logging.info('Creating content addressed object store with %d item',
                   len(infiles))

      isolateserver.upload_tree(
          base_url=options.isolate_server,
          indir=complete_state.root_dir,
          infiles=infiles,
          namespace=options.namespace)
      success = True
      print('%s  %s' % (isolated_hash[0], os.path.basename(options.isolated)))
    finally:
      # If the command failed, delete the .isolated file if it exists. This is
      # important so no stale swarm job is executed.
      if not success and os.path.isfile(options.isolated):
        os.remove(options.isolated)
  return int(not success)


def CMDcheck(parser, args):
  """Checks that all the inputs are present and generates .isolated."""
  add_subdir_option(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)

  complete_state = load_complete_state(
      options, os.getcwd(), options.subdir, False)

  # Nothing is done specifically. Just store the result and state.
  complete_state.save_files()
  return 0


def CMDhashtable(parser, args):
  """Creates a .isolated file and stores the contains in a directory.

  All the files listed in the .isolated file are put in the directory with their
  sha-1 as their file name. When using an NFS/CIFS server, the files can then be
  shared accross slaves without an isolate server.
  """
  add_subdir_option(parser)
  isolateserver.add_outdir_options(parser)
  add_skip_refresh_option(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)
  cwd = os.getcwd()
  isolateserver.process_outdir_options(parser, options, cwd)

  success = False
  try:
    complete_state, infiles, isolated_hash = prepare_for_archival(options, cwd)
    logging.info('Creating content addressed object store with %d item',
                  len(infiles))
    if not os.path.isdir(options.outdir):
      os.makedirs(options.outdir)

    # TODO(maruel): Make the files read-only?
    recreate_tree(
        outdir=options.outdir,
        indir=complete_state.root_dir,
        infiles=infiles,
        action=run_isolated.HARDLINK_WITH_FALLBACK,
        as_hash=True)
    success = True
    print('%s  %s' % (isolated_hash[0], os.path.basename(options.isolated)))
  finally:
    # If the command failed, delete the .isolated file if it exists. This is
    # important so no stale swarm job is executed.
    if not success and os.path.isfile(options.isolated):
      os.remove(options.isolated)
  return int(not success)


def CMDmerge(parser, args):
  """Reads and merges the data from the trace back into the original .isolate.
  """
  parser.require_isolated = False
  add_trace_option(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)

  complete_state = load_complete_state(options, os.getcwd(), None, False)
  blacklist = tools.gen_blacklist(options.trace_blacklist)
  merge(complete_state, blacklist)
  return 0


def CMDread(parser, args):
  """Reads the trace file generated with command 'trace'."""
  parser.require_isolated = False
  add_trace_option(parser)
  add_skip_refresh_option(parser)
  parser.add_option(
      '-m', '--merge', action='store_true',
      help='merge the results back in the .isolate file instead of printing')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)

  complete_state = load_complete_state(
      options, os.getcwd(), None, options.skip_refresh)
  blacklist = tools.gen_blacklist(options.trace_blacklist)
  value, exceptions = read_trace_as_isolate_dict(complete_state, blacklist)
  if options.merge:
    merge(complete_state, blacklist)
  else:
    isolate_format.pretty_print(value, sys.stdout)

  if exceptions:
    # It got an exception, raise the first one.
    raise \
        exceptions[0][0], \
        exceptions[0][1], \
        exceptions[0][2]
  return 0


def CMDremap(parser, args):
  """Creates a directory with all the dependencies mapped into it.

  Useful to test manually why a test is failing. The target executable is not
  run.
  """
  parser.require_isolated = False
  isolateserver.add_outdir_options(parser)
  add_skip_refresh_option(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)
  cwd = os.getcwd()
  isolateserver.process_outdir_options(parser, options, cwd)
  complete_state = load_complete_state(options, cwd, None, options.skip_refresh)

  if not os.path.isdir(options.outdir):
    os.makedirs(options.outdir)
  print('Remapping into %s' % options.outdir)
  if os.listdir(options.outdir):
    raise ExecutionError('Can\'t remap in a non-empty directory')

  create_isolate_tree(
      options.outdir, complete_state.root_dir, complete_state.saved_state.files,
      complete_state.saved_state.relative_cwd,
      complete_state.saved_state.read_only)
  if complete_state.isolated_filepath:
    complete_state.save_files()
  return 0


def CMDrewrite(parser, args):
  """Rewrites a .isolate file into the canonical format."""
  parser.require_isolated = False
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)

  if options.isolated:
    # Load the previous state if it was present. Namely, "foo.isolated.state".
    complete_state = CompleteState.load_files(options.isolated)
    isolate = options.isolate or complete_state.saved_state.isolate_filepath
  else:
    isolate = options.isolate
  if not isolate:
    parser.error('--isolate is required.')

  with open(isolate, 'r') as f:
    content = f.read()
  config = isolate_format.load_isolate_as_config(
      os.path.dirname(os.path.abspath(isolate)),
      isolate_format.eval_content(content),
      isolate_format.extract_comment(content))
  data = config.make_isolate_file()
  print('Updating %s' % isolate)
  with open(isolate, 'wb') as f:
    isolate_format.print_all(config.file_comment, data, f)
  return 0


@subcommand.usage('-- [extra arguments]')
def CMDrun(parser, args):
  """Runs the test executable in an isolated (temporary) directory.

  All the dependencies are mapped into the temporary directory and the
  directory is cleaned up after the target exits.

  Argument processing stops at -- and these arguments are appended to the
  command line of the target to run. For example, use:
    isolate.py run --isolated foo.isolated -- --gtest_filter=Foo.Bar
  """
  parser.require_isolated = False
  add_skip_refresh_option(parser)
  options, args = parser.parse_args(args)

  complete_state = load_complete_state(
      options, os.getcwd(), None, options.skip_refresh)
  cmd = complete_state.saved_state.command + args
  if not cmd:
    raise ExecutionError('No command to run.')
  cmd = tools.fix_python_path(cmd)

  outdir = run_isolated.make_temp_dir(
      'isolate-%s' % datetime.date.today(),
      os.path.dirname(complete_state.root_dir))
  try:
    # TODO(maruel): Use run_isolated.run_tha_test().
    cwd = create_isolate_tree(
        outdir, complete_state.root_dir, complete_state.saved_state.files,
        complete_state.saved_state.relative_cwd,
        complete_state.saved_state.read_only)
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    result = subprocess.call(cmd, cwd=cwd)
  finally:
    run_isolated.rmtree(outdir)

  if complete_state.isolated_filepath:
    complete_state.save_files()
  return result


@subcommand.usage('-- [extra arguments]')
def CMDtrace(parser, args):
  """Traces the target using trace_inputs.py.

  It runs the executable without remapping it, and traces all the files it and
  its child processes access. Then the 'merge' command can be used to generate
  an updated .isolate file out of it or the 'read' command to print it out to
  stdout.

  Argument processing stops at -- and these arguments are appended to the
  command line of the target to run. For example, use:
    isolate.py trace --isolated foo.isolated -- --gtest_filter=Foo.Bar
  """
  add_trace_option(parser)
  parser.add_option(
      '-m', '--merge', action='store_true',
      help='After tracing, merge the results back in the .isolate file')
  add_skip_refresh_option(parser)
  options, args = parser.parse_args(args)

  complete_state = load_complete_state(
      options, os.getcwd(), None, options.skip_refresh)
  cmd = complete_state.saved_state.command + args
  if not cmd:
    raise ExecutionError('No command to run.')
  cmd = tools.fix_python_path(cmd)
  cwd = os.path.normpath(os.path.join(
      unicode(complete_state.root_dir),
      complete_state.saved_state.relative_cwd))
  cmd[0] = os.path.normpath(os.path.join(cwd, cmd[0]))
  if not os.path.isfile(cmd[0]):
    raise ExecutionError(
        'Tracing failed for: %s\nIt doesn\'t exit' % ' '.join(cmd))
  logging.info('Running %s, cwd=%s' % (cmd, cwd))
  api = trace_inputs.get_api()
  logfile = complete_state.isolated_filepath + '.log'
  api.clean_trace(logfile)
  out = None
  try:
    with api.get_tracer(logfile) as tracer:
      result, out = tracer.trace(
          cmd,
          cwd,
          'default',
          True)
  except trace_inputs.TracingFailure, e:
    raise ExecutionError('Tracing failed for: %s\n%s' % (' '.join(cmd), str(e)))

  if result:
    logging.error(
        'Tracer exited with %d, which means the tests probably failed so the '
        'trace is probably incomplete.', result)
    logging.info(out)

  complete_state.save_files()

  if options.merge:
    blacklist = tools.gen_blacklist(options.trace_blacklist)
    merge(complete_state, blacklist)

  return result


def _process_variable_arg(option, opt, _value, parser):
  """Called by OptionParser to process a --<foo>-variable argument."""
  if not parser.rargs:
    raise optparse.OptionValueError(
        'Please use %s FOO=BAR or %s FOO BAR' % (opt, opt))
  k = parser.rargs.pop(0)
  variables = getattr(parser.values, option.dest)
  if '=' in k:
    k, v = k.split('=', 1)
  else:
    if not parser.rargs:
      raise optparse.OptionValueError(
          'Please use %s FOO=BAR or %s FOO BAR' % (opt, opt))
    v = parser.rargs.pop(0)
  if not re.match('^' + isolate_format.VALID_VARIABLE + '$', k):
    raise optparse.OptionValueError(
        'Variable \'%s\' doesn\'t respect format \'%s\'' %
        (k, isolate_format.VALID_VARIABLE))
  variables.append((k, v.decode('utf-8')))


def add_variable_option(parser):
  """Adds --isolated and --<foo>-variable to an OptionParser."""
  parser.add_option(
      '-s', '--isolated',
      metavar='FILE',
      help='.isolated file to generate or read')
  # Keep for compatibility. TODO(maruel): Remove once not used anymore.
  parser.add_option(
      '-r', '--result',
      dest='isolated',
      help=optparse.SUPPRESS_HELP)
  is_win = sys.platform in ('win32', 'cygwin')
  # There is really 3 kind of variables:
  # - path variables, like DEPTH or PRODUCT_DIR that should be
  #   replaced opportunistically when tracing tests.
  # - extraneous things like EXECUTABE_SUFFIX.
  # - configuration variables that are to be used in deducing the matrix to
  #   reduce.
  # - unrelated variables that are used as command flags for example.
  parser.add_option(
      '--config-variable',
      action='callback',
      callback=_process_variable_arg,
      default=[],
      dest='config_variables',
      metavar='FOO BAR',
      help='Config variables are used to determine which conditions should be '
           'matched when loading a .isolate file, default: %default. '
            'All 3 kinds of variables are persistent accross calls, they are '
            'saved inside <.isolated>.state')
  parser.add_option(
      '--path-variable',
      action='callback',
      callback=_process_variable_arg,
      default=[],
      dest='path_variables',
      metavar='FOO BAR',
      help='Path variables are used to replace file paths when loading a '
           '.isolate file, default: %default')
  parser.add_option(
      '--extra-variable',
      action='callback',
      callback=_process_variable_arg,
      default=[('EXECUTABLE_SUFFIX', '.exe' if is_win else '')],
      dest='extra_variables',
      metavar='FOO BAR',
      help='Extraneous variables are replaced on the \'command\' entry and on '
           'paths in the .isolate file but are not considered relative paths.')


def add_subdir_option(parser):
  parser.add_option(
      '--subdir',
      help='Filters to a subdirectory. Its behavior changes depending if it '
           'is a relative path as a string or as a path variable. Path '
           'variables are always keyed from the directory containing the '
           '.isolate file. Anything else is keyed on the root directory.')


def add_trace_option(parser):
  """Adds --trace-blacklist to the parser."""
  parser.add_option(
      '--trace-blacklist',
      action='append', default=list(isolateserver.DEFAULT_BLACKLIST),
      help='List of regexp to use as blacklist filter for files to consider '
           'important, not to be confused with --blacklist which blacklists '
           'test case.')


def add_skip_refresh_option(parser):
  parser.add_option(
      '--skip-refresh', action='store_true',
      help='Skip reading .isolate file and do not refresh the hash of '
           'dependencies')


def parse_isolated_option(parser, options, cwd, require_isolated):
  """Processes --isolated."""
  if options.isolated:
    options.isolated = os.path.normpath(
        os.path.join(cwd, options.isolated.replace('/', os.path.sep)))
  if require_isolated and not options.isolated:
    parser.error('--isolated is required.')
  if options.isolated and not options.isolated.endswith('.isolated'):
    parser.error('--isolated value must end with \'.isolated\'')


def parse_variable_option(options):
  """Processes all the --<foo>-variable flags."""
  # TODO(benrg): Maybe we should use a copy of gyp's NameValueListToDict here,
  # but it wouldn't be backward compatible.
  def try_make_int(s):
    """Converts a value to int if possible, converts to unicode otherwise."""
    try:
      return int(s)
    except ValueError:
      return s.decode('utf-8')
  options.config_variables = dict(
      (k, try_make_int(v)) for k, v in options.config_variables)
  options.path_variables = dict(options.path_variables)
  options.extra_variables = dict(options.extra_variables)


class OptionParserIsolate(tools.OptionParserWithLogging):
  """Adds automatic --isolate, --isolated, --out and --<foo>-variable handling.
  """
  # Set it to False if it is not required, e.g. it can be passed on but do not
  # fail if not given.
  require_isolated = True

  def __init__(self, **kwargs):
    tools.OptionParserWithLogging.__init__(
        self,
        verbose=int(os.environ.get('ISOLATE_DEBUG', 0)),
        **kwargs)
    group = optparse.OptionGroup(self, "Common options")
    group.add_option(
        '-i', '--isolate',
        metavar='FILE',
        help='.isolate file to load the dependency data from')
    add_variable_option(group)
    group.add_option(
        '--ignore_broken_items', action='store_true',
        default=bool(os.environ.get('ISOLATE_IGNORE_BROKEN_ITEMS')),
        help='Indicates that invalid entries in the isolated file to be '
             'only be logged and not stop processing. Defaults to True if '
             'env var ISOLATE_IGNORE_BROKEN_ITEMS is set')
    self.add_option_group(group)

  def parse_args(self, *args, **kwargs):
    """Makes sure the paths make sense.

    On Windows, / and \ are often mixed together in a path.
    """
    options, args = tools.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    if not self.allow_interspersed_args and args:
      self.error('Unsupported argument: %s' % args)

    cwd = file_path.get_native_path_case(unicode(os.getcwd()))
    parse_isolated_option(self, options, cwd, self.require_isolated)
    parse_variable_option(options)

    if options.isolate:
      # TODO(maruel): Work with non-ASCII.
      # The path must be in native path case for tracing purposes.
      options.isolate = unicode(options.isolate).replace('/', os.path.sep)
      options.isolate = os.path.normpath(os.path.join(cwd, options.isolate))
      options.isolate = file_path.get_native_path_case(options.isolate)

    return options, args


def main(argv):
  dispatcher = subcommand.CommandDispatcher(__name__)
  return dispatcher.execute(OptionParserIsolate(version=__version__), argv)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
