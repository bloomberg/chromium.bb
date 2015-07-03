#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements a standard mechanism for Chrome Infra Python environment setup.

This library provides a central location to define Chrome Infra environment
setup. It also provides several faculties to install this environment.

Within a cooperating script, the environment can be setup by importing this
module and running its 'Install' method:

  # Install Chrome-Infra environment (replaces 'sys.path').
  sys.path.insert(0,
      os.path.join(os.path.dirname(__file__), os.pardir, ...))
      # (/path/to/build/scripts)
  import common.env
  common.env.Install()

When attempting to export the Chrome Infra path to external scripts, this
script can be invoked as an executable with various subcommands to emit a valid
PYTHONPATH clause.

In addition, this module has several functions to construct the path.

The goal is to deploy this module universally among Chrome-Infra scripts,
BuildBot configurations, tool invocations, and tests to ensure that they all
execute with the same centrally-defined environment.
"""

import argparse
import collections
import contextlib
import imp
import itertools
import os
import sys
import traceback


# Export for bootstrapping.
__all__ = [
    'Install',
    'PythonPath',
    ]


# Name of enviornment extension file to seek.
ENV_EXTENSION_NAME = 'environment.cfg.py'

# Standard directories (based on this file's location in the <build> tree).
def path_if(*args):
  if not all(args):
    return None
  path = os.path.abspath(os.path.join(*args))
  return (path) if os.path.exists(path) else (None)

# The path to the <build> directory in which this script resides.
Build = path_if(os.path.dirname(__file__), os.pardir, os.pardir)
# The path to the <build_internal> directory.
BuildInternal = path_if(Build, os.pardir, 'build_internal')


def SetPythonPathEnv(value):
  """Sets the system's PYTHONPATH environemnt variable.

  Args:
    value (str): The value to use. If this is empty/None, the system's
        PYTHONPATH will be cleared.
  """
  # Since we can't assign None to the environment "dictionary", we have to
  # either set or delete the key depending on the original value.
  if value is not None:
    os.environ['PYTHONPATH'] = str(value)
  else:
    os.environ.pop('PYTHONPATH', None)


def Install(**kwargs):
  """Replaces the current 'sys.path' with a hermetic Chrome-Infra path.

  Args:
    kwargs (dict): See GetInfraPythonPath arguments.

  Returns (PythonPath): The PythonPath object that was installed.
  """
  infra_python_path = GetInfraPythonPath(**kwargs)
  infra_python_path.Install()
  return infra_python_path


def SplitPath(path):
  """Returns (list): A list of path elements.

  Splits a path into path elements. For example (assuming '/' is the local
  system path separator):
    >>> print SplitPath('/a/b/c/d')
    ['/', 'a', 'b', 'c', 'd']
    >>> print SplitPath('a/b/c')
    ['a', 'b,' 'c']
  """
  parts = []
  while True:
    path, component = os.path.split(path)
    if not component:
      if path:
        parts.append(path)
      break
    parts.append(component)
  parts.reverse()
  return parts


def ExtendPath(base, root_dir):
  """Returns (PythonPath): The extended python path.

  This method looks for the ENV_EXTENSION_NAME file within "root_dir". If
  present, it will be loaded as a Python module and have its "Extend" method
  called.

  If no extension is found, the base PythonPath will be returned.

  Args:
    base (PythonPath): The base python path.
    root_dir (str): The path to check for an extension.
  """
  extension_path = os.path.join(root_dir, ENV_EXTENSION_NAME)
  if not os.path.isfile(extension_path):
    return base
  with open(extension_path, 'r') as fd:
    extension = fd.read()
  extension_module = imp.new_module('env-extension')

  # Execute the enviornment extension.
  try:
    exec extension in extension_module.__dict__

    extend_func = getattr(extension_module, 'Extend', None)
    assert extend_func, (
        "The environment extension module is missing the 'Extend()' method.")
    base = extend_func(base, root_dir)
    if not isinstance(base, PythonPath):
      raise TypeError("Extension module returned non-PythonPath object (%s)" % (
          type(base).__name__,))
  except Exception:
    # Re-raise the exception, but include the configuration file name.
    tb = traceback.format_exc()
    raise RuntimeError("Environment extension [%s] raised exception: %s" % (
        extension_path, tb))
  return base


def IsSystemPythonPath(path):
  """Returns (bool): If a python path is user-installed.

  Paths that are known to be user-installed paths can be ignored when setting
  up a hermetic Python path environment to avoid user libraries that would not
  be present in other environments falsely affecting code.

  This function can be updated as-needed to exclude other non-system paths
  encountered on bots and in the wild.
  """
  components = SplitPath(path)
  for component in components:
    if component in ('dist-packages', 'site-packages'):
      return False
  return True


class PythonPath(collections.Sequence):
  """An immutable set of Python path elements.

  All paths represented in this structure are absolute. If a relative path
  is passed into this structure, it will be converted to absolute based on
  the current working directory (via os.path.abspath).
  """

  def __init__(self, components=None):
    """Initializes a new PythonPath instance.

    Args:
      components (list): A list of path component strings.
    """
    seen = set()
    self._components = []
    for component in (components or ()):
      component = os.path.abspath(component)
      assert isinstance(component, basestring), (
          "Path component '%s' is not a string (%s)" % (
                component, type(component).__name__))
      if component in seen:
        continue
      seen.add(component)
      self._components.append(component)

  def __getitem__(self, value):
    return self._components[value]

  def __len__(self):
    return len(self._components)

  def __iadd__(self, other):
    return self.Append(other)

  def __repr__(self):
    return self.pathstr

  def __eq__(self, other):
    assert isinstance(other, type(self))
    return self._components == other._components

  @classmethod
  def Flatten(cls, *paths):
    """Returns (list): A single-level list containing flattened path elements.

    >>> print PythonPath.Flatten('a', ['b', ['c', 'd']])
    ['a', 'b', 'c', 'd']
    """
    result = []
    for path in paths:
      if not isinstance(path, basestring):
        # Assume it's an iterable of paths.
        result += cls.Flatten(*path)
      else:
        result.append(path)
    return result

  @classmethod
  def FromPaths(cls, *paths):
    """Returns (PythonPath): A PythonPath instantiated from path elements.

    Args:
      paths (tuple): A tuple of path elements or iterables containing path
          elements (e.g., PythonPath instances).
    """
    return cls(cls.Flatten(*paths))

  @classmethod
  def FromPathStr(cls, pathstr):
    """Returns (PythonPath): A PythonPath instantiated from the path string.

    Args:
      pathstr (str): An os.pathsep()-delimited path string.
    """
    return cls(pathstr.split(os.pathsep))

  @property
  def pathstr(self):
    """Returns (str): A path string for the instance's path elements."""
    return os.pathsep.join(self)

  def IsHermetic(self):
    """Returns (bool): True if this instance contains only system paths."""
    return all(IsSystemPythonPath(p) for p in self)

  def GetHermetic(self):
    """Returns (PythonPath): derivative PythonPath containing only system paths.
    """
    return type(self).FromPaths(*(p for p in self if IsSystemPythonPath(p)))

  def Append(self, *paths):
    """Returns (PythonPath): derivative PythonPath with paths added to the end.

    Args:
      paths (tuple): A tuple of path elements to append to the current instance.
    """
    return type(self)(itertools.chain(self, self.FromPaths(*paths)))

  def Override(self, *paths):
    """Returns (PythonPath): derivative PythonPath with paths prepended.

    Args:
      paths (tuple): A tuple of path elements to prepend to the current
          instance.
    """
    return self.FromPaths(*paths).Append(self)

  def Install(self):
    """Overwrites Python runtime variables based on the current instance.

    Performs the following operations:
      - Replaces sys.path with the current instance's path.
      - Replaces os.environ['PYTHONPATH'] with the current instance's path
            string.
    """
    sys.path = list(self)
    SetPythonPathEnv(self.pathstr)

  @contextlib.contextmanager
  def Enter(self):
    """Context manager wrapper for Install.

    On exit, the context manager will restore the original environment.
    """
    orig_sys_path = sys.path[:]
    orig_pythonpath = os.environ.get('PYTHONPATH')

    try:
      self.Install()
      yield
    finally:
      sys.path = orig_sys_path
      SetPythonPathEnv(orig_pythonpath)


def GetSysPythonPath(hermetic=True):
  """Returns (PythonPath): A path based on 'sys.path'.

  Args:
    hermetic (bool): If True, prune any non-system path.
  """
  path = PythonPath.FromPaths(*sys.path)
  if hermetic:
    path = path.GetHermetic()
  return path


def GetEnvPythonPath():
  """Returns (PythonPath): A path based on the PYTHONPATH environment variable.
  """
  pythonpath = os.environ.get('PYTHONPATH')
  if not pythonpath:
    return PythonPath.FromPaths()
  return PythonPath.FromPathStr(pythonpath)


def GetMasterPythonPath(master_dir):
  """Returns (PythonPath): A path including a BuildBot master's directory.

  Args:
    master_dir (str): The BuildBot master root directory.
  """
  return PythonPath.FromPaths(master_dir)


def GetBuildPythonPath():
  """Returns (PythonPath): The Chrome Infra build path."""
  build_path = PythonPath.FromPaths()
  for extension_dir in (
      Build,
      BuildInternal,
      ):
    if extension_dir:
      build_path = ExtendPath(build_path, extension_dir)
  return build_path


def GetInfraPythonPath(hermetic=True, master_dir=None):
  """Returns (PythonPath): The full working Chrome Infra utility path.

  This path is consistent for master, slave, and tool usage. It includes (in
  this order):
    - Any environment PYTHONPATH overrides.
    - If 'master_dir' is supplied, the master's python path component.
    - The Chrome Infra build path.
    - The system python path.

  Args:
    hermetic (bool): True, prune any non-system path from the system path.
    master_dir (str): If not None, include a master path component.
  """
  path = GetEnvPythonPath()
  if master_dir:
    path += GetMasterPythonPath(master_dir)
  path += GetBuildPythonPath()
  path += GetSysPythonPath(hermetic=hermetic)
  return path


def _InfraPathFromArgs(args):
  """Returns (PythonPath): A PythonPath populated from command-line arguments.

  Args:
    args (argparse.Namespace): The command-line arguments constructed by 'main'.
  """
  return GetInfraPythonPath(
      master_dir=args.master_dir,
  )


def _Command_Echo(args, path):
  """Returns (int): Return code.

  Command function for the 'echo' subcommand. Outputs the path string for
  'path'.

  Args:
    args (argparse.Namespace): The command-line arguments constructed by 'main'.
    path (PythonPath): The python path to use.
  """
  args.output.write(path.pathstr)
  return 0


def _Command_Print(args, path):
  """Returns (int): Return code.

  Command function for the 'print' subcommand. Outputs each path component in
  path on a separate line.

  Args:
    args (argparse.Namespace): The command-line arguments constructed by 'main'.
    path (PythonPath): The python path to use.
  """
  for component in path:
    print >>args.output, component
  return 0


def main():
  """Main execution function."""
  parser = argparse.ArgumentParser()
  parser.add_argument('-M', '--master_dir',
      help="Augment the path with the master's directory.")
  parser.add_argument('-o', '--output', metavar='PATH',
      type=argparse.FileType('w'), default='-',
      help="File to output to (use '-' for STDOUT).")

  subparsers = parser.add_subparsers()

  # 'echo'
  subparser = subparsers.add_parser('echo')
  subparser.set_defaults(func=_Command_Echo)

  # 'print'
  subparser = subparsers.add_parser('print')
  subparser.set_defaults(func=_Command_Print)

  # Parse
  args = parser.parse_args()

  # Execute our subcommand function, which will return the exit code.
  path = _InfraPathFromArgs(args)
  return args.func(args, path)


if __name__ == '__main__':
  sys.exit(main())
