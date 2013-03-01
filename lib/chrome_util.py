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


def DictToGypDefines(def_dict):
  """Convert a dict to GYP_DEFINES format."""
  def_list = []
  for k, v in def_dict.iteritems():
    def_list.append("%s='%s'" % (k, v))
  return ' '.join(def_list)


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
    return flag in staging_flags

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


class MissingPathError(SystemExit):
  """An expected path is non-existant."""


class MustNotBeDirError(SystemExit):
  """The specified path should not be a directory, but is."""


class Copier(object):
  """Single file/directory copier.

  Provides destination stripping and permission setting functionality.
  """

  def __init__(self, strip_bin=None, exe_opts=None):
    """Initialization.

    Arguments:
      strip_bin: Path to the program used to strip binaries.  If set to None,
                 binaries will not be stripped.
      exe_opts: Permissions to set on executables.
    """
    self.strip_bin = strip_bin
    self.exe_opts = exe_opts

  def Copy(self, src, dest, exe):
    """Perform the copy.

    Arguments:
      src: The path of the file/directory to copy.
      dest: The exact path of the destination.  Should not already exist.
      exe: If |src| is a file, whether the file is an executable.  If |src| is a
           directory, whether to treat the contents of the directory as
           executables.
    """
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
    elif exe and os.path.getsize(src) > 0:
      if self.strip_bin:
        cros_build_lib.DebugRunCommand([self.strip_bin, '--strip-unneeded',
                                        '-o', dest, src])
      if self.exe_opts is not None:
        os.chmod(dest, self.exe_opts)
    else:
      shutil.copy(src, dest)


class Path(object):
  """Represents an artifact to be copied from build dir to staging dir."""

  def __init__(self, src, exe=False, cond=None, dest=None, optional=False):
    """Initializes the object.

    Arguments:
      src: The relative path of the artifact.  Can be a file or a directory.
           Can be a glob pattern.
      exe: Identifes the path as either being an executable or containing
           executables.  Executables may be stripped during copy, and have
           special permissions set.  We currently only support stripping of
           specified files and glob patterns that return files.  If |src| is a
           directory or contains directories, the content of the directory will
           not be stripped.
      cond: A condition (see Conditions class) to test for in deciding whether
            to process this artifact.
      dest: Name to give to the target file/directory.  Defaults to keeping the
            same name as the source.
      optional: Whether to enforce the existence of the artifact.  If unset, the
                script errors out if the artifact does not exist.
    """
    self.src = src
    self.exe = exe
    self.cond = cond
    self.optional = optional
    self.dest = dest

  def ShouldProcess(self, gyp_defines, staging_flags):
    """Tests whether this artifact should be copied."""
    if self.cond:
      return self.cond(gyp_defines, staging_flags)
    return True

  def Copy(self, src_base, dest_base, copier, ignore_missing=False):
    """Copy artifact(s) from source directory to destination.

    Arguments:
      src_base: The directory to apply the src glob pattern match in.
      dest_base: The directory to copy matched files to.  |Path.dest|.
      copier: A Copier instance that performs the actual file/directory copying.
    """
    src = os.path.join(src_base, self.src)
    paths = glob.glob(src)
    if not paths:
      if self.optional or ignore_missing:
        logging.info('%s does not exist.  Skipping.', src)
        return
      else:
        raise MissingPathError('%s does not exist and is required.' % src)

    if len(paths) > 1 and self.dest and not self.dest.endswith('/'):
      raise MultipleMatchError(
          'Glob pattern %r has multiple matches, but dest %s '
          'is not a directory.' % (self.src, self.dest))

    if not src.endswith('/') and os.path.isdir(src):
      raise MustNotBeDirError('%s must not be a directory' % (src,))

    for p in paths:
      dest = os.path.join(
          dest_base,
          os.path.relpath(p, src_base) if self.dest is None else self.dest)
      copier.Copy(p, dest, self.exe)


_DISABLE_NACL = 'disable_nacl'
_USE_DRM = 'use_drm'
_USE_PDF = 'use_pdf'

_HIGHDPI_FLAG = 'highdpi'
_CONTENT_SHELL_FLAG = 'content_shell'
_WIDEVINE_FLAG = 'widevine'
STAGING_FLAGS = (_HIGHDPI_FLAG, _CONTENT_SHELL_FLAG, _WIDEVINE_FLAG)

_CHROME_DIR = 'opt/google/chrome'
_CHROME_SANDBOX_DEST = 'chrome-sandbox'
C = Conditions


_COPY_PATHS = (
  Path('ash_shell',
       cond=C.GypSet(_USE_DRM)),
  Path('aura_demo',
       cond=C.GypSet(_USE_DRM)),
  Path('chrome',
       exe=True),
  Path('chrome_sandbox',
       dest=_CHROME_SANDBOX_DEST),
  Path('chrome-wrapper'),
  Path('chrome.pak'),
  Path('chrome_100_percent.pak'),
  Path('chrome_200_percent.pak',
       cond=C.StagingFlagSet(_HIGHDPI_FLAG)),
  Path('content_shell',
       cond=C.StagingFlagSet(_CONTENT_SHELL_FLAG)),
  Path('content_shell.pak',
       cond=C.StagingFlagSet(_CONTENT_SHELL_FLAG)),
  Path('extensions/',
       optional=True),
  Path('lib/*.so',
       exe=True,
       cond=C.GypSet('component', value='shared_library')),
  Path('libffmpegsumo.so',
       exe=True),
  Path('libpdf.so',
       exe=True,
       cond=C.GypSet(_USE_PDF)),
  Path('libppGoogleNaClPluginChrome.so',
       exe=True,
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('libosmesa.so',
       exe=True),
  Path('libwidevinecdmadapter.so',
       exe=True,
       cond=C.StagingFlagSet(_WIDEVINE_FLAG)),
  Path('libwidevinecdm.so',
       exe=True,
       cond=C.StagingFlagSet(_WIDEVINE_FLAG)),
  Path('locales/'),
  Path('nacl_helper_bootstrap',
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('nacl_irt_*.nexe',
       exe=True,
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('nacl_helper',
       exe=True, optional=True,
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('resources/'),
  Path('resources.pak'),
  Path('xdg-settings'),
  Path('*.png'),
)


def _FixPermissions(dest_base):
  """Last minute permission fixes."""
  # Set the suid bit for the chrome sandbox.
  # TODO(rcui): Implement this through a permission mask attribute in the Path
  # class.
  cros_build_lib.DebugRunCommand(['chmod', '-R', 'a+r', dest_base])
  cros_build_lib.DebugRunCommand(
      ['find', dest_base, '-perm', '/110', '-exec', 'chmod', 'a+x', '{}', '+'])
  target = os.path.join(dest_base, _CHROME_SANDBOX_DEST)
  if os.path.exists(target):
    cros_build_lib.DebugRunCommand(['chmod', '4755', target])


class StagingError(Exception):
  """Raised by StageChromeFromBuildDir."""
  pass


def StageChromeFromBuildDir(staging_dir, build_dir, strip_bin, strict=False,
                            gyp_defines=None, staging_flags=None):
  """Populates a staging directory with necessary build artifacts.

  If |strict| is set, then we decide what to stage based on the |gyp_defines|
  and |staging_flags| passed in.  Otherwise, we stage everything that we know
  about, that we can find.

  Arguments:
    staging_dir: Path to an empty staging directory.
    build_dir: Path to location of Chrome build artifacts.
    strip_bin: Path to executable used for stripping binaries.
    strict: If set, decide what to stage based on the |gyp_defines| and
      |staging_flags| passed in.  Otherwise, we stage everything that we know
      about, that we can find.
    gyp_defines: A dictionary (i.e., one returned by ProcessGypDefines)
      containing GYP_DEFINES Chrome was built with.
    staging_flags: A list of extra staging flags.  Valid flags are specified in
      STAGING_FLAGS.
  """
  if os.path.exists(staging_dir) and os.listdir(staging_dir):
    raise StagingError('Staging directory %s must be empty.' % staging_dir)

  dest_base = os.path.join(staging_dir, _CHROME_DIR)
  osutils.SafeMakedirs(os.path.join(dest_base, 'plugins'))
  cros_build_lib.DebugRunCommand(['chmod', '-R', '0755', staging_dir])

  if gyp_defines is None:
    gyp_defines = {}
  if staging_flags is None:
    staging_flags = []

  copier = Copier(strip_bin=strip_bin, exe_opts=0755)
  for p in _COPY_PATHS:
    if not strict or p.ShouldProcess(gyp_defines, staging_flags):
      p.Copy(build_dir, dest_base, copier, ignore_missing=(not strict))

  _FixPermissions(dest_base)
