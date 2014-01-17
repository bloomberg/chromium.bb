# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run lint checks on the specified files."""

import os
import sys

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git

from chromite import cros


def _GetProjectPath(path):
  """Find the absolute path of the git checkout that contains |path|."""
  manifest = git.ManifestCheckout.Cached(path)
  return manifest.FindCheckoutFromPath(path).GetPath(absolute=True)


def _GetPylintGroups(paths):
  """Return a dictionary mapping pylintrc files to lists of paths."""
  groups = {}
  for path in paths:
    if path.endswith('.py'):
      path = os.path.realpath(path)
      project_path = _GetProjectPath(path)
      parent = os.path.dirname(path)
      while project_path and parent.startswith(project_path):
        pylintrc = os.path.join(parent, 'pylintrc')
        if os.path.isfile(pylintrc):
          break
        parent = os.path.dirname(parent)
      if project_path is None or not os.path.isfile(pylintrc):
        pylintrc = os.path.join(constants.SOURCE_ROOT, 'chromite', 'pylintrc')
      groups.setdefault(pylintrc, []).append(path)
  return groups


def _GetPythonPath(paths):
  """Return the set of Python library paths to use."""
  return sys.path + [
    # Add the Portage installation inside the chroot to the Python path.
    # This ensures that scripts that need to import portage can do so.
    os.path.join(constants.SOURCE_ROOT, 'chroot', 'usr', 'lib', 'portage',
                 'pym'),

    # Scripts outside of chromite expect the scripts in src/scripts/lib to
    # be importable.
    os.path.join(constants.CROSUTILS_DIR, 'lib'),

    # Allow platform projects to be imported by name (e.g. crostestutils).
    os.path.join(constants.SOURCE_ROOT, 'src', 'platform'),

    # Ideally we'd modify meta_path in pylint to handle our virtual chromite
    # module, but that's not possible currently.  We'll have to deal with
    # that at some point if we want `cros lint` to work when the dir is not
    # named 'chromite'.
    constants.SOURCE_ROOT,

    # Also allow scripts to import from their current directory.
  ] + list(set(os.path.dirname(x) for x in paths))


@cros.CommandDecorator('lint')
class LintCommand(cros.CrosCommand):
  """Run lint checks on the specified files."""

  EPILOG = """
Right now, cros lint just runs pylint on *.py files, but we may also in future
run other checks (e.g. pyflakes, cpplint, etc.)
"""

  @classmethod
  def AddParser(cls, parser):
    super(LintCommand, cls).AddParser(parser)
    parser.add_argument('files', help='Files to lint', nargs='*')

  def Run(self):
    files = self.options.files
    if not files:
      # Running with no arguments is allowed to make the repo upload hook
      # simple, but print a warning so that if someone runs this manually
      # they are aware that nothing was linted.
      cros_build_lib.Warning('No files provided to lint.  Doing nothing.')

    errors = False
    for pylintrc, paths in sorted(_GetPylintGroups(files).items()):
      paths = sorted(list(set([os.path.realpath(x) for x in paths])))
      cmd = ['pylint', '--rcfile=%s' % pylintrc] + paths
      extra_env = {'PYTHONPATH': ':'.join(_GetPythonPath(paths))}
      res = cros_build_lib.RunCommand(cmd, extra_env=extra_env,
                                      error_code_ok=True,
                                      print_cmd=self.options.debug)
      if res.returncode != 0:
        errors = True

    if errors:
      sys.exit(1)
