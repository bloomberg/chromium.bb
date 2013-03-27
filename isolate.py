#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Front end tool to manage .isolate files and corresponding tests.

Run ./isolate.py --help for more detailed information.

See more information at
http://dev.chromium.org/developers/testing/isolated-testing
"""

import ast
import copy
import hashlib
import itertools
import logging
import optparse
import os
import posixpath
import re
import stat
import subprocess
import sys

import isolateserver_archive
import run_isolated
import short_expression_finder
import trace_inputs

# Import here directly so isolate is easier to use as a library.
from run_isolated import get_flavor


PATH_VARIABLES = ('DEPTH', 'PRODUCT_DIR')

# Files that should be 0-length when mapped.
KEY_TOUCHED = 'isolate_dependency_touched'
# Files that should be tracked by the build tool.
KEY_TRACKED = 'isolate_dependency_tracked'
# Files that should not be tracked by the build tool.
KEY_UNTRACKED = 'isolate_dependency_untracked'

_GIT_PATH = os.path.sep + '.git'
_SVN_PATH = os.path.sep + '.svn'


class ExecutionError(Exception):
  """A generic error occurred."""
  def __str__(self):
    return self.args[0]


### Path handling code.


def relpath(path, root):
  """os.path.relpath() that keeps trailing os.path.sep."""
  out = os.path.relpath(path, root)
  if path.endswith(os.path.sep):
    out += os.path.sep
  return out


def normpath(path):
  """os.path.normpath() that keeps trailing os.path.sep."""
  out = os.path.normpath(path)
  if path.endswith(os.path.sep):
    out += os.path.sep
  return out


def posix_relpath(path, root):
  """posix.relpath() that keeps trailing slash."""
  out = posixpath.relpath(path, root)
  if path.endswith('/'):
    out += '/'
  return out


def cleanup_path(x):
  """Cleans up a relative path. Converts any os.path.sep to '/' on Windows."""
  if x:
    x = x.rstrip(os.path.sep).replace(os.path.sep, '/')
  if x == '.':
    x = ''
  if x:
    x += '/'
  return x


def is_url(path):
  return bool(re.match(r'^https?://.+$', path))


def default_blacklist(f):
  """Filters unimportant files normally ignored."""
  return (
      f.endswith(('.pyc', '.run_test_cases', 'testserver.log')) or
      _GIT_PATH in f or
      _SVN_PATH in f or
      f in ('.git', '.svn'))


def path_starts_with(prefix, path):
  """Returns true if the components of the path |prefix| are the same as the
  initial components of |path| (or all of the components of |path|). The paths
  must be absolute.
  """
  assert os.path.isabs(prefix) and os.path.isabs(path)
  prefix = os.path.normpath(prefix)
  path = os.path.normpath(path)
  assert prefix == trace_inputs.get_native_path_case(prefix), prefix
  assert path == trace_inputs.get_native_path_case(path), path
  prefix = prefix.rstrip(os.path.sep) + os.path.sep
  path = path.rstrip(os.path.sep) + os.path.sep
  return path.startswith(prefix)


def expand_symlinks(indir, relfile):
  """Follows symlinks in |relfile|, but treating symlinks that point outside the
  build tree as if they were ordinary directories/files. Returns the final
  symlink-free target and a list of paths to symlinks encountered in the
  process.

  The rule about symlinks outside the build tree is for the benefit of the
  Chromium OS ebuild, which symlinks the output directory to an unrelated path
  in the chroot.

  Fails when a directory loop is detected, although in theory we could support
  that case.
  """
  is_directory = relfile.endswith(os.path.sep)
  done = indir
  todo = relfile.strip(os.path.sep)
  symlinks = []

  while todo:
    pre_symlink, symlink, post_symlink = trace_inputs.split_at_symlink(
        done, todo)
    if not symlink:
      done = os.path.join(done, todo)
      break
    symlink_path = os.path.join(done, pre_symlink, symlink)
    post_symlink = post_symlink.lstrip(os.path.sep)
    # readlink doesn't exist on Windows.
    # pylint: disable=E1101
    target = os.readlink(symlink_path)
    target = os.path.normpath(os.path.join(done, pre_symlink, target))
    if not os.path.exists(target):
      raise run_isolated.MappingError(
          'Symlink target doesn\'t exist: %s -> %s' % (symlink_path, target))
    target = trace_inputs.get_native_path_case(target)
    if not path_starts_with(indir, target):
      done = symlink_path
      todo = post_symlink
      continue
    if path_starts_with(target, symlink_path):
      raise run_isolated.MappingError(
          'Can\'t map recursive symlink reference %s -> %s' %
          (symlink_path, target))
    logging.info('Found symlink: %s -> %s', symlink_path, target)
    symlinks.append(os.path.relpath(symlink_path, indir))
    # Treat the common prefix of the old and new paths as done, and start
    # scanning again.
    target = target.split(os.path.sep)
    symlink_path = symlink_path.split(os.path.sep)
    prefix_length = 0
    for target_piece, symlink_path_piece in zip(target, symlink_path):
      if target_piece == symlink_path_piece:
        prefix_length += 1
      else:
        break
    done = os.path.sep.join(target[:prefix_length])
    todo = os.path.join(
        os.path.sep.join(target[prefix_length:]), post_symlink)

  relfile = os.path.relpath(done, indir)
  relfile = relfile.rstrip(os.path.sep) + is_directory * os.path.sep
  return relfile, symlinks


def expand_directory_and_symlink(indir, relfile, blacklist, follow_symlinks):
  """Expands a single input. It can result in multiple outputs.

  This function is recursive when relfile is a directory.

  Note: this code doesn't properly handle recursive symlink like one created
  with:
    ln -s .. foo
  """
  if os.path.isabs(relfile):
    raise run_isolated.MappingError(
        'Can\'t map absolute path %s' % relfile)

  infile = normpath(os.path.join(indir, relfile))
  if not infile.startswith(indir):
    raise run_isolated.MappingError(
        'Can\'t map file %s outside %s' % (infile, indir))

  filepath = os.path.join(indir, relfile)
  native_filepath = trace_inputs.get_native_path_case(filepath)
  if filepath != native_filepath:
    # Special case './'.
    if filepath != native_filepath + '.' + os.path.sep:
      raise run_isolated.MappingError(
          'File path doesn\'t equal native file path\n%s != %s' %
          (filepath, native_filepath))

  symlinks = []
  if follow_symlinks:
    relfile, symlinks = expand_symlinks(indir, relfile)

  if relfile.endswith(os.path.sep):
    if not os.path.isdir(infile):
      raise run_isolated.MappingError(
          '%s is not a directory but ends with "%s"' % (infile, os.path.sep))

    # Special case './'.
    if relfile.startswith('.' + os.path.sep):
      relfile = relfile[2:]
    outfiles = symlinks
    try:
      for filename in os.listdir(infile):
        inner_relfile = os.path.join(relfile, filename)
        if blacklist(inner_relfile):
          continue
        if os.path.isdir(os.path.join(indir, inner_relfile)):
          inner_relfile += os.path.sep
        outfiles.extend(
            expand_directory_and_symlink(indir, inner_relfile, blacklist,
                                         follow_symlinks))
      return outfiles
    except OSError as e:
      raise run_isolated.MappingError('Unable to iterate over directories.\n'
                                      '%s' % e)
  else:
    # Always add individual files even if they were blacklisted.
    if os.path.isdir(infile):
      raise run_isolated.MappingError(
          'Input directory %s must have a trailing slash' % infile)

    if not os.path.isfile(infile):
      raise run_isolated.MappingError(
          'Input file %s doesn\'t exist' % infile)

    return symlinks + [relfile]


def expand_directories_and_symlinks(indir, infiles, blacklist,
                                    follow_symlinks, ignore_broken_items):
  """Expands the directories and the symlinks, applies the blacklist and
  verifies files exist.

  Files are specified in os native path separator.
  """
  outfiles = []
  for relfile in infiles:
    try:
      outfiles.extend(expand_directory_and_symlink(indir, relfile, blacklist,
                                                   follow_symlinks))
    except run_isolated.MappingError as e:
      if ignore_broken_items:
        logging.info('warning: %s', e)
      else:
        raise
  return outfiles


def recreate_tree(outdir, indir, infiles, action, as_sha1):
  """Creates a new tree with only the input files in it.

  Arguments:
    outdir:    Output directory to create the files in.
    indir:     Root directory the infiles are based in.
    infiles:   dict of files to map from |indir| to |outdir|.
    action:    See assert below.
    as_sha1:   Output filename is the sha1 instead of relfile.
  """
  logging.info(
      'recreate_tree(outdir=%s, indir=%s, files=%d, action=%s, as_sha1=%s)' %
      (outdir, indir, len(infiles), action, as_sha1))

  assert action in (
      run_isolated.HARDLINK,
      run_isolated.SYMLINK,
      run_isolated.COPY)
  assert os.path.isabs(outdir) and outdir == os.path.normpath(outdir), outdir
  if not os.path.isdir(outdir):
    logging.info('Creating %s' % outdir)
    os.makedirs(outdir)

  for relfile, metadata in infiles.iteritems():
    infile = os.path.join(indir, relfile)
    if as_sha1:
      # Do the hashtable specific checks.
      if 'l' in metadata:
        # Skip links when storing a hashtable.
        continue
      outfile = os.path.join(outdir, metadata['h'])
      if os.path.isfile(outfile):
        # Just do a quick check that the file size matches. No need to stat()
        # again the input file, grab the value from the dict.
        if not 's' in metadata:
          raise run_isolated.MappingError(
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


def process_input(filepath, prevdict, read_only):
  """Processes an input file, a dependency, and return meta data about it.

  Arguments:
  - filepath: File to act on.
  - prevdict: the previous dictionary. It is used to retrieve the cached sha-1
              to skip recalculating the hash.
  - read_only: If True, the file mode is manipulated. In practice, only save
               one of 4 modes: 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r). On
               windows, mode is not set since all files are 'executable' by
               default.

  Behaviors:
  - Retrieves the file mode, file size, file timestamp, file link
    destination if it is a file link and calcultate the SHA-1 of the file's
    content if the path points to a file and not a symlink.
  """
  out = {}
  # TODO(csharp): Fix crbug.com/150823 and enable the touched logic again.
  # if prevdict.get('T') == True:
  #   # The file's content is ignored. Skip the time and hard code mode.
  #   if get_flavor() != 'win':
  #     out['m'] = stat.S_IRUSR | stat.S_IRGRP
  #   out['s'] = 0
  #   out['h'] = SHA_1_NULL
  #   out['T'] = True
  #   return out

  # Always check the file stat and check if it is a link. The timestamp is used
  # to know if the file's content/symlink destination should be looked into.
  # E.g. only reuse from prevdict if the timestamp hasn't changed.
  # There is the risk of the file's timestamp being reset to its last value
  # manually while its content changed. We don't protect against that use case.
  try:
    filestats = os.lstat(filepath)
  except OSError:
    # The file is not present.
    raise run_isolated.MappingError('%s is missing' % filepath)
  is_link = stat.S_ISLNK(filestats.st_mode)

  if get_flavor() != 'win':
    # Ignore file mode on Windows since it's not really useful there.
    filemode = stat.S_IMODE(filestats.st_mode)
    # Remove write access for group and all access to 'others'.
    filemode &= ~(stat.S_IWGRP | stat.S_IRWXO)
    if read_only:
      filemode &= ~stat.S_IWUSR
    if filemode & stat.S_IXUSR:
      filemode |= stat.S_IXGRP
    else:
      filemode &= ~stat.S_IXGRP
    out['m'] = filemode

  # Used to skip recalculating the hash or link destination. Use the most recent
  # update time.
  # TODO(maruel): Save it in the .state file instead of .isolated so the
  # .isolated file is deterministic.
  out['t'] = int(round(filestats.st_mtime))

  if not is_link:
    out['s'] = filestats.st_size
    # If the timestamp wasn't updated and the file size is still the same, carry
    # on the sha-1.
    if (prevdict.get('t') == out['t'] and
        prevdict.get('s') == out['s']):
      # Reuse the previous hash if available.
      out['h'] = prevdict.get('h')
    if not out.get('h'):
      out['h'] = isolateserver_archive.sha1_file(filepath)
  else:
    # If the timestamp wasn't updated, carry on the link destination.
    if prevdict.get('t') == out['t']:
      # Reuse the previous link destination if available.
      out['l'] = prevdict.get('l')
    if out.get('l') is None:
      out['l'] = os.readlink(filepath)  # pylint: disable=E1101
  return out


### Variable stuff.


def isolatedfile_to_state(filename):
  """Replaces the file's extension."""
  return filename + '.state'


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
  m = re.match(r'<\(([A-Z_]+)\)', part)
  if m:
    if m.group(1) not in variables:
      raise ExecutionError(
        'Variable "%s" was not found in %s.\nDid you forget to specify '
        '--variable?' % (m.group(1), variables))
    return variables[m.group(1)]
  return part


def process_variables(cwd, variables, relative_base_dir):
  """Processes path variables as a special case and returns a copy of the dict.

  For each 'path' variable: first normalizes it based on |cwd|, verifies it
  exists then sets it as relative to relative_base_dir.
  """
  relative_base_dir = trace_inputs.get_native_path_case(relative_base_dir)
  variables = variables.copy()
  for i in PATH_VARIABLES:
    if i not in variables:
      continue
    variable = variables[i].strip()
    # Variables could contain / or \ on windows. Always normalize to
    # os.path.sep.
    variable = variable.replace('/', os.path.sep)
    variable = os.path.join(cwd, variable)
    variable = os.path.normpath(variable)
    variable = trace_inputs.get_native_path_case(variable)
    if not os.path.isdir(variable):
      raise ExecutionError('%s=%s is not a directory' % (i, variable))

    # All variables are relative to the .isolate file.
    variable = os.path.relpath(variable, relative_base_dir)
    logging.debug(
        'Translated variable %s from %s to %s', i, variables[i], variable)
    variables[i] = variable
  return variables


def eval_variables(item, variables):
  """Replaces the .isolate variables in a string item.

  Note that the .isolate format is a subset of the .gyp dialect.
  """
  return ''.join(
      replace_variable(p, variables) for p in re.split(r'(<\([A-Z_]+\))', item))


def classify_files(root_dir, tracked, untracked):
  """Converts the list of files into a .isolate 'variables' dictionary.

  Arguments:
  - tracked: list of files names to generate a dictionary out of that should
             probably be tracked.
  - untracked: list of files names that must not be tracked.
  """
  # These directories are not guaranteed to be always present on every builder.
  OPTIONAL_DIRECTORIES = (
    'test/data/plugin',
    'third_party/WebKit/LayoutTests',
  )

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
    if any(i in filepath for i in OPTIONAL_DIRECTORIES):
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
    variables[KEY_TRACKED] = sorted(new_tracked)
  if new_untracked:
    variables[KEY_UNTRACKED] = sorted(new_untracked)
  return variables


def chromium_fix(f, variables):
  """Fixes an isolate dependnecy with Chromium-specific fixes."""
  # Skip log in PRODUCT_DIR. Note that these are applied on '/' style path
  # separator.
  LOG_FILE = re.compile(r'^\<\(PRODUCT_DIR\)\/[^\/]+\.log$')
  # Ignored items.
  IGNORED_ITEMS = (
      # http://crbug.com/160539, on Windows, it's in chrome/.
      'Media Cache/',
      'chrome/Media Cache/',
      # 'First Run' is not created by the compile, but by the test itself.
      '<(PRODUCT_DIR)/First Run')

  # Blacklist logs and other unimportant files.
  if LOG_FILE.match(f) or f in IGNORED_ITEMS:
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
    tracked, untracked, touched, root_dir, variables, relative_cwd):
  """Generates a clean and complete .isolate 'variables' dictionary.

  Cleans up and extracts only files from within root_dir then processes
  variables and relative_cwd.
  """
  root_dir = os.path.realpath(root_dir)
  logging.info(
      'generate_simplified(%d files, %s, %s, %s)' %
      (len(tracked) + len(untracked) + len(touched),
        root_dir, variables, relative_cwd))

  # Preparation work.
  relative_cwd = cleanup_path(relative_cwd)
  assert not os.path.isabs(relative_cwd), relative_cwd
  # Creates the right set of variables here. We only care about PATH_VARIABLES.
  path_variables = dict(
      ('<(%s)' % k, variables[k].replace(os.path.sep, '/'))
      for k in PATH_VARIABLES if k in variables)
  variables = variables.copy()
  variables.update(path_variables)

  # Actual work: Process the files.
  # TODO(maruel): if all the files in a directory are in part tracked and in
  # part untracked, the directory will not be extracted. Tracked files should be
  # 'promoted' to be untracked as needed.
  tracked = trace_inputs.extract_directories(
      root_dir, tracked, default_blacklist)
  untracked = trace_inputs.extract_directories(
      root_dir, untracked, default_blacklist)
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
      f = posix_relpath(
          posixpath.join(root_dir_posix, f),
          posixpath.join(root_dir_posix, relative_cwd)) or './'

    for variable, root_path in path_variables.iteritems():
      if f.startswith(root_path):
        f = variable + f[len(root_path):]
        logging.debug('Converted to %s' % f)
        break
    return f

  def fix_all(items):
    """Reduces the items to convert variables, removes unneeded items, apply
    chromium-specific fixes and only return unique items.
    """
    variables_converted = (fix(f.path) for f in items)
    chromium_fixed = (chromium_fix(f, variables) for f in variables_converted)
    return set(f for f in chromium_fixed if f)

  tracked = fix_all(tracked)
  untracked = fix_all(untracked)
  touched = fix_all(touched)
  out = classify_files(root_dir, tracked, untracked)
  if touched:
    out[KEY_TOUCHED] = sorted(touched)
  return out


def chromium_filter_flags(variables):
  """Filters out build flags used in Chromium that we don't want to treat as
  configuration variables.
  """
  # TODO(benrg): Need a better way to determine this.
  blacklist = set(PATH_VARIABLES + ('EXECUTABLE_SUFFIX', 'FLAG'))
  return dict((k, v) for k, v in variables.iteritems() if k not in blacklist)


def generate_isolate(
    tracked, untracked, touched, root_dir, variables, relative_cwd):
  """Generates a clean and complete .isolate file."""
  dependencies = generate_simplified(
      tracked, untracked, touched, root_dir, variables, relative_cwd)
  config_variables = chromium_filter_flags(variables)
  config_variable_names, config_values = zip(
      *sorted(config_variables.iteritems()))
  out = Configs(None)
  # The new dependencies apply to just one configuration, namely config_values.
  out.merge_dependencies(dependencies, config_variable_names, [config_values])
  return out.make_isolate_file()


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


### Internal state files.


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
    assert not (self.config and rhs.config) or (self.config == rhs.config)
    assert not (self.command and rhs.command) or (self.command == rhs.command)
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
        raise ExecutionError(
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
    return re.match(r'OS=="(\w+)"\Z', cond[0]).group(1)

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

  |value| is the loaded dictionary that was defined in the gyp file.

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

  # Load the includes.
  for include in value.get('includes', []):
    if os.path.isabs(include):
      raise ExecutionError(
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


def load_isolate_for_config(isolate_dir, content, variables):
  """Loads the .isolate file and returns the information unprocessed but
  filtered for the specific OS.

  Returns the command, dependencies and read_only flag. The dependencies are
  fixed to use os.path.sep.
  """
  # Load the .isolate file, process its conditions, retrieve the command and
  # dependencies.
  isolate = load_isolate_as_config(isolate_dir, eval_content(content), None)
  try:
    config = tuple(variables[var] for var in isolate.config_variables)
  except KeyError:
    raise ExecutionError(
        'These configuration variables were missing from the command line: %s' %
        ', '.join(sorted(set(isolate.config_variables) - set(variables))))
  config = isolate.by_config.get(config)
  if not config:
    raise ExecutionError('Failed to load configuration for (%s)' %
        ', '.join(isolate.config_variables))
  # Merge tracked and untracked variables, isolate.py doesn't care about the
  # trackability of the variables, only the build tool does.
  dependencies = [
    f.replace('/', os.path.sep) for f in config.tracked + config.untracked
  ]
  touched = [f.replace('/', os.path.sep) for f in config.touched]
  return config.command, dependencies, touched, config.read_only


def chromium_save_isolated(isolated, data, variables):
  """Writes one or many .isolated files.

  This slightly increases the cold cache cost but greatly reduce the warm cache
  cost by splitting low-churn files off the master .isolated file. It also
  reduces overall isolateserver memcache consumption.
  """
  slaves = []

  def extract_into_included_isolated(prefix):
    new_slave = {'files': {}, 'os': data['os']}
    for f in data['files'].keys():
      if f.startswith(prefix):
        new_slave['files'][f] = data['files'].pop(f)
    if new_slave['files']:
      slaves.append(new_slave)

  # Split test/data/ in its own .isolated file.
  extract_into_included_isolated(os.path.join('test', 'data', ''))

  # Split everything out of PRODUCT_DIR in its own .isolated file.
  if variables.get('PRODUCT_DIR'):
    extract_into_included_isolated(variables['PRODUCT_DIR'])

  files = [isolated]
  for index, f in enumerate(slaves):
    slavepath = isolated[:-len('.isolated')] + '.%d.isolated' % index
    trace_inputs.write_json(slavepath, f, True)
    data.setdefault('includes', []).append(
        isolateserver_archive.sha1_file(slavepath))
    files.append(slavepath)

  trace_inputs.write_json(isolated, data, True)
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
  def load(cls, data):
    """Loads a flattened version."""
    data = data.copy()
    out = cls()
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
  def load_file(cls, filename):
    """Loads the data from a file or return an empty instance."""
    out = cls()
    try:
      out = cls.load(trace_inputs.read_json(filename))
      logging.debug('Loaded %s(%s)' % (cls.__name__, filename))
    except (IOError, ValueError):
      logging.warn('Failed to load %s' % filename)
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
    'command',
    'files',
    'isolate_file',
    'isolated_files',
    'read_only',
    'relative_cwd',
    'variables',
  )

  def __init__(self):
    super(SavedState, self).__init__()
    self.command = []
    self.files = {}
    # Link back to the .isolate file.
    self.isolate_file = None
    # Used to support/remember 'slave' .isolated files.
    self.isolated_files = []
    self.read_only = None
    self.relative_cwd = None
    # Variables are saved so a user can use isolate.py after building and the
    # GYP variables are still defined.
    self.variables = {'OS': get_flavor()}

  def update(self, isolate_file, variables):
    """Updates the saved state with new data to keep GYP variables and internal
    reference to the original .isolate file.
    """
    self.isolate_file = isolate_file
    self.variables.update(variables)

  def update_isolated(self, command, infiles, touched, read_only, relative_cwd):
    """Updates the saved state with data necessary to generate a .isolated file.
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

    http://chromium.org/developers/testing/isolated-testing/design
    """
    def strip(data):
      """Returns a 'files' entry with only the whitelisted keys."""
      return dict((k, data[k]) for k in ('h', 'l', 'm', 's') if k in data)

    out = {
      'files': dict(
          (filepath, strip(data)) for filepath, data in self.files.iteritems()),
      'os': self.variables['OS'],
    }
    if self.command:
      out['command'] = self.command
    if self.read_only is not None:
      out['read_only'] = self.read_only
    if self.relative_cwd:
      out['relative_cwd'] = self.relative_cwd
    return out

  @classmethod
  def load(cls, data):
    out = super(SavedState, cls).load(data)
    if 'os' in data:
      out.variables['OS'] = data['os']
    if out.variables['OS'] != get_flavor():
      raise run_isolated.ConfigError(
          'The .isolated.state file was created on another platform')
    if out.isolate_file:
      out.isolate_file = trace_inputs.get_native_path_case(
          unicode(out.isolate_file))
    return out

  def __str__(self):
    out = '%s(\n' % self.__class__.__name__
    out += '  command: %s\n' % self.command
    out += '  files: %d\n' % len(self.files)
    out += '  isolate_file: %s\n' % self.isolate_file
    out += '  read_only: %s\n' % self.read_only
    out += '  relative_cwd: %s' % self.relative_cwd
    out += '  isolated_files: %s' % self.isolated_files
    out += '  variables: %s' % ''.join(
        '\n    %s=%s' % (k, self.variables[k]) for k in sorted(self.variables))
    out += ')'
    return out


class CompleteState(object):
  """Contains all the state to run the task at hand."""
  def __init__(self, isolated_filepath, saved_state):
    super(CompleteState, self).__init__()
    self.isolated_filepath = isolated_filepath
    # Contains the data to ease developer's use-case but that is not strictly
    # necessary.
    self.saved_state = saved_state

  @classmethod
  def load_files(cls, isolated_filepath):
    """Loads state from disk."""
    assert os.path.isabs(isolated_filepath), isolated_filepath
    return cls(
        isolated_filepath,
        SavedState.load_file(isolatedfile_to_state(isolated_filepath)))

  def load_isolate(self, cwd, isolate_file, variables, ignore_broken_items):
    """Updates self.isolated and self.saved_state with information loaded from a
    .isolate file.

    Processes the loaded data, deduce root_dir, relative_cwd.
    """
    # Make sure to not depend on os.getcwd().
    assert os.path.isabs(isolate_file), isolate_file
    logging.info(
        'CompleteState.load_isolate(%s, %s, %s, %s)',
        cwd, isolate_file, variables, ignore_broken_items)
    relative_base_dir = os.path.dirname(isolate_file)

    # Processes the variables and update the saved state.
    variables = process_variables(cwd, variables, relative_base_dir)
    self.saved_state.update(isolate_file, variables)
    variables = self.saved_state.variables

    with open(isolate_file, 'r') as f:
      # At that point, variables are not replaced yet in command and infiles.
      # infiles may contain directory entries and is in posix style.
      command, infiles, touched, read_only = load_isolate_for_config(
          os.path.dirname(isolate_file), f.read(), variables)
    command = [eval_variables(i, variables) for i in command]
    infiles = [eval_variables(f, variables) for f in infiles]
    touched = [eval_variables(f, variables) for f in touched]
    # root_dir is automatically determined by the deepest root accessed with the
    # form '../../foo/bar'.
    root_dir = determine_root_dir(relative_base_dir, infiles + touched)
    # The relative directory is automatically determined by the relative path
    # between root_dir and the directory containing the .isolate file,
    # isolate_base_dir.
    relative_cwd = os.path.relpath(relative_base_dir, root_dir)
    # Now that we know where the root is, check that the PATH_VARIABLES point
    # inside it.
    for i in PATH_VARIABLES:
      if i in variables:
        if not path_starts_with(
            root_dir, os.path.join(relative_base_dir, variables[i])):
          raise run_isolated.MappingError(
              'Path variable %s=%r points outside the inferred root directory' %
              (i, variables[i]))
    # Normalize the files based to root_dir. It is important to keep the
    # trailing os.path.sep at that step.
    infiles = [
      relpath(normpath(os.path.join(relative_base_dir, f)), root_dir)
      for f in infiles
    ]
    touched = [
      relpath(normpath(os.path.join(relative_base_dir, f)), root_dir)
      for f in touched
    ]
    follow_symlinks = variables['OS'] != 'win'
    # Expand the directories by listing each file inside. Up to now, trailing
    # os.path.sep must be kept. Do not expand 'touched'.
    infiles = expand_directories_and_symlinks(
        root_dir,
        infiles,
        lambda x: re.match(r'.*\.(git|svn|pyc)$', x),
        follow_symlinks,
        ignore_broken_items)

    # If we ignore broken items then remove any missing touched items.
    if ignore_broken_items:
      original_touched_count = len(touched)
      touched = [touch for touch in touched if os.path.exists(touch)]

      if len(touched) != original_touched_count:
        logging.info('warning: removed %d invalid touched entries',
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

    See process_input() for more information.
    """
    for infile in sorted(self.saved_state.files):
      if subdir and not infile.startswith(subdir):
        self.saved_state.files.pop(infile)
      else:
        filepath = os.path.join(self.root_dir, infile)
        self.saved_state.files[infile] = process_input(
            filepath,
            self.saved_state.files[infile],
            self.saved_state.read_only)

  def save_files(self):
    """Saves self.saved_state and creates a .isolated file."""
    logging.debug('Dumping to %s' % self.isolated_filepath)
    self.saved_state.isolated_files = chromium_save_isolated(
        self.isolated_filepath,
        self.saved_state.to_isolated(),
        self.saved_state.variables)
    total_bytes = sum(
        i.get('s', 0) for i in self.saved_state.files.itervalues())
    if total_bytes:
      # TODO(maruel): Stats are missing the .isolated files.
      logging.debug('Total size: %d bytes' % total_bytes)
    saved_state_file = isolatedfile_to_state(self.isolated_filepath)
    logging.debug('Dumping to %s' % saved_state_file)
    trace_inputs.write_json(saved_state_file, self.saved_state.flatten(), True)

  @property
  def root_dir(self):
    """isolate_file is always inside relative_cwd relative to root_dir."""
    if not self.saved_state.isolate_file:
      raise ExecutionError('Please specify --isolate')
    isolate_dir = os.path.dirname(self.saved_state.isolate_file)
    # Special case '.'.
    if self.saved_state.relative_cwd == '.':
      return isolate_dir
    assert isolate_dir.endswith(self.saved_state.relative_cwd), (
        isolate_dir, self.saved_state.relative_cwd)
    return isolate_dir[:-(len(self.saved_state.relative_cwd) + 1)]

  @property
  def resultdir(self):
    """Directory containing the results, usually equivalent to the variable
    PRODUCT_DIR.
    """
    return os.path.dirname(self.isolated_filepath)

  def __str__(self):
    def indent(data, indent_length):
      """Indents text."""
      spacing = ' ' * indent_length
      return ''.join(spacing + l for l in str(data).splitlines(True))

    out = '%s(\n' % self.__class__.__name__
    out += '  root_dir: %s\n' % self.root_dir
    out += '  saved_state: %s)' % indent(self.saved_state, 2)
    return out


def load_complete_state(options, cwd, subdir):
  """Loads a CompleteState.

  This includes data from .isolate and .isolated.state files. Never reads the
  .isolated file.

  Arguments:
    options: Options instance generated with OptionParserIsolate.
  """
  if options.isolated:
    # Load the previous state if it was present. Namely, "foo.isolated.state".
    complete_state = CompleteState.load_files(options.isolated)
  else:
    # Constructs a dummy object that cannot be saved. Useful for temporary
    # commands like 'run'.
    complete_state = CompleteState(None, SavedState())
  options.isolate = options.isolate or complete_state.saved_state.isolate_file
  if not options.isolate:
    raise ExecutionError('A .isolate file is required.')
  if (complete_state.saved_state.isolate_file and
      options.isolate != complete_state.saved_state.isolate_file):
    raise ExecutionError(
        '%s and %s do not match.' % (
          options.isolate, complete_state.saved_state.isolate_file))

  # Then load the .isolate and expands directories.
  complete_state.load_isolate(
      cwd, options.isolate, options.variables, options.ignore_broken_items)

  # Regenerate complete_state.saved_state.files.
  if subdir:
    subdir = unicode(subdir)
    subdir = eval_variables(subdir, complete_state.saved_state.variables)
    subdir = subdir.replace('/', os.path.sep)
  complete_state.process_inputs(subdir)
  return complete_state


def read_trace_as_isolate_dict(complete_state):
  """Reads a trace and returns the .isolate dictionary.

  Returns exceptions during the log parsing so it can be re-raised.
  """
  api = trace_inputs.get_api()
  logfile = complete_state.isolated_filepath + '.log'
  if not os.path.isfile(logfile):
    raise ExecutionError(
        'No log file \'%s\' to read, did you forget to \'trace\'?' % logfile)
  try:
    data = api.parse_log(logfile, default_blacklist, None)
    exceptions = [i['exception'] for i in data if 'exception' in i]
    results = (i['results'] for i in data if 'results' in i)
    results_stripped = (i.strip_root(complete_state.root_dir) for i in results)
    files = set(sum((result.existent for result in results_stripped), []))
    tracked, touched = split_touched(files)
    value = generate_isolate(
        tracked,
        [],
        touched,
        complete_state.root_dir,
        complete_state.saved_state.variables,
        complete_state.saved_state.relative_cwd)
    return value, exceptions
  except trace_inputs.TracingFailure, e:
    raise ExecutionError(
        'Reading traces failed for: %s\n%s' %
          (' '.join(complete_state.saved_state.command), str(e)))


def print_all(comment, data, stream):
  """Prints a complete .isolate file and its top-level file comment into a
  stream.
  """
  if comment:
    stream.write(comment)
  pretty_print(data, stream)


def merge(complete_state):
  """Reads a trace and merges it back into the source .isolate file."""
  value, exceptions = read_trace_as_isolate_dict(complete_state)

  # Now take that data and union it into the original .isolate file.
  with open(complete_state.saved_state.isolate_file, 'r') as f:
    prev_content = f.read()
  isolate_dir = os.path.dirname(complete_state.saved_state.isolate_file)
  prev_config = load_isolate_as_config(
      isolate_dir,
      eval_content(prev_content),
      extract_comment(prev_content))
  new_config = load_isolate_as_config(isolate_dir, value, '')
  config = union(prev_config, new_config)
  data = config.make_isolate_file()
  print('Updating %s' % complete_state.saved_state.isolate_file)
  with open(complete_state.saved_state.isolate_file, 'wb') as f:
    print_all(config.file_comment, data, f)
  if exceptions:
    # It got an exception, raise the first one.
    raise \
        exceptions[0][0], \
        exceptions[0][1], \
        exceptions[0][2]


def CMDcheck(args):
  """Checks that all the inputs are present and generates .isolated."""
  parser = OptionParserIsolate(command='check')
  parser.add_option('--subdir', help='Filters to a subdirectory')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)
  complete_state = load_complete_state(options, os.getcwd(), options.subdir)

  # Nothing is done specifically. Just store the result and state.
  complete_state.save_files()
  return 0


def CMDhashtable(args):
  """Creates a hash table content addressed object store.

  All the files listed in the .isolated file are put in the output directory
  with the file name being the sha-1 of the file's content.
  """
  parser = OptionParserIsolate(command='hashtable')
  parser.add_option('--subdir', help='Filters to a subdirectory')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)

  with run_isolated.Profiler('GenerateHashtable'):
    success = False
    try:
      complete_state = load_complete_state(options, os.getcwd(), options.subdir)
      options.outdir = (
          options.outdir or os.path.join(complete_state.resultdir, 'hashtable'))
      # Make sure that complete_state isn't modified until save_files() is
      # called, because any changes made to it here will propagate to the files
      # created (which is probably not intended).
      complete_state.save_files()

      infiles = complete_state.saved_state.files
      # Add all the .isolated files.
      for item in complete_state.saved_state.isolated_files:
        item_path = os.path.join(
            os.path.dirname(complete_state.isolated_filepath), item)
        with open(item_path, 'rb') as f:
          content = f.read()
        isolated_metadata = {
          'h': hashlib.sha1(content).hexdigest(),
          's': len(content),
          'priority': '0'
        }
        infiles[item_path] = isolated_metadata

      logging.info('Creating content addressed object store with %d item',
                   len(infiles))

      if is_url(options.outdir):
        isolateserver_archive.upload_sha1_tree(
            base_url=options.outdir,
            indir=complete_state.root_dir,
            infiles=infiles,
            namespace='default-gzip')
      else:
        recreate_tree(
            outdir=options.outdir,
            indir=complete_state.root_dir,
            infiles=infiles,
            action=run_isolated.HARDLINK,
            as_sha1=True)
      success = True
    finally:
      # If the command failed, delete the .isolated file if it exists. This is
      # important so no stale swarm job is executed.
      if not success and os.path.isfile(options.isolated):
        os.remove(options.isolated)


def CMDmerge(args):
  """Reads and merges the data from the trace back into the original .isolate.

  Ignores --outdir.
  """
  parser = OptionParserIsolate(command='merge', require_isolated=False)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)
  complete_state = load_complete_state(options, os.getcwd(), None)
  merge(complete_state)
  return 0


def CMDread(args):
  """Reads the trace file generated with command 'trace'.

  Ignores --outdir.
  """
  parser = OptionParserIsolate(command='read', require_isolated=False)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)
  complete_state = load_complete_state(options, os.getcwd(), None)
  value, exceptions = read_trace_as_isolate_dict(complete_state)
  pretty_print(value, sys.stdout)
  if exceptions:
    # It got an exception, raise the first one.
    raise \
        exceptions[0][0], \
        exceptions[0][1], \
        exceptions[0][2]
  return 0


def CMDremap(args):
  """Creates a directory with all the dependencies mapped into it.

  Useful to test manually why a test is failing. The target executable is not
  run.
  """
  parser = OptionParserIsolate(command='remap', require_isolated=False)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)
  complete_state = load_complete_state(options, os.getcwd(), None)

  if not options.outdir:
    options.outdir = run_isolated.make_temp_dir(
        'isolate', complete_state.root_dir)
  else:
    if is_url(options.outdir):
      raise ExecutionError('Can\'t use url for --outdir with mode remap')
    if not os.path.isdir(options.outdir):
      os.makedirs(options.outdir)
  print('Remapping into %s' % options.outdir)
  if len(os.listdir(options.outdir)):
    raise ExecutionError('Can\'t remap in a non-empty directory')
  recreate_tree(
      outdir=options.outdir,
      indir=complete_state.root_dir,
      infiles=complete_state.saved_state.files,
      action=run_isolated.HARDLINK,
      as_sha1=False)
  if complete_state.saved_state.read_only:
    run_isolated.make_writable(options.outdir, True)

  if complete_state.isolated_filepath:
    complete_state.save_files()
  return 0


def CMDrewrite(args):
  """Rewrites a .isolate file into the canonical format."""
  parser = OptionParserIsolate(command='rewrite', require_isolated=False)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported argument: %s' % args)

  if options.isolated:
    # Load the previous state if it was present. Namely, "foo.isolated.state".
    complete_state = CompleteState.load_files(options.isolated)
  else:
    # Constructs a dummy object that cannot be saved. Useful for temporary
    # commands like 'run'.
    complete_state = CompleteState(None, SavedState())
  isolate = options.isolate or complete_state.saved_state.isolate_file
  if not isolate:
    raise ExecutionError('A .isolate file is required.')
  with open(isolate, 'r') as f:
    content = f.read()
  config = load_isolate_as_config(
      os.path.dirname(os.path.abspath(isolate)),
      eval_content(content),
      extract_comment(content))
  data = config.make_isolate_file()
  print('Updating %s' % isolate)
  with open(isolate, 'wb') as f:
    print_all(config.file_comment, data, f)
  return 0


def CMDrun(args):
  """Runs the test executable in an isolated (temporary) directory.

  All the dependencies are mapped into the temporary directory and the
  directory is cleaned up after the target exits. Warning: if --outdir is
  specified, it is deleted upon exit.

  Argument processing stops at the first non-recognized argument and these
  arguments are appended to the command line of the target to run. For example,
  use: isolate.py --isolated foo.isolated -- --gtest_filter=Foo.Bar
  """
  parser = OptionParserIsolate(command='run', require_isolated=False)
  parser.enable_interspersed_args()
  options, args = parser.parse_args(args)
  complete_state = load_complete_state(options, os.getcwd(), None)
  cmd = complete_state.saved_state.command + args
  if not cmd:
    raise ExecutionError('No command to run')
  if options.outdir and is_url(options.outdir):
    raise ExecutionError('Can\'t use url for --outdir with mode run')

  cmd = trace_inputs.fix_python_path(cmd)
  try:
    if not options.outdir:
      options.outdir = run_isolated.make_temp_dir(
          'isolate', complete_state.root_dir)
    else:
      if not os.path.isdir(options.outdir):
        os.makedirs(options.outdir)
    recreate_tree(
        outdir=options.outdir,
        indir=complete_state.root_dir,
        infiles=complete_state.saved_state.files,
        action=run_isolated.HARDLINK,
        as_sha1=False)
    cwd = os.path.normpath(
        os.path.join(options.outdir, complete_state.saved_state.relative_cwd))
    if not os.path.isdir(cwd):
      # It can happen when no files are mapped from the directory containing the
      # .isolate file. But the directory must exist to be the current working
      # directory.
      os.makedirs(cwd)
    if complete_state.saved_state.read_only:
      run_isolated.make_writable(options.outdir, True)
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    result = subprocess.call(cmd, cwd=cwd)
  finally:
    if options.outdir:
      run_isolated.rmtree(options.outdir)

  if complete_state.isolated_filepath:
    complete_state.save_files()
  return result


def CMDtrace(args):
  """Traces the target using trace_inputs.py.

  It runs the executable without remapping it, and traces all the files it and
  its child processes access. Then the 'read' command can be used to generate an
  updated .isolate file out of it.

  Argument processing stops at the first non-recognized argument and these
  arguments are appended to the command line of the target to run. For example,
  use: isolate.py --isolated foo.isolated -- --gtest_filter=Foo.Bar
  """
  parser = OptionParserIsolate(command='trace')
  parser.enable_interspersed_args()
  parser.add_option(
      '-m', '--merge', action='store_true',
      help='After tracing, merge the results back in the .isolate file')
  options, args = parser.parse_args(args)
  complete_state = load_complete_state(options, os.getcwd(), None)
  cmd = complete_state.saved_state.command + args
  if not cmd:
    raise ExecutionError('No command to run')
  cmd = trace_inputs.fix_python_path(cmd)
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
    merge(complete_state)

  return result


def add_variable_option(parser):
  """Adds --isolated and --variable to an OptionParser."""
  parser.add_option(
      '-s', '--isolated',
      metavar='FILE',
      help='.isolated file to generate or read')
  # Keep for compatibility. TODO(maruel): Remove once not used anymore.
  parser.add_option(
      '-r', '--result',
      dest='isolated',
      help=optparse.SUPPRESS_HELP)
  default_variables = [('OS', get_flavor())]
  if sys.platform in ('win32', 'cygwin'):
    default_variables.append(('EXECUTABLE_SUFFIX', '.exe'))
  else:
    default_variables.append(('EXECUTABLE_SUFFIX', ''))
  parser.add_option(
      '-V', '--variable',
      nargs=2,
      action='append',
      default=default_variables,
      dest='variables',
      metavar='FOO BAR',
      help='Variables to process in the .isolate file, default: %default. '
            'Variables are persistent accross calls, they are saved inside '
            '<.isolated>.state')


def parse_isolated_option(parser, options, cwd, require_isolated):
  """Processes --isolated."""
  if options.isolated:
    options.isolated = os.path.normpath(
        os.path.join(cwd, options.isolated.replace('/', os.path.sep)))
  if require_isolated and not options.isolated:
    parser.error('--isolated is required. Visit http://chromium.org/developers/'
                 'testing/isolated-testing#TOC-Where-can-I-find-the-.isolated-'
                 'file- to see how to create the .isolated file.')
  if options.isolated and not options.isolated.endswith('.isolated'):
    parser.error('--isolated value must end with \'.isolated\'')


def parse_variable_option(options):
  """Processes --variable."""
  # TODO(benrg): Maybe we should use a copy of gyp's NameValueListToDict here,
  # but it wouldn't be backward compatible.
  def try_make_int(s):
    """Converts a value to int if possible, converts to unicode otherwise."""
    try:
      return int(s)
    except ValueError:
      return s.decode('utf-8')
  options.variables = dict((k, try_make_int(v)) for k, v in options.variables)


class OptionParserIsolate(trace_inputs.OptionParserWithNiceDescription):
  """Adds automatic --isolate, --isolated, --out and --variable handling."""
  def __init__(self, require_isolated=True, **kwargs):
    trace_inputs.OptionParserWithNiceDescription.__init__(
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
        '-o', '--outdir', metavar='DIR',
        help='Directory used to recreate the tree or store the hash table. '
             'Defaults: run|remap: a /tmp subdirectory, others: '
             'defaults to the directory containing --isolated')
    group.add_option(
        '--ignore_broken_items', action='store_true',
        default=bool(os.environ.get('ISOLATE_IGNORE_BROKEN_ITEMS')),
        help='Indicates that invalid entries in the isolated file to be '
             'only be logged and not stop processing. Defaults to True if '
             'env var ISOLATE_IGNORE_BROKEN_ITEMS is set')
    self.add_option_group(group)
    self.require_isolated = require_isolated

  def parse_args(self, *args, **kwargs):
    """Makes sure the paths make sense.

    On Windows, / and \ are often mixed together in a path.
    """
    options, args = trace_inputs.OptionParserWithNiceDescription.parse_args(
        self, *args, **kwargs)
    if not self.allow_interspersed_args and args:
      self.error('Unsupported argument: %s' % args)

    cwd = os.getcwd()
    parse_isolated_option(self, options, cwd, self.require_isolated)
    parse_variable_option(options)

    if options.isolate:
      # TODO(maruel): Work with non-ASCII.
      # The path must be in native path case for tracing purposes.
      options.isolate = unicode(options.isolate).replace('/', os.path.sep)
      options.isolate = os.path.normpath(os.path.join(cwd, options.isolate))
      options.isolate = trace_inputs.get_native_path_case(options.isolate)

    if options.outdir and not is_url(options.outdir):
      options.outdir = unicode(options.outdir).replace('/', os.path.sep)
      # outdir doesn't need native path case since tracing is never done from
      # there.
      options.outdir = os.path.normpath(os.path.join(cwd, options.outdir))

    return options, args


### Glue code to make all the commands works magically.


CMDhelp = trace_inputs.CMDhelp


def main(argv):
  try:
    return trace_inputs.main_impl(argv)
  except (
      ExecutionError,
      run_isolated.MappingError,
      run_isolated.ConfigError) as e:
    sys.stderr.write('\nError: ')
    sys.stderr.write(str(e))
    sys.stderr.write('\n')
    return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
