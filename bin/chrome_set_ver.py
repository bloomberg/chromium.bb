#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that resets your Chrome GIT checkout."""

import optparse
import os
import sys

import chromite.lib.cros_build_lib as cros_lib


_CHROMIUM_SRC_ROOT = 'chromium/src'


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
  """Reset a git repo to the specified commit hash"""
  cros_lib.RunCommand(['git', 'checkout', commit_hash],  print_cmd=False,
                      redirect_stdout=True, redirect_stderr=True,
                      cwd=project_path)


def _ResetGitCheckout(chromium_root, deps):
  """Reset chrome repos to hashes specified in the DEPS file.

  Arguments:
   chromium_root: directory where chromium is checked out - level above 'src'.
   deps: a dictionary indexed by repo paths.  The same format as the 'deps'
         entry in the '.DEPS.git' file.
  """
  for rel_path, project_hash in deps.iteritems():
    print 'resetting project %s' % rel_path
    repo_url, commit_hash = project_hash.split('@')
    _ResetProject(os.path.join(chromium_root, rel_path), commit_hash)


def _RunHooks(chromium_root, hooks):
  """Run the hooks contained in the DEPS file.

  Arguments:
    chromium_root: directory where chromium is checked out - level above 'src'.
    hooks: a list of hook dictionaries.  The same format as the 'hooks' entry
           in the '.DEPS.git' file.
  """
  for hook in hooks:
    cros_lib.RunCommand(hook['action'], cwd=chromium_root)


def main(argv=None):
  if not argv:
    argv = sys.argv[1:]

  parser = optparse.OptionParser(usage='usage: %prog -d .DEPS.git')
  # TODO(rcui): have -d accept a URL
  parser.add_option('-d', '--deps', default=None,
                    help=('DEPS file to use. Defaults to '
                         '<chrome_src_root>/.DEPS.git'))
  parser.add_option('--internal', action='store_false', dest='internal',
                    default=True, help='Allow chrome-internal URLs')
  (options, inputs) = parser.parse_args(argv)

  chromium_src_root = os.path.join(os.path.dirname(cros_lib.FindRepoDir()),
                                   _CHROMIUM_SRC_ROOT)
  if not os.path.isdir(chromium_src_root):
    sys.exit('chromium src/ dir not found')

  deps_file = os.path.join(chromium_src_root, '.DEPS.git')
  if options.deps:
    deps_file = options.deps

  deps = _LoadDEPS(open(deps_file, 'rU').read())

  merged_deps = deps['deps']
  unix_deps = deps['deps_os']['unix']
  assert(not set(merged_deps.keys()).intersection(set(unix_deps)))
  merged_deps.update(unix_deps)

  chromium_root = os.path.dirname(chromium_src_root)
  _ResetGitCheckout(chromium_root, merged_deps)
  _RunHooks(chromium_root, deps['hooks'])


if __name__ == '__main__':
  main()
