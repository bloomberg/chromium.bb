#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that resets your Chrome GIT checkout."""

import optparse
import os
import sys
import urlparse

# Want to use correct version of libraries even when executed through symlink.
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),
                             '..', '..'))
import chromite.lib.cros_build_lib as cros_lib


_CHROMIUM_ROOT = 'chromium'
_CHROMIUM_SRC_ROOT = os.path.join(_CHROMIUM_ROOT, 'src')
_CHROMIUM_SRC_INTERNAL = os.path.join(_CHROMIUM_ROOT, 'src-internal')
_CHROMIUM_CROS_DEPS = os.path.join(_CHROMIUM_SRC_ROOT, 'tools/cros.DEPS/DEPS')


def _LoadDEPS(deps_content):
  """Load contents of DEPS file into a dictionary.

  Arguments:
    deps_content: The contents of the .DEPS.git file.

  Returns:
    A dictionary indexed by the top level items in the structure - i.e.,
    'hooks', 'deps', 'os_deps'.
  """
  class FromImpl:
    """From syntax is not supported."""
    def __init__(self, module_name, var_name):
      raise NotImplementedError('The From() syntax is not supported in'
                                'chrome_set_ver.')

  class _VarImpl:
    def __init__(self,custom_vars, local_scope):
      self._custom_vars = custom_vars
      self._local_scope = local_scope

    def Lookup(self, var_name):
      """Implements the Var syntax."""
      if var_name in self._custom_vars:
        return self._custom_vars[var_name]
      elif var_name in self._local_scope.get('vars', {}):
        return self._local_scope['vars'][var_name]
      raise Exception('Var is not defined: %s' % var_name)

  locals = {}
  var = _VarImpl({}, locals)
  globals = {'From': FromImpl, 'Var': var.Lookup, 'deps_os': {}}
  exec(deps_content) in globals, locals
  return locals


def _ResetProject(project_path, commit_hash):
  """Reset a git repo to the specified commit hash."""
  cros_lib.RunCommand(['git', 'checkout', commit_hash],  print_cmd=False,
                      redirect_stdout=True, redirect_stderr=True,
                      cwd=project_path)


def _ExtractProjectFromEntry(entry):
  """From a deps entry extract the Gerrit project name.

  Arguments:
    entry: The deps entry in the format ${url_prefix}/${project_name}@${hash}
  """
  # We only support Gerrit urls, where the path is the project name.
  repo_url = entry.partition('@')[0]
  project = urlparse.urlparse(repo_url).path.lstrip('/')
  project = os.path.splitext(project)[0]
  return project


def GetPathToProjectMappings(deps):
  """Get dictionary relating path to Gerrit project names.

  Arguments:
   deps: a dictionary indexed by repo paths.  The same format as the 'deps'
         entry in the '.DEPS.git' file.
  """
  mappings = {}
  for rel_path, entry in deps.iteritems():
    mappings[rel_path] = _ExtractProjectFromEntry(entry)

  return mappings


def _CreateCrosSymlink(repo_root):
  """Create symlinks to CrOS projects specified in the cros_DEPS/DEPS file."""
  cros_deps_file = os.path.join(repo_root, _CHROMIUM_CROS_DEPS)
  deps, merged_deps = _GetParsedDeps(cros_deps_file)
  chromium_root = os.path.join(repo_root, _CHROMIUM_ROOT)

  mappings = GetPathToProjectMappings(merged_deps)
  for rel_path, project in mappings.iteritems():
    link_dir = os.path.join(chromium_root, rel_path)
    target_dir = os.path.join(repo_root,
                              cros_lib.GetProjectDir(repo_root, project))
    path_to_target = os.path.relpath(target_dir, os.path.dirname(link_dir))
    if not os.path.exists(link_dir):
      os.symlink(path_to_target, link_dir)


def _ResetGitCheckout(chromium_root, deps):
  """Reset chrome repos to hashes specified in the DEPS file.

  Arguments:
   chromium_root: directory where chromium is checked out - level above 'src'.
   deps: a dictionary indexed by repo paths.  The same format as the 'deps'
         entry in the '.DEPS.git' file.
  """
  for rel_path, project_hash in deps.iteritems():
    print 'resetting project %s' % rel_path
    repo_url, _, commit_hash = project_hash.partition('@')
    abs_path = os.path.join(chromium_root, rel_path)
    if not os.path.isdir(abs_path):
      cros_lib.Die('Cannot find project %s. Expecting project to be '
                   'checked out to %s.\n' % (repo_url, abs_path))
    if commit_hash:
      _ResetProject(abs_path, commit_hash)


def _RunHooks(chromium_root, hooks):
  """Run the hooks contained in the DEPS file.

  Arguments:
    chromium_root: directory where chromium is checked out - level above 'src'.
    hooks: a list of hook dictionaries.  The same format as the 'hooks' entry
           in the '.DEPS.git' file.
  """
  for hook in hooks:
    cros_lib.RunCommand(hook['action'], cwd=chromium_root)


def _MergeDeps(dest, update):
  """Merge the dependencies specified in two dictionaries.

  Arguments:
    dest: The dictionary that will be merged into.
    update: The dictionary whose elements will be merged into dest.
  """
  assert(not set(dest.keys()).intersection(set(update.keys())))
  dest.update(update)
  return dest


def _GetParsedDeps(deps_file):
  """Returns the full parsed DEPS file dictionary, and merged deps.

  Arguments:
    deps_file: Path to the .DEPS.git file

  Returns:
    An (x,y) tuple.  x is a dictionary containing the contents of the DEPS file,
    and y is a dictionary containing the result of merging unix and common deps.
  """
  with open(deps_file, 'rU') as f:
    deps = _LoadDEPS(f.read())

  merged_deps = deps.get('deps', {})
  unix_deps = deps.get('deps_os', {}).get('unix', {})
  merged_deps = _MergeDeps(merged_deps, unix_deps)
  return deps, merged_deps


def main(argv=None):
  if not argv:
    argv = sys.argv[1:]

  usage = 'usage: %prog [-d <DEPS.git file>] [command]'
  parser = optparse.OptionParser(usage=usage)

  # TODO(rcui): have -d accept a URL
  parser.add_option('-d', '--deps', default=None,
                    help=('DEPS file to use. Defaults to '
                         '<chrome_src_root>/.DEPS.git'))
  parser.add_option('--internal', action='store_false', dest='internal',
                    default=True, help='Allow chrome-internal URLs')
  parser.add_option('--runhooks', action='store_true', dest='runhooks',
                    default=False, help="Run hooks as well.")
  (options, inputs) = parser.parse_args(argv)

  repo_root = cros_lib.FindRepoCheckoutRoot()
  chromium_src_root = os.path.join(repo_root, _CHROMIUM_SRC_ROOT)
  if not os.path.isdir(chromium_src_root):
    cros_lib.Die('chromium src/ dir not found')

  # Add DEPS files to parse
  deps_files_to_parse = []
  if options.deps:
    deps_files_to_parse.append(options.deps)
  else:
    deps_files_to_parse.append(os.path.join(chromium_src_root, '.DEPS.git'))

  internal_deps = os.path.join(repo_root, _CHROMIUM_SRC_INTERNAL, '.DEPS.git')
  if os.path.exists(internal_deps):
    deps_files_to_parse.append(internal_deps)

  deps_files_to_parse.append(os.path.join(repo_root, _CHROMIUM_CROS_DEPS))

  # Prepare source tree for resetting
  chromium_root = os.path.join(repo_root, _CHROMIUM_ROOT)
  _CreateCrosSymlink(repo_root)

  # Parse DEPS files and store hooks
  hook_dicts = []
  for deps_file in deps_files_to_parse:
    deps, merged_deps = _GetParsedDeps(deps_file)
    _ResetGitCheckout(chromium_root, merged_deps)
    hook_dicts.append(deps.get('hooks', {}))

  # Run hooks after checkout has been reset properly
  if options.runhooks:
    for hooks in hook_dicts:
      _RunHooks(chromium_root, hooks)


if __name__ == '__main__':
  try:
    main()
  except cros_lib.RunCommandError, e:
    cros_lib.Die('Failed to run a command.\n' + e.args[0])
