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

from chromite.buildbot import cbuildbot_results as results_lib
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


class MultipleMatchError(results_lib.StepFailure):
  """A glob pattern matches multiple files but a non-dir dest was specified."""


class MissingPathError(results_lib.StepFailure):
  """An expected path is non-existant."""


class MustNotBeDirError(results_lib.StepFailure):
  """The specified path should not be a directory, but is."""


class Copier(object):
  """File/directory copier.

  Provides destination stripping and permission setting functionality.
  """

  def __init__(self, strip_bin=None, strip_flags=None, default_mode=0644,
               dir_mode=0755, exe_mode=0755):
    """Initialization.

    Arguments:
      strip_bin: Path to the program used to strip binaries.  If set to None,
                 binaries will not be stripped.
      strip_flags: A list of flags to pass to the |strip_bin| executable.
      default_mode: Default permissions to set on files.
      dir_mode: Mode to set for directories.
      exe_mode: Permissions to set on executables.
    """
    self.strip_bin = strip_bin
    self.strip_flags = strip_flags
    self.default_mode = default_mode
    self.dir_mode = dir_mode
    self.exe_mode = exe_mode

  @staticmethod
  def Log(src, dest, directory):
    sep = ' [d] -> ' if directory else ' -> '
    logging.debug('%s %s %s', src, sep, dest)

  def _CopyFile(self, src, dest, path):
    """Perform the copy.

    Arguments:
      src: The path of the file/directory to copy.
      dest: The exact path of the destination.  Should not already exist.
      path: The Path instance containing copy operation modifiers (such as
            Path.exe, Path.strip, etc.)
    """
    assert not os.path.isdir(src), '%s: Not expecting a directory!' % src
    osutils.SafeMakedirs(os.path.dirname(dest), mode=self.dir_mode)
    if path.exe and self.strip_bin and path.strip and os.path.getsize(src) > 0:
      strip_flags = (['--strip-unneeded'] if self.strip_flags is None else
                     self.strip_flags)
      cros_build_lib.DebugRunCommand(
          [self.strip_bin] + strip_flags + ['-o', dest, src])
      shutil.copystat(src, dest)
    else:
      shutil.copy2(src, dest)

    mode = path.mode
    if mode is None:
      mode = self.exe_mode if path.exe else self.default_mode
    os.chmod(dest, mode)

  def Copy(self, src_base, dest_base, path, strict=False, sloppy=False):
    """Copy artifact(s) from source directory to destination.

    Arguments:
      src_base: The directory to apply the src glob pattern match in.
      dest_base: The directory to copy matched files to.  |Path.dest|.
      path: A Path instance that specifies what is to be copied.
      strict: If set, enforce that all optional files are copied.
      sloppy: If set, ignore when mandatory artifacts are missing.
    Returns:
      A list of the artifacts copied.
    """
    assert not (strict and sloppy), 'strict and sloppy are not compatible.'
    copied_paths = []
    src = os.path.join(src_base, path.src)
    if not src.endswith('/') and os.path.isdir(src):
      raise MustNotBeDirError('%s must not be a directory\n'
                              'Aborting copy...' % (src,))
    paths = glob.glob(src)
    if not paths:
      if strict or (not path.optional and not sloppy):
        msg = ('%s does not exist and is required.\n'
               'You can bypass this error with --sloppy.\n'
               'Aborting copy...' % src)
        raise MissingPathError(msg)
      elif path.optional:
        logging.debug('%s does not exist and is optional.  Skipping.', src)
      else:
        logging.warn('%s does not exist and is required.  Skipping anyway.',
                     src)
    elif len(paths) > 1 and path.dest and not path.dest.endswith('/'):
      raise MultipleMatchError(
          'Glob pattern %r has multiple matches, but dest %s '
          'is not a directory.\n'
          'Aborting copy...' % (path.src, path.dest))
    else:
      for p in paths:
        rel_src = os.path.relpath(p, src_base)
        if path.dest is None:
          rel_dest = rel_src
        elif path.dest.endswith('/'):
          rel_dest = os.path.join(path.dest, os.path.basename(p))
        else:
          rel_dest = path.dest
        assert not rel_dest.endswith('/')
        dest = os.path.join(dest_base, rel_dest)

        copied_paths.append(p)
        self.Log(p, dest, os.path.isdir(p))
        if os.path.isdir(p):
          for sub_path in osutils.DirectoryIterator(p):
            rel_path = os.path.relpath(sub_path, p)
            sub_dest = os.path.join(dest, rel_path)
            if sub_path.endswith('/'):
              osutils.SafeMakedirs(sub_dest, mode=self.dir_mode)
            else:
              self._CopyFile(sub_path, sub_dest, path)
        else:
          self._CopyFile(p, dest, path)

    return copied_paths


class Path(object):
  """Represents an artifact to be copied from build dir to staging dir."""

  def __init__(self, src, exe=False, cond=None, dest=None, mode=None,
               optional=False, strip=True):
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
            to process this artifact. If supplied, the artifact will be treated
            as optional unless --strict is supplied.
      dest: Name to give to the target file/directory.  Defaults to keeping the
            same name as the source.
      mode: The mode to set for the matched files, and the contents of matched
            directories.
      optional: Whether to enforce the existence of the artifact.  If unset, the
                script errors out if the artifact does not exist.  In 'strict'
                mode, the Copier class ignores this flag.  In 'sloppy' mode, the
                Copier class treats all artifacts as optional.
      strip: If |exe| is set, whether to strip the executable.
    """
    self.src = src
    self.exe = exe
    self.cond = cond
    self.dest = dest
    self.mode = mode
    self.optional = optional or cond
    self.strip = strip

  def ShouldProcess(self, gyp_defines, staging_flags):
    """Tests whether this artifact should be copied."""
    if self.cond:
      return self.cond(gyp_defines, staging_flags)
    return True


_DISABLE_NACL = 'disable_nacl'
_USE_DRM = 'use_drm'


_CHROME_INTERNAL_FLAG = 'chrome_internal'
_CONTENT_SHELL_FLAG = 'content_shell'
_HIGHDPI_FLAG = 'highdpi'
_PDF_FLAG = 'chrome_pdf'
STAGING_FLAGS = (_CHROME_INTERNAL_FLAG, _CONTENT_SHELL_FLAG, _HIGHDPI_FLAG,
                 _PDF_FLAG)

_CHROME_SANDBOX_DEST = 'chrome-sandbox'
C = Conditions


_COPY_PATHS = (
  Path('ash_shell',
       cond=C.GypSet(_USE_DRM)),
  Path('aura_demo',
       cond=C.GypSet(_USE_DRM)),
  Path('chrome',
       exe=True),
  Path('chrome_sandbox', mode=04755,
       dest=_CHROME_SANDBOX_DEST),
  Path('chrome-wrapper'),
  Path('chrome.pak'),
  Path('chrome_100_percent.pak'),
  Path('chrome_200_percent.pak',
       cond=C.StagingFlagSet(_HIGHDPI_FLAG)),
  Path('content_shell',
       exe=True,
       cond=C.StagingFlagSet(_CONTENT_SHELL_FLAG)),
  Path('content_shell.pak',
       cond=C.StagingFlagSet(_CONTENT_SHELL_FLAG)),
  Path('extensions/',
       cond=C.StagingFlagSet(_CHROME_INTERNAL_FLAG)),
  Path('lib/*.so',
       exe=True,
       cond=C.GypSet('component', value='shared_library')),
  Path('libffmpegsumo.so',
       exe=True),
  Path('libpdf.so',
       exe=True,
       cond=C.StagingFlagSet(_PDF_FLAG)),
  Path('libppGoogleNaClPluginChrome.so',
       exe=True,
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('libosmesa.so',
       exe=True,
       optional=True),
  # Widevine binaries are already pre-stripped.  In addition, they don't
  # play well with the binutils stripping tools, so skip stripping.
  Path('libwidevinecdmadapter.so',
       exe=True,
       strip=False,
       cond=C.StagingFlagSet(_CHROME_INTERNAL_FLAG)),
  Path('libwidevinecdm.so',
       exe=True,
       strip=False,
       cond=C.StagingFlagSet(_CHROME_INTERNAL_FLAG)),
  Path('locales/'),
  # Do not strip the nacl_helper_bootstrap binary because the binutils
  # objcopy/strip mangles the ELF program headers.
  Path('nacl_helper_bootstrap',
       exe=True, strip=False,
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('nacl_irt_*.nexe',
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('nacl_helper',
       optional=True,
       cond=C.GypNotSet(_DISABLE_NACL)),
  Path('resources/'),
  Path('resources.pak'),
  Path('xdg-settings'),
  Path('*.png'),
)


def _FixPermissions(dest_base):
  """Last minute permission fixes."""
  cros_build_lib.DebugRunCommand(['chmod', '-R', 'a+r', dest_base])
  cros_build_lib.DebugRunCommand(
      ['find', dest_base, '-perm', '/110', '-exec', 'chmod', 'a+x', '{}', '+'])


def StageChromeFromBuildDir(staging_dir, build_dir, strip_bin, strict=False,
                            sloppy=False, gyp_defines=None, staging_flags=None,
                            strip_flags=None):
  """Populates a staging directory with necessary build artifacts.

  If |strict| is set, then we decide what to stage based on the |gyp_defines|
  and |staging_flags| passed in.  Otherwise, we stage everything that we know
  about, that we can find.

  Arguments:
    staging_dir: Path to an empty staging directory.
    build_dir: Path to location of Chrome build artifacts.
    strip_bin: Path to executable used for stripping binaries.
    strict: If set, decide what to stage based on the |gyp_defines| and
            |staging_flags| passed in, and enforce that all optional files
            are copied.  Otherwise, we stage optional files if they are
            there, but we don't complain if they're not.
    sloppy: Ignore when mandatory artifacts are missing.
    gyp_defines: A dictionary (i.e., one returned by ProcessGypDefines)
      containing GYP_DEFINES Chrome was built with.
    staging_flags: A list of extra staging flags.  Valid flags are specified in
      STAGING_FLAGS.
    strip_flags: A list of flags to pass to the tool used to strip binaries.
  """
  os.mkdir(os.path.join(staging_dir, 'plugins'), 0755)

  if gyp_defines is None:
    gyp_defines = {}
  if staging_flags is None:
    staging_flags = []

  copier = Copier(strip_bin=strip_bin, strip_flags=strip_flags)
  copied_paths = []
  for p in _COPY_PATHS:
    if not strict or p.ShouldProcess(gyp_defines, staging_flags):
      copied_paths += copier.Copy(build_dir, staging_dir, p, strict=strict,
                                  sloppy=sloppy)

  if not copied_paths:
    raise MissingPathError('Couldn\'t find anything to copy!\n'
                           'Are you looking in the right directory?\n'
                           'Aborting copy...')

  _FixPermissions(staging_dir)
