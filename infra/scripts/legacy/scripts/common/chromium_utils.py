# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Set of basic operations/utilities that are used by the build. """

from contextlib import contextmanager
import ast
import base64
import cStringIO
import copy
import errno
import fnmatch
import glob
import math
import multiprocessing
import os
import re
import shutil
import socket
import stat
import string  # pylint: disable=W0402
import subprocess
import sys
import threading
import time
import traceback
import urllib
import zipfile
import zlib

try:
  import json  # pylint: disable=F0401
except ImportError:
  import simplejson as json

from common import env


BUILD_DIR = os.path.realpath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))


WIN_LINK_FUNC = None
try:
  if sys.platform.startswith('win'):
    import ctypes
    # There's 4 possibilities on Windows for links:
    # 1. Symbolic file links;
    # 2. Symbolic directory links;
    # 3. Hardlinked files;
    # 4. Junctioned directories.
    # (Hardlinked directories don't really exist.)
    #
    # 7-Zip does not handle symbolic file links as we want (it puts the
    # content of the link, not what it refers to, and reports "CRC Error" on
    # extraction). It does work as expected for symbolic directory links.
    # Because the majority of the large files are in the root of the staging
    # directory, we do however need to handle file links, so we do this with
    # hardlinking. Junctioning requires a huge whack of code, so we take the
    # slightly odd tactic of using #2 and #3, but not #1 and #4. That is,
    # hardlinks for files, but symbolic links for directories.
    def _WIN_LINK_FUNC(src, dst):
      print 'linking %s -> %s' % (src, dst)
      if os.path.isdir(src):
        if not ctypes.windll.kernel32.CreateSymbolicLinkA(
            str(dst), str(os.path.abspath(src)), 1):
          raise ctypes.WinError()
      else:
        if not ctypes.windll.kernel32.CreateHardLinkA(str(dst), str(src), 0):
          raise ctypes.WinError()
    WIN_LINK_FUNC = _WIN_LINK_FUNC
except ImportError:
  # If we don't have ctypes or aren't on Windows, leave WIN_LINK_FUNC as None.
  pass


# Wrapper around git that enforces a timeout.
GIT_BIN = os.path.join(BUILD_DIR, 'scripts', 'tools', 'git-with-timeout')

# Wrapper around svn that enforces a timeout.
SVN_BIN = os.path.join(BUILD_DIR, 'scripts', 'tools', 'svn-with-timeout')

# The Google Storage metadata key for the full commit position
GS_COMMIT_POSITION_KEY = 'Cr-Commit-Position'
# The Google Storage metadata key for the commit position number
GS_COMMIT_POSITION_NUMBER_KEY = 'Cr-Commit-Position-Number'
# The Google Storage metadata key for the Git commit hash
GS_GIT_COMMIT_KEY = 'Cr-Git-Commit'

# Regular expression to identify a Git hash
GIT_COMMIT_HASH_RE = re.compile(r'[a-zA-Z0-9]{40}')
#
# Regular expression to parse a commit position
COMMIT_POSITION_RE = re.compile(r'([^@]+)@{#(\d+)}')

# Local errors.
class MissingArgument(Exception):
  pass
class PathNotFound(Exception):
  pass
class ExternalError(Exception):
  pass
class NoIdentifiedRevision(Exception):
  pass

def IsWindows():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')

def IsLinux():
  return sys.platform.startswith('linux')

def IsMac():
  return sys.platform.startswith('darwin')

# For chromeos we need to end up with a different platform name, but the
# scripts use the values like sys.platform for both the build target and
# and the running OS, so this gives us a back door that can be hit to
# force different naming then the default for some of the chromeos build
# steps.
override_platform_name = None


def OverridePlatformName(name):
  """Sets the override for PlatformName()"""
  global override_platform_name
  override_platform_name = name


def PlatformName():
  """Return a string to be used in paths for the platform."""
  if override_platform_name:
    return override_platform_name
  if IsWindows():
    return 'win32'
  if IsLinux():
    return 'linux'
  if IsMac():
    return 'mac'
  raise NotImplementedError('Unknown platform "%s".' % sys.platform)


# Name of the file (inside the packaged build) containing revision number
# of that build. Also used for determining the latest packaged build.
FULL_BUILD_REVISION_FILENAME = 'FULL_BUILD_REVISION'

def IsGitCommit(value):
  """Returns: If a value is a Git commit hash.

  This only works on full Git commit hashes. A value qualifies as a Git commit
  hash if it only contains hexadecimal numbers and is forty characters long.
  """
  if value is None:
    return False
  return GIT_COMMIT_HASH_RE.match(str(value)) is not None


# GetParentClass allows a class instance to find its parent class using Python's
# inspect module.  This allows a class instantiated from a module to access
# their parent class's methods even after the containing module has been
# re-imported and reloaded.
#
# Also see:
#   http://code.google.com/p/chromium/issues/detail?id=34089
#   http://atlee.ca/blog/2008/11/21/python-reload-danger-here-be-dragons/
#
def GetParentClass(obj, n=1):
  import inspect
  if inspect.isclass(obj):
    return inspect.getmro(obj)[n]
  else:
    return inspect.getmro(obj.__class__)[n]


def MeanAndStandardDeviation(data):
  """Calculates mean and standard deviation for the values in the list.

    Args:
      data: list of numbers

    Returns:
      Mean and standard deviation for the numbers in the list.
  """
  n = len(data)
  if n == 0:
    return 0.0, 0.0
  mean = float(sum(data)) / n
  variance = sum([(element - mean)**2 for element in data]) / n
  return mean, math.sqrt(variance)


def FilteredMeanAndStandardDeviation(data):
  """Calculates mean and standard deviation for the values in the list
  ignoring first occurence of max value (unless there was only one sample).

    Args:
      data: list of numbers

    Returns:
      Mean and standard deviation for the numbers in the list ignoring
      first occurence of max value.
  """

  def _FilterMax(array):
    new_array = copy.copy(array)  # making sure we are not creating side-effects
    if len(new_array) != 1:
      new_array.remove(max(new_array))
    return new_array
  return MeanAndStandardDeviation(_FilterMax(data))

def HistogramPercentiles(histogram, percentiles):
  if not 'buckets' in histogram or not 'count' in histogram:
    return []
  computed_percentiles = _ComputePercentiles(histogram['buckets'],
                                             histogram['count'],
                                             percentiles)
  output = []
  for p in computed_percentiles:
    output.append({'percentile': p, 'value': computed_percentiles[p]})
  return output

def GeomMeanAndStdDevFromHistogram(histogram):
  if not 'buckets' in histogram:
    return 0.0, 0.0
  count = 0
  sum_of_logs = 0
  for bucket in histogram['buckets']:
    if 'high' in bucket:
      bucket['mean'] = (bucket['low'] + bucket['high']) / 2.0
    else:
      bucket['mean'] = bucket['low']
    if bucket['mean'] > 0:
      sum_of_logs += math.log(bucket['mean']) * bucket['count']
      count += bucket['count']

  if count == 0:
    return 0.0, 0.0

  sum_of_squares = 0
  geom_mean = math.exp(sum_of_logs / count)
  for bucket in histogram['buckets']:
    if bucket['mean'] > 0:
      sum_of_squares += (bucket['mean'] - geom_mean) ** 2 * bucket['count']
  return geom_mean, math.sqrt(sum_of_squares / count)

def _LinearInterpolate(x0, target, x1, y0, y1):
  """Perform linear interpolation to estimate an intermediate value.

  We assume for some F, F(x0) == y0, and F(x1) == z1.

  We return an estimate for what F(target) should be, using linear
  interpolation.

  Args:
    x0: (Float) A location at which some function F() is known.
    target: (Float) A location at which we need to estimate F().
    x1: (Float) A second location at which F() is known.
    y0: (Float) The value of F(x0).
    y1: (Float) The value of F(x1).

  Returns:
    (Float) The estimated value of F(target).
  """
  if x0 == x1:
    return (y0 + y1) / 2
  return  (y1 - y0) * (target - x0) / (x1 - x0) + y0

def _BucketInterpolate(last_percentage, target, next_percentage, bucket_min,
                       bucket_max):
  """Estimate a minimum which should have the target % of samples below it.

  We do linear interpolation only if last_percentage and next_percentage are
  adjacent, and hence we are in a linear section of a histogram. Once they
  spread further apart we generally get exponentially broader buckets, and we
  need to interpolate in the log domain (and exponentiate our result).

  Args:
    last_percentage: (Float) This is the percentage of samples below bucket_min.
    target: (Float) A percentage for which we need an estimated bucket.
    next_percentage: (Float) This is the percentage of samples below bucket_max.
    bucket_min: (Float) This is the lower value for samples in a bucket.
    bucket_max: (Float) This exceeds the upper value for samples.

  Returns:
    (Float) An estimate of what bucket cutoff would have probably had the target
        percentage.
  """
  log_domain = False
  if bucket_min + 1.5 < bucket_max and bucket_min > 0:
    log_domain = True
    bucket_min = math.log(bucket_min)
    bucket_max = math.log(bucket_max)
  result = _LinearInterpolate(
      last_percentage, target, next_percentage, bucket_min, bucket_max)
  if log_domain:
    result = math.exp(result)
  return result

def _ComputePercentiles(buckets, total, percentiles):
  """Compute percentiles for the given histogram.

  Returns estimates for the bucket cutoffs that would probably have the taret
  percentiles.

  Args:
    buckets: (List) A list of buckets representing the histogram to analyze.
    total: (Float) The total number of samples in the histogram.
    percentiles: (Tuple) The percentiles we are interested in.

  Returns:
    (Dictionary) Map from percentiles to bucket cutoffs.
  """
  if not percentiles:
    return {}
  current_count = 0
  current_percentage = 0
  next_percentile_index = 0
  result = {}
  for bucket in buckets:
    if bucket['count'] > 0:
      current_count += bucket['count']
      old_percentage = current_percentage
      current_percentage = float(current_count) / total

      # Check whether we passed one of the percentiles we're interested in.
      while (next_percentile_index < len(percentiles) and
             current_percentage > percentiles[next_percentile_index]):
        if not 'high' in bucket:
          result[percentiles[next_percentile_index]] = bucket['low']
        else:
          result[percentiles[next_percentile_index]] = float(_BucketInterpolate(
              old_percentage, percentiles[next_percentile_index],
              current_percentage, bucket['low'], bucket['high']))
        next_percentile_index += 1
  return result

class InitializePartiallyWithArguments:
  # pylint: disable=old-style-class
  """Function currying implementation.

  Works for constructors too. Primary use is to be able to construct a class
  with some constructor arguments beings set ahead of actual initialization.
  Copy of an ASPN cookbook (#52549).
  """

  def __init__(self, clazz, *args, **kwargs):
    self.clazz = clazz
    self.pending = args[:]
    self.kwargs = kwargs.copy()

  def __call__(self, *args, **kwargs):
    if kwargs and self.kwargs:
      kw = self.kwargs.copy()
      kw.update(kwargs)
    else:
      kw = kwargs or self.kwargs

    return self.clazz(*(self.pending + args), **kw)


def Prepend(filepath, text):
  """ Prepends text to the file.

  Creates the file if it does not exist.
  """
  file_data = text
  if os.path.exists(filepath):
    file_data += open(filepath).read()
  f = open(filepath, 'w')
  f.write(file_data)
  f.close()


def MakeWorldReadable(path):
  """Change the permissions of the given path to make it world-readable.
  This is often needed for archived files, so they can be served by web servers
  or accessed by unprivileged network users."""

  # No need to do anything special on Windows.
  if IsWindows():
    return

  perms = stat.S_IMODE(os.stat(path)[stat.ST_MODE])
  if os.path.isdir(path):
    # Directories need read and exec.
    os.chmod(path, perms | 0555)
  else:
    os.chmod(path, perms | 0444)


def MakeParentDirectoriesWorldReadable(path):
  """Changes the permissions of the given path and its parent directories
  to make them world-readable. Stops on first directory which is
  world-readable. This is often needed for archive staging directories,
  so that they can be served by web servers or accessed by unprivileged
  network users."""

  # No need to do anything special on Windows.
  if IsWindows():
    return

  while path != os.path.dirname(path):
    current_permissions = stat.S_IMODE(os.stat(path)[stat.ST_MODE])
    if current_permissions & 0555 == 0555:
      break
    os.chmod(path, current_permissions | 0555)
    path = os.path.dirname(path)


def MaybeMakeDirectory(*path):
  """Creates an entire path, if it doesn't already exist."""
  file_path = os.path.join(*path)
  try:
    os.makedirs(file_path)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise


def RemovePath(*path):
  """Removes the file or directory at 'path', if it exists."""
  file_path = os.path.join(*path)
  if os.path.exists(file_path):
    if os.path.isdir(file_path):
      RemoveDirectory(file_path)
    else:
      RemoveFile(file_path)


def RemoveFile(*path):
  """Removes the file located at 'path', if it exists."""
  file_path = os.path.join(*path)
  try:
    os.remove(file_path)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise


def MoveFile(path, new_path):
  """Moves the file located at 'path' to 'new_path', if it exists."""
  try:
    RemoveFile(new_path)
    os.rename(path, new_path)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise


def LocateFiles(pattern, root=os.curdir):
  """Yeilds files matching pattern found in root and its subdirectories.

  An exception is thrown if root doesn't exist."""
  for path, _, files in os.walk(os.path.abspath(root)):
    for filename in fnmatch.filter(files, pattern):
      yield os.path.join(path, filename)


def RemoveFilesWildcards(file_wildcard, root=os.curdir):
  """Removes files matching 'file_wildcard' in root and its subdirectories, if
  any exists.

  An exception is thrown if root doesn't exist."""
  for item in LocateFiles(file_wildcard, root):
    try:
      os.remove(item)
    except OSError, e:
      if e.errno != errno.ENOENT:
        raise


def RemoveGlobbedPaths(path_wildcard, root=os.curdir):
  """Removes all paths matching 'path_wildcard' beneath root.

  Returns the list of paths removed.

  An exception is thrown if root doesn't exist."""
  if not os.path.exists(root):
    raise OSError(2, 'No such file or directory', root)

  full_path_wildcard = os.path.join(path_wildcard, root)
  paths = glob.glob(full_path_wildcard)
  for path in paths:
    # When glob returns directories they end in "/."
    if path.endswith(os.sep + '.'):
      path = path[:-2]
    RemovePath(path)
  return paths


def RemoveDirectory(*path):
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  if sys.platform == 'win32':
    # Give up and use cmd.exe's rd command.
    file_path = os.path.normcase(file_path)
    for _ in xrange(3):
      print 'RemoveDirectory running %s' % (' '.join(
          ['cmd.exe', '/c', 'rd', '/q', '/s', file_path]))
      if not subprocess.call(['cmd.exe', '/c', 'rd', '/q', '/s', file_path]):
        break
      print '  Failed'
      time.sleep(3)
    return

  def RemoveWithRetry_non_win(rmfunc, path):
    if os.path.islink(path):
      return os.remove(path)
    else:
      return rmfunc(path)

  remove_with_retry = RemoveWithRetry_non_win

  def RmTreeOnError(function, path, excinfo):
    r"""This works around a problem whereby python 2.x on Windows has no ability
    to check for symbolic links.  os.path.islink always returns False.  But
    shutil.rmtree will fail if invoked on a symbolic link whose target was
    deleted before the link.  E.g., reproduce like this:
    > mkdir test
    > mkdir test\1
    > mklink /D test\current test\1
    > python -c "import chromium_utils; chromium_utils.RemoveDirectory('test')"
    To avoid this issue, we pass this error-handling function to rmtree.  If
    we see the exact sort of failure, we ignore it.  All other failures we re-
    raise.
    """

    exception_type = excinfo[0]
    exception_value = excinfo[1]
    # If shutil.rmtree encounters a symbolic link on Windows, os.listdir will
    # fail with a WindowsError exception with an ENOENT errno (i.e., file not
    # found).  We'll ignore that error.  Note that WindowsError is not defined
    # for non-Windows platforms, so we use OSError (of which it is a subclass)
    # to avoid lint complaints about an undefined global on non-Windows
    # platforms.
    if (function is os.listdir) and issubclass(exception_type, OSError):
      if exception_value.errno == errno.ENOENT:
        # File does not exist, and we're trying to delete, so we can ignore the
        # failure.
        print 'WARNING:  Failed to list %s during rmtree.  Ignoring.\n' % path
      else:
        raise
    else:
      raise

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(lambda p: shutil.rmtree(p, onerror=RmTreeOnError),
                        os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)


def CopyFileToDir(src_path, dest_dir, dest_fn=None, link_ok=False):
  """Copies the file found at src_path to the dest_dir directory, with metadata.

  If dest_fn is specified, the src_path is copied to that name in dest_dir,
  otherwise it is copied to a file of the same name.

  Raises PathNotFound if either the file or the directory is not found.
  """
  # Verify the file and directory separately so we can tell them apart and
  # raise PathNotFound rather than shutil.copyfile's IOError.
  if not os.path.isfile(src_path):
    raise PathNotFound('Unable to find file %s' % src_path)
  if not os.path.isdir(dest_dir):
    raise PathNotFound('Unable to find dir %s' % dest_dir)
  src_file = os.path.basename(src_path)
  if dest_fn:
    # If we have ctypes and the caller doesn't mind links, use that to
    # try to make the copy faster on Windows. http://crbug.com/418702.
    if link_ok and WIN_LINK_FUNC:
      WIN_LINK_FUNC(src_path, os.path.join(dest_dir, dest_fn))
    else:
      shutil.copy2(src_path, os.path.join(dest_dir, dest_fn))
  else:
    shutil.copy2(src_path, os.path.join(dest_dir, src_file))


def MakeZip(output_dir, archive_name, file_list, file_relative_dir,
            raise_error=True, remove_archive_directory=True):
  """Packs files into a new zip archive.

  Files are first copied into a directory within the output_dir named for
  the archive_name, which will be created if necessary and emptied if it
  already exists.  The files are then then packed using archive names
  relative to the output_dir.  That is, if the zipfile is unpacked in place,
  it will create a directory identical to the new archive_name directory, in
  the output_dir.  The zip file will be named as the archive_name, plus
  '.zip'.

  Args:
    output_dir: Absolute path to the directory in which the archive is to
      be created.
    archive_dir: Subdirectory of output_dir holding files to be added to
      the new zipfile.
    file_list: List of paths to files or subdirectories, relative to the
      file_relative_dir.
    file_relative_dir: Absolute path to the directory containing the files
      and subdirectories in the file_list.
    raise_error: Whether to raise a PathNotFound error if one of the files in
      the list is not found.
    remove_archive_directory: Whether to remove the archive staging directory
      before copying files over to it.

  Returns:
    A tuple consisting of (archive_dir, zip_file_path), where archive_dir
    is the full path to the newly created archive_name subdirectory.

  Raises:
    PathNotFound if any of the files in the list is not found, unless
    raise_error is False, in which case the error will be ignored.
  """

  start_time = time.clock()
  # Collect files into the archive directory.
  archive_dir = os.path.join(output_dir, archive_name)
  print 'output_dir: %s, archive_name: %s' % (output_dir, archive_name)
  print 'archive_dir: %s, remove_archive_directory: %s, exists: %s' % (
      archive_dir, remove_archive_directory, os.path.exists(archive_dir))
  if remove_archive_directory and os.path.exists(archive_dir):
    # Move it even if it's not a directory as expected. This can happen with
    # FILES.cfg archive creation where we create an archive staging directory
    # that is the same name as the ultimate archive name.
    if not os.path.isdir(archive_dir):
      print 'Moving old "%s" file to create same name directory.' % archive_dir
      previous_archive_file = '%s.old' % archive_dir
      MoveFile(archive_dir, previous_archive_file)
    else:
      print 'Removing %s' % archive_dir
      RemoveDirectory(archive_dir)
      print 'Now, os.path.exists(%s): %s' % (
          archive_dir, os.path.exists(archive_dir))
  MaybeMakeDirectory(archive_dir)
  for needed_file in file_list:
    needed_file = needed_file.rstrip()
    # These paths are relative to the file_relative_dir.  We need to copy
    # them over maintaining the relative directories, where applicable.
    src_path = os.path.join(file_relative_dir, needed_file)
    dirname, basename = os.path.split(needed_file)
    try:
      if os.path.isdir(src_path):
        if WIN_LINK_FUNC:
          WIN_LINK_FUNC(src_path, os.path.join(archive_dir, needed_file))
        else:
          shutil.copytree(src_path, os.path.join(archive_dir, needed_file),
                          symlinks=True)
      elif dirname != '' and basename != '':
        dest_dir = os.path.join(archive_dir, dirname)
        MaybeMakeDirectory(dest_dir)
        CopyFileToDir(src_path, dest_dir, basename, link_ok=True)
      else:
        CopyFileToDir(src_path, archive_dir, basename, link_ok=True)
    except PathNotFound:
      if raise_error:
        raise
  end_time = time.clock()
  print 'Took %f seconds to create archive directory.' % (end_time - start_time)

  # Pack the zip file.
  output_file = '%s.zip' % archive_dir
  previous_file = '%s_old.zip' % archive_dir
  MoveFile(output_file, previous_file)

  # If we have 7z, use that as it's much faster. See http://crbug.com/418702.
  windows_zip_cmd = None
  if os.path.exists('C:\\Program Files\\7-Zip\\7z.exe'):
    windows_zip_cmd = ['C:\\Program Files\\7-Zip\\7z.exe', 'a', '-y', '-mx1']

  # On Windows we use the python zip module; on Linux and Mac, we use the zip
  # command as it will handle links and file bits (executable).  Which is much
  # easier then trying to do that with ZipInfo options.
  start_time = time.clock()
  if IsWindows() and not windows_zip_cmd:
    print 'Creating %s' % output_file

    def _Addfiles(to_zip_file, dirname, files_to_add):
      for this_file in files_to_add:
        archive_name = this_file
        this_path = os.path.join(dirname, this_file)
        if os.path.isfile(this_path):
          # Store files named relative to the outer output_dir.
          archive_name = this_path.replace(output_dir + os.sep, '')
          if os.path.getsize(this_path) == 0:
            compress_method = zipfile.ZIP_STORED
          else:
            compress_method = zipfile.ZIP_DEFLATED
          to_zip_file.write(this_path, archive_name, compress_method)
          print 'Adding %s' % archive_name
    zip_file = zipfile.ZipFile(output_file, 'w', zipfile.ZIP_DEFLATED,
                               allowZip64=True)
    try:
      os.path.walk(archive_dir, _Addfiles, zip_file)
    finally:
      zip_file.close()
  else:
    if IsMac() or IsLinux():
      zip_cmd = ['zip', '-yr1']
    else:
      zip_cmd = windows_zip_cmd
    saved_dir = os.getcwd()
    os.chdir(os.path.dirname(archive_dir))
    command = zip_cmd + [output_file, os.path.basename(archive_dir)]
    result = RunCommand(command)
    os.chdir(saved_dir)
    if result and raise_error:
      raise ExternalError('zip failed: %s => %s' %
                          (str(command), result))
  end_time = time.clock()
  print 'Took %f seconds to create zip.' % (end_time - start_time)
  return (archive_dir, output_file)


def ExtractZip(filename, output_dir, verbose=True):
  """ Extract the zip archive in the output directory.
  """
  MaybeMakeDirectory(output_dir)

  # On Linux and Mac, we use the unzip command as it will
  # handle links and file bits (executable), which is much
  # easier then trying to do that with ZipInfo options.
  #
  # The Mac Version of unzip unfortunately does not support Zip64, whereas
  # the python module does, so we have to fallback to the python zip module
  # on Mac if the filesize is greater than 4GB.
  #
  # On Windows, try to use 7z if it is installed, otherwise fall back to python
  # zip module and pray we don't have files larger than 512MB to unzip.
  unzip_cmd = None
  if ((IsMac() and os.path.getsize(filename) < 4 * 1024 * 1024 * 1024)
      or IsLinux()):
    unzip_cmd = ['unzip', '-o']
  elif IsWindows() and os.path.exists('C:\\Program Files\\7-Zip\\7z.exe'):
    unzip_cmd = ['C:\\Program Files\\7-Zip\\7z.exe', 'x', '-y']

  if unzip_cmd:
    # Make sure path is absolute before changing directories.
    filepath = os.path.abspath(filename)
    saved_dir = os.getcwd()
    os.chdir(output_dir)
    command = unzip_cmd + [filepath]
    result = RunCommand(command)
    os.chdir(saved_dir)
    if result:
      raise ExternalError('unzip failed: %s => %s' % (str(command), result))
  else:
    assert IsWindows() or IsMac()
    zf = zipfile.ZipFile(filename)
    # TODO(hinoka): This can be multiprocessed.
    for name in zf.namelist():
      if verbose:
        print 'Extracting %s' % name
      zf.extract(name, output_dir)
      if IsMac():
        # Restore permission bits.
        os.chmod(os.path.join(output_dir, name),
                 zf.getinfo(name).external_attr >> 16L)


def WindowsPath(path):
  """Returns a Windows mixed-style absolute path, given a Cygwin absolute path.

  The version of Python in the Chromium tree uses posixpath for os.path even
  on Windows, so we convert to a mixed Windows path (that is, a Windows path
  that uses forward slashes instead of backslashes) manually.
  """
  # TODO(pamg): make this work for other drives too.
  if path.startswith('/cygdrive/c/'):
    return path.replace('/cygdrive/c/', 'C:/')
  return path


def FindUpwardParent(start_dir, *desired_list):
  """Finds the desired object's parent, searching upward from the start_dir.

  Searches within start_dir and within all its parents looking for the desired
  directory or file, which may be given in one or more path components. Returns
  the first directory in which the top desired path component was found, or
  raises PathNotFound if it wasn't.
  """
  desired_path = os.path.join(*desired_list)
  last_dir = ''
  cur_dir = start_dir
  found_path = os.path.join(cur_dir, desired_path)
  while not os.path.exists(found_path):
    last_dir = cur_dir
    cur_dir = os.path.dirname(cur_dir)
    if last_dir == cur_dir:
      raise PathNotFound('Unable to find %s above %s' %
                         (desired_path, start_dir))
    found_path = os.path.join(cur_dir, desired_path)
  # Strip the entire original desired path from the end of the one found
  # and remove a trailing path separator, if present.
  found_path = found_path[:len(found_path) - len(desired_path)]
  if found_path.endswith(os.sep):
    found_path = found_path[:len(found_path) - 1]
  return found_path


def FindUpward(start_dir, *desired_list):
  """Returns a path to the desired directory or file, searching upward.

  Searches within start_dir and within all its parents looking for the desired
  directory or file, which may be given in one or more path components. Returns
  the full path to the desired object, or raises PathNotFound if it wasn't
  found.
  """
  parent = FindUpwardParent(start_dir, *desired_list)
  return os.path.join(parent, *desired_list)


def RunAndPrintDots(function):
  """Starts a background thread that prints dots while the function runs."""

  def Hook(*args, **kwargs):
    event = threading.Event()

    def PrintDots():
      counter = 0
      while not event.isSet():
        event.wait(5)
        sys.stdout.write('.')
        counter = (counter + 1) % 80
        if not counter:
          sys.stdout.write('\n')
        sys.stdout.flush()
    t = threading.Thread(target=PrintDots)
    t.start()
    try:
      return function(*args, **kwargs)
    finally:
      event.set()
      t.join()
  return Hook


class RunCommandFilter(object):
  """Class that should be subclassed to provide a filter for RunCommand."""
  # Method could be a function
  # pylint: disable=R0201

  def FilterLine(self, a_line):
    """Called for each line of input.  The \n is included on a_line.  Should
    return what is to be recorded as the output for this line.  A result of
    None suppresses the line."""
    return a_line

  def FilterDone(self, last_bits):
    """Acts just like FilterLine, but is called with any data collected after
    the last newline of the command."""
    return last_bits


class FilterCapture(RunCommandFilter):
  """Captures the text and places it into an array."""
  def __init__(self):
    RunCommandFilter.__init__(self)
    self.text = []

  def FilterLine(self, line):
    self.text.append(line.rstrip())

  def FilterDone(self, text):
    self.text.append(text)


def RunCommand(command, parser_func=None, filter_obj=None, pipes=None,
               print_cmd=True, timeout=None, max_time=None, **kwargs):
  """Runs the command list, printing its output and returning its exit status.

  Prints the given command (which should be a list of one or more strings),
  then runs it and writes its stdout and stderr to the appropriate file handles.

  If timeout is set, the process will be killed if output is stopped after
  timeout seconds. If max_time is set, the process will be killed if it runs for
  more than max_time.

  If parser_func is not given, the subprocess's output is passed to stdout
  and stderr directly.  If the func is given, each line of the subprocess's
  stdout/stderr is passed to the func and then written to stdout.

  If filter_obj is given, all output is run through the filter a line
  at a time before it is written to stdout.

  We do not currently support parsing stdout and stderr independent of
  each other.  In previous attempts, this led to output ordering issues.
  By merging them when either needs to be parsed, we avoid those ordering
  issues completely.

  pipes is a list of commands (also a list) that will receive the output of
  the intial command. For example, if you want to run "python a | python b | c",
  the "command" will be set to ['python', 'a'], while pipes will be set to
  [['python', 'b'],['c']]
  """

  def TimedFlush(timeout, fh, kill_event):
    """Flush fh every timeout seconds until kill_event is true."""
    while True:
      try:
        fh.flush()
      # File handle is closed, exit.
      except ValueError:
        break
      # Wait for kill signal or timeout.
      if kill_event.wait(timeout):
        break

  # TODO(all): nsylvain's CommandRunner in buildbot_slave is based on this
  # method.  Update it when changes are introduced here.
  def ProcessRead(readfh, writefh, parser_func=None, filter_obj=None,
                  log_event=None):
    writefh.flush()

    # Python on Windows writes the buffer only when it reaches 4k.  Ideally
    # we would flush a minimum of 10 seconds.  However, we only write and
    # flush no more often than 20 seconds to avoid flooding the master with
    # network traffic from unbuffered output.
    kill_event = threading.Event()
    flush_thread = threading.Thread(
        target=TimedFlush, args=(20, writefh, kill_event))
    flush_thread.daemon = True
    flush_thread.start()

    try:
      in_byte = readfh.read(1)
      in_line = cStringIO.StringIO()
      while in_byte:
        # Capture all characters except \r.
        if in_byte != '\r':
          in_line.write(in_byte)

        # Write and flush on newline.
        if in_byte == '\n':
          if log_event:
            log_event.set()
          if parser_func:
            parser_func(in_line.getvalue().strip())

          if filter_obj:
            filtered_line = filter_obj.FilterLine(in_line.getvalue())
            if filtered_line is not None:
              writefh.write(filtered_line)
          else:
            writefh.write(in_line.getvalue())
          in_line = cStringIO.StringIO()
        in_byte = readfh.read(1)

      if log_event and in_line.getvalue():
        log_event.set()

      # Write remaining data and flush on EOF.
      if parser_func:
        parser_func(in_line.getvalue().strip())

      if filter_obj:
        if in_line.getvalue():
          filtered_line = filter_obj.FilterDone(in_line.getvalue())
          if filtered_line is not None:
            writefh.write(filtered_line)
      else:
        if in_line.getvalue():
          writefh.write(in_line.getvalue())
    finally:
      kill_event.set()
      flush_thread.join()
      writefh.flush()

  pipes = pipes or []

  # Print the given command (which should be a list of one or more strings).
  if print_cmd:
    print '\n' + subprocess.list2cmdline(command) + '\n',
    for pipe in pipes:
      print '     | ' + subprocess.list2cmdline(pipe) + '\n',

  sys.stdout.flush()
  sys.stderr.flush()

  if not (parser_func or filter_obj or pipes or timeout or max_time):
    # Run the command.  The stdout and stderr file handles are passed to the
    # subprocess directly for writing.  No processing happens on the output of
    # the subprocess.
    proc = subprocess.Popen(command, stdout=sys.stdout, stderr=sys.stderr,
                            bufsize=0, **kwargs)

  else:
    if not (parser_func or filter_obj):
      filter_obj = RunCommandFilter()

    # Start the initial process.
    proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, bufsize=0, **kwargs)
    proc_handles = [proc]

    if pipes:
      pipe_number = 0
      for pipe in pipes:
        pipe_number = pipe_number + 1
        if pipe_number == len(pipes) and not (parser_func or filter_obj):
          # The last pipe process needs to output to sys.stdout or filter
          stdout = sys.stdout
        else:
          # Output to a pipe, since another pipe is on top of us.
          stdout = subprocess.PIPE
        pipe_proc = subprocess.Popen(pipe, stdin=proc_handles[0].stdout,
                                     stdout=stdout, stderr=subprocess.STDOUT)
        proc_handles.insert(0, pipe_proc)

      # Allow proc to receive a SIGPIPE if the piped process exits.
      for handle in proc_handles[1:]:
        handle.stdout.close()

    log_event = threading.Event()

    # Launch and start the reader thread.
    thread = threading.Thread(target=ProcessRead,
                              args=(proc_handles[0].stdout, sys.stdout),
                              kwargs={'parser_func': parser_func,
                                      'filter_obj': filter_obj,
                                      'log_event': log_event})

    kill_lock = threading.Lock()


    def term_then_kill(handle, initial_timeout, numtimeouts, interval):
      def timed_check():
        for _ in range(numtimeouts):
          if handle.poll() is not None:
            return True
          time.sleep(interval)

      handle.terminate()
      time.sleep(initial_timeout)
      timed_check()
      if handle.poll() is None:
        handle.kill()
      timed_check()
      return handle.poll() is not None


    def kill_proc(proc_handles, message=None):
      with kill_lock:
        if proc_handles:
          killed = term_then_kill(proc_handles[0], 0.1, 5, 1)

          if message:
            print >> sys.stderr, message

          if not killed:
            print >> sys.stderr, 'could not kill pid %d!' % proc_handles[0].pid
          else:
            print >> sys.stderr, 'program finished with exit code %d' % (
                proc_handles[0].returncode)

          # Prevent other timeouts from double-killing.
          del proc_handles[:]

    def timeout_func(timeout, proc_handles, log_event, finished_event):
      while log_event.wait(timeout):
        log_event.clear()
        if finished_event.is_set():
          return

      message = ('command timed out: %d seconds without output, attempting to '
                 'kill' % timeout)
      kill_proc(proc_handles, message)

    def maxtimeout_func(timeout, proc_handles, finished_event):
      if not finished_event.wait(timeout):
        message = ('command timed out: %d seconds elapsed' % timeout)
        kill_proc(proc_handles, message)

    timeout_thread = None
    maxtimeout_thread = None
    finished_event = threading.Event()

    if timeout:
      timeout_thread = threading.Thread(target=timeout_func,
                                        args=(timeout, proc_handles, log_event,
                                              finished_event))
      timeout_thread.daemon = True
    if max_time:
      maxtimeout_thread = threading.Thread(target=maxtimeout_func,
                                           args=(max_time, proc_handles,
                                                 finished_event))
      maxtimeout_thread.daemon = True

    thread.start()
    if timeout_thread:
      timeout_thread.start()
    if maxtimeout_thread:
      maxtimeout_thread.start()

    # Wait for the commands to terminate.
    for handle in proc_handles:
      handle.wait()

    # Wake up timeout threads.
    finished_event.set()
    log_event.set()

    # Wait for the reader thread to complete (implies EOF reached on stdout/
    # stderr pipes).
    thread.join()

    # Check whether any of the sub commands has failed.
    for handle in proc_handles:
      if handle.returncode:
        return handle.returncode

  # Wait for the command to terminate.
  proc.wait()
  return proc.returncode


def GetStatusOutput(command, **kwargs):
  """Runs the command list, returning its result and output."""
  proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, bufsize=1,
                          **kwargs)
  output = proc.communicate()[0]
  result = proc.returncode

  return (result, output)


def GetCommandOutput(command):
  """Runs the command list, returning its output.

  Run the command and returns its output (stdout and stderr) as a string.

  If the command exits with an error, raises ExternalError.
  """
  (result, output) = GetStatusOutput(command)
  if result:
    raise ExternalError('%s: %s' % (subprocess.list2cmdline(command), output))
  return output


def GetGClientCommand(platform=None):
  """Returns the executable command name, depending on the platform.
  """
  if not platform:
    platform = sys.platform
  if platform.startswith('win'):
    # Windows doesn't want to depend on bash.
    return 'gclient.bat'
  else:
    return 'gclient'


# Linux scripts use ssh to to move files to the archive host.
def SshMakeDirectory(host, dest_path):
  """Creates the entire dest_path on the remote ssh host.
  """
  command = ['ssh', host, 'mkdir', '-p', dest_path]
  result = RunCommand(command)
  if result:
    raise ExternalError('Failed to ssh mkdir "%s" on "%s" (%s)' %
                        (dest_path, host, result))


def SshMoveFile(host, src_path, dest_path):
  """Moves src_path (if it exists) to dest_path on the remote host.
  """
  command = ['ssh', host, 'test', '-e', src_path]
  result = RunCommand(command)
  if result:
    # Nothing to do if src_path doesn't exist.
    return result

  command = ['ssh', host, 'mv', src_path, dest_path]
  result = RunCommand(command)
  if result:
    raise ExternalError('Failed to ssh mv "%s" -> "%s" on "%s" (%s)' %
                        (src_path, dest_path, host, result))


def SshCopyFiles(srcs, host, dst):
  """Copies the srcs file(s) to dst on the remote ssh host.
  dst is expected to exist.
  """
  command = ['scp', srcs, host + ':' + dst]
  result = RunCommand(command)
  if result:
    raise ExternalError('Failed to scp "%s" to "%s" (%s)' %
                        (srcs, host + ':' + dst, result))


def SshExtractZip(host, zipname, dst):
  """extract the remote zip file to dst on the remote ssh host.
  """
  command = ['ssh', host, 'unzip', '-o', '-d', dst, zipname]
  result = RunCommand(command)
  if result:
    raise ExternalError('Failed to ssh unzip -o -d "%s" "%s" on "%s" (%s)' %
                        (dst, zipname, host, result))

  # unzip will create directories with access 700, which is not often what we
  # need. Fix the permissions for the whole archive.
  command = ['ssh', host, 'chmod', '-R', '755', dst]
  result = RunCommand(command)
  if result:
    raise ExternalError('Failed to ssh chmod -R 755 "%s" on "%s" (%s)' %
                        (dst, host, result))


def SshCopyTree(srctree, host, dst):
  """Recursively copies the srctree to dst on the remote ssh host.
  For consistency with shutil, dst is expected to not exist.
  """
  command = ['ssh', host, '[ -d "%s" ]' % dst]
  result = RunCommand(command)
  if result:
    raise ExternalError('SshCopyTree destination directory "%s" already exists.'
                        % host + ':' + dst)

  SshMakeDirectory(host, os.path.dirname(dst))
  command = ['scp', '-r', '-p', srctree, host + ':' + dst]
  result = RunCommand(command)
  if result:
    raise ExternalError('Failed to scp "%s" to "%s" (%s)' %
                        (srctree, host + ':' + dst, result))


def ListMasters(cue='master.cfg', include_public=True, include_internal=True):
  """Returns all the masters found."""
  # Look for "internal" masters first.
  path_internal = os.path.join(
      BUILD_DIR, os.pardir, 'build_internal', 'masters/*/' + cue)
  path = os.path.join(BUILD_DIR, 'masters/*/' + cue)
  filenames = []
  if include_public:
    filenames += glob.glob(path)
  if include_internal:
    filenames += glob.glob(path_internal)
  return [os.path.abspath(os.path.dirname(f)) for f in filenames]


def MasterPath(mastername, include_public=True, include_internal=True):
  path = os.path.join(BUILD_DIR, 'masters', 'master.%s' % mastername)
  path_internal = os.path.join(
      BUILD_DIR, os.pardir, 'build_internal', 'masters',
      'master.%s' % mastername)
  if include_public and os.path.isdir(path):
    return path
  if include_internal and os.path.isdir(path_internal):
    return path_internal
  raise LookupError('Path for master %s not found' % mastername)


def ListMastersWithSlaves(include_public=True, include_internal=True):
  masters_path = ListMasters('builders.pyl', include_public, include_internal)
  masters_path.extend(ListMasters('slaves.cfg', include_public,
                                  include_internal))
  return masters_path


def GetSlavesFromMasterPath(path, fail_hard=False):
  builders_path = os.path.join(path, 'builders.pyl')
  if os.path.exists(builders_path):
    return GetSlavesFromBuildersFile(builders_path)
  return RunSlavesCfg(os.path.join(path, 'slaves.cfg'), fail_hard=fail_hard)


def GetAllSlaves(fail_hard=False, include_public=True, include_internal=True):
  """Return all slave objects from masters."""
  slaves = []
  for master in ListMastersWithSlaves(include_public, include_internal):
    cur_slaves = GetSlavesFromMasterPath(master, fail_hard)
    for slave in cur_slaves:
      slave['mastername'] = os.path.basename(master)
    slaves.extend(cur_slaves)
  return slaves


def GetSlavesForHost():
  """Get slaves for a host, defaulting to current host."""
  hostname = os.getenv('TESTING_SLAVENAME')
  if not hostname:
    hostname = socket.getfqdn().split('.', 1)[0].lower()
  return [s for s in GetAllSlaves() if s.get('hostname') == hostname]


def GetActiveSubdir():
  """Get current checkout's subdir, if checkout uses subdir layout."""
  rootdir, subdir = os.path.split(os.path.dirname(BUILD_DIR))
  if subdir != 'b' and os.path.basename(rootdir) == 'c':
    return subdir


def GetActiveSlavename():
  slavename = os.getenv('TESTING_SLAVENAME')
  if not slavename:
    slavename = socket.getfqdn().split('.', 1)[0].lower()
  subdir = GetActiveSubdir()
  if subdir:
    return '%s#%s' % (slavename, subdir)
  return slavename


def EntryToSlaveName(entry):
  """Produces slave name from the slaves config dict."""
  name = entry.get('slavename') or entry.get('hostname')
  if 'subdir' in entry:
    return '%s#%s' % (name, entry['subdir'])
  return name


def GetActiveMaster(slavename=None, default=None):
  """Returns the name of the Active master serving the current host.

  Parse all of the active masters with slaves matching the current hostname
  and optional slavename. Returns |default| if no match found.
  """
  slavename = slavename or GetActiveSlavename()
  for slave in GetAllSlaves():
    if slavename == EntryToSlaveName(slave):
      return slave['master']
  return default


@contextmanager
def MasterEnvironment(master_dir):
  """Context manager that enters an enviornment similar to a master's.

  This involves:
    - Modifying 'sys.path' to include paths available to the master.
    - Changing directory (via os.chdir()) to the master's base directory.

  These changes will be reverted after the context manager completes.

  Args:
    master_dir: (str) The master's base directory.
  """
  master_dir = os.path.abspath(master_dir)

  # Setup a 'sys.path' that is adequate for loading 'slaves.cfg'.
  old_cwd = os.getcwd()

  with env.GetInfraPythonPath(master_dir=master_dir).Enter():
    try:
      os.chdir(master_dir)
      yield
    finally:
      os.chdir(old_cwd)


def ParsePythonCfg(cfg_filepath, fail_hard=False):
  """Retrieves data from a python config file."""
  if not os.path.exists(cfg_filepath):
    return None

  # Execute 'slaves.sfg' in the master path environment.
  with MasterEnvironment(os.path.dirname(os.path.abspath(cfg_filepath))):
    try:
      local_vars = {}
      execfile(os.path.join(cfg_filepath), local_vars)
      del local_vars['__builtins__']
      return local_vars
    except Exception as e:
      # pylint: disable=C0323
      print >>sys.stderr, 'An error occurred while parsing %s: %s' % (
          cfg_filepath, e)
      print >>sys.stderr, traceback.format_exc()  # pylint: disable=C0323
      if fail_hard:
        raise
      return {}


def RunSlavesCfg(slaves_cfg, fail_hard=False):
  """Runs slaves.cfg in a consistent way."""
  slave_config = ParsePythonCfg(slaves_cfg, fail_hard=fail_hard) or {}
  return slave_config.get('slaves', [])


def convert_json(option, _, value, parser):
  """Provide an OptionParser callback to unmarshal a JSON string."""
  setattr(parser.values, option.dest, json.loads(value))


def b64_gz_json_encode(obj):
  """Serialize a python object into base64."""
  # The |separators| argument is to densify the command line.
  return base64.b64encode(zlib.compress(
      json.dumps(obj or {}, sort_keys=True, separators=(',', ':')), 9))


def convert_gz_json(option, _, value, parser):
  """Provide an OptionParser callback to unmarshal a b64 gz JSON string."""
  setattr(
      parser.values, option.dest,
      json.loads(zlib.decompress(base64.b64decode(value))))


def SafeTranslate(inputstr):
  """Convert a free form string to one that can be used in a path.

  This is similar to the safeTranslate function in buildbot.
  """

  badchars_map = string.maketrans('\t !#$%&\'()*+,./:;<=>?@[\\]^{|}~',
                                  '______________________________')
  if isinstance(inputstr, unicode):
    inputstr = inputstr.encode('utf8')
  return inputstr.translate(badchars_map)


def GetPrimaryProject(options):
  """Returns: (str) the key of the primary project, or 'None' if none exists.
  """
  # The preferred way is to reference the 'primary_project' parameter.
  result = options.build_properties.get('primary_project')
  if result:
    return result

  # TODO(dnj): The 'primary_repo' parameter is used by some scripts to indictate
  #     the primary project name. This is not consistently used and will be
  #     deprecated in favor of 'primary_project' once that is rolled out.
  result = options.build_properties.get('primary_repo')
  if not result:
    # The 'primary_repo' property currently contains a trailing underscore.
    # However, this isn't an obvious thing given its name, so we'll strip it
    # here and remove that expectation.
    return result.strip('_')
  return None


def GetBuildSortKey(options, project=None):
  """Reads a variety of sources to determine the current build revision.

  NOTE: Currently, the return value does not qualify branch name. This can
  present a problem with git numbering scheme, where numbers are only unique
  in the context of their respective branches. When this happens, this
  function will return a branch name as part of the sort key and its callers
  will need to adapt their naming/querying schemes to accommodate this. Until
  then, we will return 'None' as the branch name.
  (e.g., refs/foo/bar@{#12345} => ("refs/foo/bar", 12345)

  Args:
    options: Command-line options structure
    project: (str/None) If not None, the project to get the build sort key
        for. Otherwise, the build-wide sort key will be used.
  Returns: (branch, value) The qualified sortkey value
    branch: (str/None) The name of the branch, or 'None' if there is no branch
        context. Currently this always returns 'None'.
    value: (int) The iteration value within the specified branch
  Raises: (NoIdentifiedRevision) if no revision could be identified from the
      supplied options.
  """
  # Is there a commit position for this build key?
  try:
    return GetCommitPosition(options, project=project)
  except NoIdentifiedRevision:
    pass

  # Nope; derive the sort key from the 'got_[*_]revision' build properties. Note
  # that this could be a Git commit (post flag day).
  if project:
    revision_key = 'got_%s_revision' % (project,)
  else:
    revision_key = 'got_revision'
  revision = options.build_properties.get(revision_key)
  if revision and not IsGitCommit(revision):
    return None, int(revision)
  raise NoIdentifiedRevision("Unable to identify revision for revision key "
                             "[%s]" % (revision_key,))


def GetGitCommit(options, project=None):
  """Returns the 'git' commit hash for the specified repository

  This function uses environmental options to identify the 'git' commit hash
  for the specified repository.

  Args:
    options: Command-line options structure
    project: (str/None) The project key to use. If None, use the topmost
        repository identification properties.
  Raises: (NoIdentifiedRevision) if no git commit could be identified from the
      supplied options.
  """
  if project:
    git_commit_key = 'got_%s_revision_git' % (project,)
  else:
    git_commit_key = 'got_revision_git'
  commit = options.build_properties.get(git_commit_key)
  if commit:
    return commit

  # Is 'got_[_*]revision' itself is the Git commit?
  if project:
    commit_key = 'got_%s_revision' % (project,)
  else:
    commit_key = 'got_revision'
  commit = options.build_properties.get(commit_key)
  if commit and IsGitCommit(commit):
    return commit
  raise NoIdentifiedRevision("Unable to identify commit for commit key: %s" % (
                           (git_commit_key, commit_key),))


def GetSortableUploadPathForSortKey(branch, value, delimiter=None):
  """Returns: (str) the canonical sort key path constructed from a sort key.

  Returns a canonical sort key path for a sort key. The result will be one of
  the following forms:
  - (Without Branch or With Branch=='refs/heads/master'): <value> (e.g., 12345)
  - (With non-Master Branch): <branch-path>-<value> (e.g.,
        "refs_my-branch-12345")

  When a 'branch' is supplied, it is converted to a path-suitable form. This
  conversion replaces undesirable characters ('/') with underscores.

  Note that when parsing the upload path, 'rsplit' should be used to isolate the
  commit position value, as the branch path may have instances of the delimiter
  in it.

  See 'GetBuildSortKey' for more information about sort keys.

  Args:
    branch: (str/None) The sort key branch, or 'None' if there is no associated
        branch.
    value: (int) The sort key value.
    delimiter: (str) The delimiter to insert in between <branch-path> and
        <value> when constructing the branch-inclusive form. If omitted
        (default), a hyphen ('-') will be used.
  """
  if branch and branch != 'refs/heads/master':
    delimiter = delimiter or '-'
    branch = branch.replace('/', '_')
    return '%s%s%s' % (branch, delimiter, value)
  return str(value)


def ParseCommitPosition(value):
  """Returns: The (branch, value) parsed from a commit position string.

  Args:
    value: (str) The value to parse.
  Raises:
    ValueError: If a commit position could not be parsed from 'value'.
  """
  match = COMMIT_POSITION_RE.match(value)
  if not match:
    raise ValueError("Failed to parse commit position from '%s'" % (value,))
  return match.group(1), int(match.group(2))


def BuildCommitPosition(branch, value):
  """Returns: A constructed commit position.

  An example commit position for branch 'refs/heads/master' value '12345' is:
  refs/heads/master@{#12345}

  This value can be parsed via 'ParseCommitPosition'.

  Args:
    branch: (str) The name of the commit position branch
    value: (int): The commit position number.
  """
  return '%s@{#%s}' % (branch, value)


def GetCommitPosition(options, project=None):
  """Returns: (branch, value) The parsed commit position from build options.

  Returns the parsed commit position from the build options. This is identified
  by examining the 'got_revision_cp' (or 'got_REPO_revision_cp', if 'project' is
  specified) keys.

  Args:
    options: Command-line options structure
    project: (str/None) If not None, the project to get the build sort key
        for. Otherwise, the build-wide sort key will be used.
  Returns: (branch, value) The qualified commit position value
  Raises:
    NoIdentifiedRevision: if no revision could be identified from the
        supplied options.
    ValueError: If the supplied commit position failed to parse successfully.
  """
  if project:
    key = 'got_%s_revision_cp' % (project,)
  else:
    key = 'got_revision_cp'
  cp = options.build_properties.get(key)
  if not cp:
    raise NoIdentifiedRevision("Unable to identify the commit position; the "
                               "build property is missing: %s" % (key,))
  return ParseCommitPosition(cp)


def AddPropertiesOptions(option_parser):
  """Registers command line options for parsing build and factory properties.

  After parsing, the options object will have the 'build_properties' and
  'factory_properties' attributes. The corresponding values will be python
  dictionaries containing the properties. If the options are not given on
  the command line, the dictionaries will be empty.

  Args:
    option_parser: An optparse.OptionParser to register command line options
                   for build and factory properties.
  """
  option_parser.add_option('--build-properties', action='callback',
                           callback=convert_json, type='string',
                           nargs=1, default={},
                           help='build properties in JSON format')
  option_parser.add_option('--factory-properties', action='callback',
                           callback=convert_json, type='string',
                           nargs=1, default={},
                           help='factory properties in JSON format')


def AddThirdPartyLibToPath(lib, override=False):
  """Adds the specified dir in build/third_party to sys.path.

  Setting 'override' to true will place the directory in the beginning of
  sys.path, useful for overriding previously set packages.

  NOTE: We would like to deprecate this method, as it allows (encourages?)
      scripts to define their own one-off Python path sequences, creating a
      difficult-to-manage state where different scripts and libraries have
      different path expectations. Please don't use this method if possible;
      it preferred to augment 'common.env' instead.
  """
  libpath = os.path.abspath(os.path.join(BUILD_DIR, 'third_party', lib))
  if override:
    sys.path.insert(0, libpath)
  else:
    sys.path.append(libpath)


def GetLKGR():
  """Connect to chromium LKGR server and get LKGR revision.

  On success, returns the LKGR and 'ok'. On error, returns None and the text of
  the error message.
  """

  try:
    conn = urllib.urlopen('https://chromium-status.appspot.com/lkgr')
  except IOError:
    return (None, 'Error connecting to LKGR server! Is your internet '
            'connection working properly?')
  try:
    rev = int('\n'.join(conn.readlines()))
  except IOError:
    return (None, 'Error connecting to LKGR server! Is your internet '
            'connection working properly?')
  except ValueError:
    return None, 'LKGR server returned malformed data! Aborting...'
  finally:
    conn.close()

  return rev, 'ok'


def AbsoluteCanonicalPath(*path):
  """Return the most canonical path Python can provide."""

  file_path = os.path.join(*path)
  return os.path.realpath(os.path.abspath(os.path.expanduser(file_path)))


def IsolatedImportFromPath(path, extra_paths=None):
  dir_path, module_file = os.path.split(path)
  module_file = os.path.splitext(module_file)[0]

  saved = sys.path
  sys.path = [dir_path] + (extra_paths or [])
  try:
    return __import__(module_file)
  except ImportError:
    pass
  finally:
    sys.path = saved


@contextmanager
def MultiPool(processes):
  """Manages a multiprocessing.Pool making sure to close the pool when done.

  This will also call pool.terminate() when an exception is raised (and
  re-raised the exception to the calling procedure can handle it).
  """
  try:
    pool = multiprocessing.Pool(processes=processes)
    yield pool
    pool.close()
  except:
    pool.terminate()
    raise
  finally:
    pool.join()


def ReadJsonAsUtf8(filename=None, text=None):
  """Read a json file or string and output a dict.

  This function is different from json.load and json.loads in that it
  returns utf8-encoded string for keys and values instead of unicode.

  Args:
  filename: path of a file to parse
  text: json string to parse

  If both 'filename' and 'text' are provided, 'filename' is used.
  """
  def _decode_list(data):
    rv = []
    for item in data:
      if isinstance(item, unicode):
        item = item.encode('utf-8')
      elif isinstance(item, list):
        item = _decode_list(item)
      elif isinstance(item, dict):
        item = _decode_dict(item)
      rv.append(item)
    return rv

  def _decode_dict(data):
    rv = {}
    for key, value in data.iteritems():
      if isinstance(key, unicode):
        key = key.encode('utf-8')
      if isinstance(value, unicode):
        value = value.encode('utf-8')
      elif isinstance(value, list):
        value = _decode_list(value)
      elif isinstance(value, dict):
        value = _decode_dict(value)
      rv[key] = value
    return rv

  if filename:
    with open(filename, 'rb') as f:
      return json.load(f, object_hook=_decode_dict)
  if text:
    return json.loads(text, object_hook=_decode_dict)


def GetMasterDevParameters(filename='master_cfg_params.json'):
  """Look for master development parameter files in the master directory.

  Return the parsed content if the file exists, as a dictionary.
  Every string value in the dictionary is utf8-encoded str.

  If the file is not found, returns an empty dict. This is on purpose, to
  make the file optional.
  """
  if os.path.isfile(filename):
    return ReadJsonAsUtf8(filename=filename)
  return {}


def FileExclusions():
  all_platforms = ['.landmines', 'obj', 'gen', '.ninja_deps', '.ninja_log']
  # Skip files that the testers don't care about. Mostly directories.
  if IsWindows():
    # Remove obj or lib dir entries
    return all_platforms + ['cfinstaller_archive', 'lib', 'installer_archive']
  if IsMac():
    return all_platforms + [
      # We don't need the arm bits v8 builds.
      'd8_arm', 'v8_shell_arm',
      # pdfsqueeze is a build helper, no need to copy it to testers.
      'pdfsqueeze',
      # We copy the framework into the app bundle, we don't need the second
      # copy outside the app.
      # TODO(mark): Since r28431, the copy in the build directory is actually
      # used by tests.  Putting two copies in the .zip isn't great, so maybe
      # we can find another workaround.
      # 'Chromium Framework.framework',
      # 'Google Chrome Framework.framework',
      # We copy the Helper into the app bundle, we don't need the second
      # copy outside the app.
      'Chromium Helper.app',
      'Google Chrome Helper.app',
      'App Shim Socket',
      '.deps', 'obj.host', 'obj.target', 'lib'
    ]
  if IsLinux():
    return all_platforms + [
      # intermediate build directories (full of .o, .d, etc.).
      'appcache', 'glue', 'lib.host', 'obj.host',
      'obj.target', 'src', '.deps',
      # scons build cruft
      '.sconsign.dblite',
      # build helper, not needed on testers
      'mksnapshot',
    ]

  return all_platforms


def DatabaseSetup(buildmaster_config, require_dbconfig=False):
  """Read database credentials in the master directory."""
  if os.path.isfile('.dbconfig'):
    values = {}
    execfile('.dbconfig', values)
    if 'password' not in values:
      raise Exception('could not get db password')

    buildmaster_config['db_url'] = 'postgresql://%s:%s@%s/%s' % (
        values['username'], values['password'],
        values.get('hostname', 'localhost'), values['dbname'])
  else:
    assert not require_dbconfig


def ReadBuildersFile(builders_path):
  with open(builders_path) as fp:
    contents = fp.read()
  return ParseBuildersFileContents(builders_path, contents)


def ParseBuildersFileContents(path, contents):
  builders = ast.literal_eval(contents)

  # Set some additional derived fields that are derived from the
  # file's location in the filesystem.
  basedir = os.path.dirname(os.path.abspath(path))
  master_dirname = os.path.basename(basedir)
  master_name_comps = master_dirname.split('.')[1:]
  buildbot_path =  '.'.join(master_name_comps)
  master_classname =  ''.join(c[0].upper() + c[1:] for c in master_name_comps)
  builders['master_dirname'] = master_dirname
  builders.setdefault('master_classname', master_classname)
  builders.setdefault('buildbot_url',
                      'https://build.chromium.org/p/%s/' % buildbot_path)

  builders.setdefault('buildbucket_bucket', None)
  builders.setdefault('service_account_file', None)

  # The _str fields are printable representations of Python values:
  # if builders['foo'] == "hello", then builders['foo_str'] == "'hello'".
  # This allows them to be read back in by Python scripts properly.
  builders['buildbucket_bucket_str'] = repr(builders['buildbucket_bucket'])
  builders['service_account_file_str'] = repr(builders['service_account_file'])

  return builders


def GetSlavesFromBuildersFile(builders_path):
  """Read builders_path and return a list of slave dicts."""
  builders = ReadBuildersFile(builders_path)
  return GetSlavesFromBuilders(builders)


def GetSlavesFromBuilders(builders):
  """Returns a list of slave dicts derived from the builders dict."""
  builders_in_pool = {}

  # builders.pyl contains a list of builders -> slave_pools
  # and a list of slave_pools -> slaves.
  # We require that each slave is in a single pool, but each slave
  # may have multiple builders, so we need to build up the list of
  # builders each slave pool supports.
  for builder_name, builder_vals in builders['builders'].items():
    pool_names = builder_vals['slave_pools']
    for pool_name in pool_names:
     if pool_name not in builders_in_pool:
       builders_in_pool[pool_name] = set()
     pool_data = builders['slave_pools'][pool_name]
     for slave in pool_data['slaves']:
       builders_in_pool[pool_name].add(builder_name)

  # Now we can generate the list of slaves using the above lookup table.
  slaves = []
  for pool_name, pool_data in builders['slave_pools'].items():
    slave_data = pool_data['slave_data']
    builder_names = sorted(builders_in_pool[pool_name])
    for slave in pool_data['slaves']:
      slaves.append({
          'hostname': slave,
          'builder_name': builder_names,
          'master': builders['master_classname'],
          'os': slave_data['os'],
          'version': slave_data['version'],
          'bits': slave_data['bits'],
      })

  return slaves

def GetSlaveNamesForBuilder(builders, builder_name):
  """Returns a list of slave hostnames for the given builder name."""
  slaves = []
  pool_names = builders['builders'][builder_name]['slave_pools']
  for pool_name in pool_names:
    slaves.extend(builders['slave_pools'][pool_name]['slaves'])
  return slaves
