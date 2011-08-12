#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that resets your Chrome GIT checkout."""

import optparse
import os
import sys

# Want to use correct version of libraries even when executed through symlink.
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),
                             '..', '..'))
import chromite.lib.cros_build_lib as cros_lib


_SUPPORTED_COMMANDS = ['runhooks']
_CHROMIUM_SRC_ROOT = 'chromium/src'
_CHROMIUM_CROS_DIR = 'chromium/src/third_party/cros'
_PLATFORM_CROS_DIR = 'src/platform/cros'
_PLATFORM_CROS_NAME = 'chromiumos/platform/cros'


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


def _CreateCrosSymlink(repo_root):
  """Create the src/third_party/cros symlink that chromium needs to build."""
  platform_cros_dir = os.path.join(repo_root, _PLATFORM_CROS_DIR)
  if not os.path.isdir(platform_cros_dir):
    cros_lib.Die('Repository %s not found!  Please add project %s to your'
                 'manifest' % (platform_cros_dir, _PLATFORM_CROS_NAME))

  chromium_cros_dir = os.path.join(os.path.join(repo_root, _CHROMIUM_CROS_DIR))
  rel_link_source = os.path.relpath(platform_cros_dir,
                                    os.path.dirname(chromium_cros_dir))
  if not os.path.exists(chromium_cros_dir):
    print rel_link_source
    os.symlink(rel_link_source, chromium_cros_dir)


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


def _ParseCommand(inputs):
  """Get the command, and quit on erros.

  Args:
    inputs: A list of positional (non-option) arguments returned by optparse.
  """
  cmd = ''
  if len(inputs) == 1:
    cmd = inputs[0]
    if cmd not in _SUPPORTED_COMMANDS:
      cros_lib.Die('Unsupported command %s.  Use one of:\n%s' %
                   (cmd, '\n'.join(_SUPPORTED_COMMANDS)))
  elif len(inputs) > 1:
    cros_lib.Die('More than one command specified')

  return cmd


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

  merged_deps = deps['deps']
  unix_deps = deps['deps_os']['unix']
  assert(not set(merged_deps.keys()).intersection(set(unix_deps)))
  merged_deps.update(unix_deps)
  return deps, merged_deps


def main(argv=None):
  if not argv:
    argv = sys.argv[1:]

  usage = ('usage: %prog [-d <DEPS.git file>] [command]\n'
           '\n'
           'commands:\n'
           "  runhooks - Only run hooks.  Don't reset dependent repositories.")
  epilog = ('\n'
            'When no command is given, defaults to resetting dependent\n'
            'repositories to revision specified in .DEPS.git file, and runs\n'
            'the defined hooks.\n')

  parser = optparse.OptionParser(usage=usage, epilog=epilog)

  # TODO(rcui): have -d accept a URL
  parser.add_option('-d', '--deps', default=None,
                    help=('DEPS file to use. Defaults to '
                         '<chrome_src_root>/.DEPS.git'))
  parser.add_option('--internal', action='store_false', dest='internal',
                    default=True, help='Allow chrome-internal URLs')
  (options, inputs) = parser.parse_args(argv)

  cmd = _ParseCommand(inputs)

  run_reset = True
  run_hooks = True
  if cmd == 'runhooks':
    # The command is currently used by the chromeos-chrome-9999 ebuild.
    # TODO(rcui): update comment when chrome stable ebuilds also use runhooks.
    run_reset = False

  repo_root = cros_lib.FindRepoCheckoutRoot()
  chromium_src_root = os.path.join(repo_root, _CHROMIUM_SRC_ROOT)
  if not os.path.isdir(chromium_src_root):
    cros_lib.Die('chromium src/ dir not found')

  deps_file = os.path.join(chromium_src_root, '.DEPS.git')
  if options.deps:
    deps_file = options.deps

  deps, merged_deps = _GetParsedDeps(deps_file)

  chromium_root = os.path.dirname(chromium_src_root)
  _CreateCrosSymlink(repo_root)
  if run_reset: _ResetGitCheckout(chromium_root, merged_deps)
  if run_hooks: _RunHooks(chromium_root, deps['hooks'])


if __name__ == '__main__':
  main()
