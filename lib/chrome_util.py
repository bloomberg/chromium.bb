#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Library containing utility functions used for Chrome-specific build tasks.
"""


import functools
import glob
import logging
import os
import shlex
import shutil

from chromite.lib import cros_build_lib
from chromite.lib import osutils


# Taken from external/gyp.git/pylib.
def _NameValueListToDict(name_value_list):
  """
  Takes an array of strings of the form 'NAME=VALUE' and creates a dictionary
  of the pairs.  If a string is simply NAME, then the value in the dictionary
  is set to True.  If VALUE can be converted to an integer, it is.
  """
  result = { }
  for item in name_value_list:
    tokens = item.split('=', 1)
    if len(tokens) == 2:
      # If we can make it an int, use that, otherwise, use the string.
      try:
        token_value = int(tokens[1])
      except ValueError:
        token_value = tokens[1]
      # Set the variable to the supplied value.
      result[tokens[0]] = token_value
    else:
      # No value supplied, treat it as a boolean and set it.
      result[tokens[0]] = True
  return result


def ProcessGypDefines(defines):
  """Validate and convert a string containing GYP_DEFINES to dictionary."""
  assert defines is not None
  return _NameValueListToDict(shlex.split(defines))


class Conditions(object):
  """Functions that return conditions used to construct Path objects.

  Condition functions returned by the public methods have signature
  f(gyp_defines, staging_flags).  For description of gyp_defines and
  staging_flags see docstring for StageChromeFromBuildDir().
  """

  @classmethod
  def _GypSet(cls, flag, value, gyp_defines, _staging_flags):
    val = gyp_defines.get(flag)
    return val == value if value is not None else bool(val)

  @classmethod
  def _GypNotSet(cls, flag, gyp_defines, staging_flags):
    return not cls._GypSet(flag, None, gyp_defines, staging_flags)

  @classmethod
  def _StagingFlagSet(cls, flag, _gyp_defines, staging_flags):
    return bool(staging_flags.get(flag))

  @classmethod
  def GypSet(cls, flag, value=None):
    """Returns condition that tests a gyp flag is set (possibly to a value)."""
    return functools.partial(cls._GypSet, flag, value)

  @classmethod
  def GypNotSet(cls, flag):
    """Returns condition that tests a gyp flag is not set."""
    return functools.partial(cls._GypNotSet, flag)

  @classmethod
  def StagingFlagSet(cls, flag):
    """Returns condition that tests a staging_flag is set."""
    return functools.partial(cls._StagingFlagSet, flag)


class MultipleMatchError(AssertionError):
  """A glob pattern matches multiple files but a non-dir dest was specified."""


class MissingPathError(Exception):
  """An expected path is non-existant."""


class Path(object):
  """Represents an artifact to be copied from build dir to staging dir."""

  def __init__(self, src, cond=None, dest=None, optional=False):
    """Initializes the object.

    Arguments:
      src: The relative path of the artifact.  Can be a file or a directory.
           Can be a glob pattern.
      cond: A condition (see Conditions class) to test for in deciding whether
            to process this artifact.
      dest: Name to give to the target file/directory.  Defaults to keeping the
            same name as the source.
      optional: Whether to enforce the existence of the artifact.  If unset, the
                script errors out if the artifact does not exist.
    """
    self.src = src
    self.cond = cond
    self.optional = optional
    self.dest = dest

  def ShouldProcess(self, gyp_defines, staging_flags):
    """Tests whether this artifact should be copied."""
    if self.cond:
      return self.cond(gyp_defines, staging_flags)
    return True

  @staticmethod
  def _Copy(src, dest):
    def Log(directory):
      sep = ' [d] -> ' if directory else ' -> '
      logging.debug('%s %s %s', src, sep, dest)

    osutils.SafeMakedirs(os.path.dirname(dest))
    src_is_dir = os.path.isdir(src)
    Log(src_is_dir)
    if src_is_dir:
      # copytree() does not know about copying to a containing directory.
      if os.path.isdir(dest):
        dest = os.path.join(dest, os.path.basename(src))
      shutil.copytree(src, dest)
    else:
      shutil.copy(src, dest)

  def Copy(self, src_base, dest_base):
    """Copy artifact(s) from source directory to destination."""
    src = os.path.join(src_base, self.src)
    paths = glob.glob(src)
    if not paths:
      if self.optional:
        logging.info('%s does not exist.  Skipping.', src)
        return
      else:
        raise MissingPathError('%s does not exist and is required.' % src)

    if len(paths) > 1 and self.dest and not self.dest.endswith('/'):
      raise MultipleMatchError(
          'Glob pattern %r has multiple matches, but dest %s '
          'is not a directory.' % (self.src, self.dest))

    for p in paths:
      dest = os.path.join(
          dest_base,
          os.path.relpath(p, src_base) if self.dest is None else self.dest)
      self._Copy(p, dest)


_DISABLE_NACL = 'disable_nacl'
_USE_DRM = 'use_drm'
_USE_PDF = 'use_pdf'

_HIGHDPI_FLAG = 'highdpi'
STAGING_FLAGS = (_HIGHDPI_FLAG,)

_CHROME_DIR = 'opt/google/chrome'
_CHROME_SANDBOX_DEST = 'chrome-sandbox'
C = Conditions


_COPY_PATHS = (
  Path('chrome'),
  Path('chrome_sandbox', dest=_CHROME_SANDBOX_DEST),
  Path('chrome-wrapper'),
  Path('chrome.pak'),
  Path('chrome_100_percent.pak'),
  Path('extensions', optional=True),
  Path('libffmpegsumo.so'),
  Path('libosmesa.so'),
  Path('locales'),
  Path('resources'),
  Path('resources.pak'),
  Path('xdg-settings'),
  Path('*.png'),
  Path('lib.target/*.so', cond=C.GypSet('component', value='shared_library')),
  Path('libppGoogleNaClPluginChrome.so', cond=C.GypNotSet(_DISABLE_NACL)),
  Path('nacl_helper_bootstrap', cond=C.GypNotSet(_DISABLE_NACL)),
  Path('nacl_irt_*.nexe', cond=C.GypNotSet(_DISABLE_NACL)),
  Path('nacl_helper.exe', optional=True, cond=C.GypNotSet(_DISABLE_NACL)),
  Path('ash_shell', cond=C.GypSet(_USE_DRM)),
  Path('aura_demo', cond=C.GypSet(_USE_DRM)),
  Path('libpdf.so', cond=C.GypSet(_USE_PDF)),
  Path('chrome_200_percent.pak', cond=C.StagingFlagSet(_HIGHDPI_FLAG)),
)


def _SetPermissions(staging_dir, dest_base):
  # Set the ownership of all files in staging dir to root:root.
  cros_build_lib.SudoRunCommand(['chown', 'root:root', '-R', staging_dir])

  # Setuid the sandbox after running chown, since chown clears the setuid bit.
  target = os.path.join(dest_base, _CHROME_SANDBOX_DEST)
  cros_build_lib.SudoRunCommand(['chmod', '4755', target])


class StagingError(Exception):
  """Raised by StageChromeFromBuildDir."""
  pass


def StageChromeFromBuildDir(staging_dir, build_dir, gyp_defines, staging_flags):
  """Populates a staging directory with necessary build artifacts.

  Arguments:
    staging_dir: Path to an empty staging directory.
    build_dir: Path to location of Chrome build artifacts.
    gyp_defines: A dictionary (i.e., one returned by ProcessGypDefines)
                 containing GYP_DEFINES Chrome was built with.
    staging_flags: A list of extra staging flags.  Valid flags are specified in
                   STAGING_FLAGS.
  """
  if os.path.exists(staging_dir) and os.listdir(staging_dir):
    raise StagingError('Staging directory %s must be empty.' % staging_dir)

  dest_base = os.path.join(staging_dir, _CHROME_DIR)
  os.makedirs(os.path.join(dest_base, 'plugins'))

  for p in _COPY_PATHS:
    if p.ShouldProcess(gyp_defines, staging_flags):
      p.Copy(build_dir, dest_base)

  _SetPermissions(staging_dir, dest_base)
