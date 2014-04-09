# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library to make common google storage operations more reliable."""

import contextlib
import datetime
import getpass
import hashlib
import logging
import os
import re
import urlparse
import uuid

from chromite.buildbot import constants
from chromite.lib import cache
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import retry_util
from chromite.lib import timeout_util

PUBLIC_BASE_HTTPS_URL = 'https://commondatastorage.googleapis.com/'
PRIVATE_BASE_HTTPS_URL = 'https://storage.cloud.google.com/'
BASE_GS_URL = 'gs://'

# Format used by "gsutil ls -l" when reporting modified time.
DATETIME_FORMAT = '%Y-%m-%dT%H:%M:%SZ'

# Regexp for parsing each line of output from "gsutil ls -l".
# This regexp is prepared for the generation and meta_generation values,
# too, even though they are not expected until we use "-a".
LS_LA_RE = re.compile(
    r'^\s*(\d*?)\s+(\S*?)\s+([^#$]+).*?(#(\d+)\s+meta_?generation=(\d+))?\s*$')


def CanonicalizeURL(url, strict=False):
  """Convert provided URL to gs:// URL, if it follows a known format.

  Args:
    url: URL to canonicalize.
    strict: Raises exception if URL cannot be canonicalized.
  """
  for prefix in (PUBLIC_BASE_HTTPS_URL, PRIVATE_BASE_HTTPS_URL):
    if url.startswith(prefix):
      return url.replace(prefix, BASE_GS_URL, 1)

  if not url.startswith(BASE_GS_URL) and strict:
    raise ValueError('Url %r cannot be canonicalized.' % url)

  return url


def GetGsURL(bucket, for_gsutil=False, public=True, suburl=''):
  """Construct a Google Storage URL

  Args:
    bucket: The Google Storage bucket to use
    for_gsutil: Do you want a URL for passing to `gsutil`?
    public: Do we want the public or private url
    suburl: A url fragment to tack onto the end

  Returns:
    The fully constructed URL
  """
  if for_gsutil:
    urlbase = BASE_GS_URL
  else:
    urlbase = PUBLIC_BASE_HTTPS_URL if public else PRIVATE_BASE_HTTPS_URL
  return '%s%s/%s' % (urlbase, bucket, suburl)


class GSContextException(Exception):
  """Base exception for all exceptions thrown by GSContext."""


# Since the underlying code uses RunCommand, some callers might be trying to
# catch cros_build_lib.RunCommandError themselves.  Extend that class so that
# code continues to work.
class GSCommandError(GSContextException, cros_build_lib.RunCommandError):
  """Thrown when an error happened we couldn't decode."""


class GSContextPreconditionFailed(GSContextException):
  """Thrown when google storage returns code=PreconditionFailed."""


class GSNoSuchKey(GSContextException):
  """Thrown when google storage returns code=NoSuchKey."""


class GSCounter(object):
  """A counter class for Google Storage."""

  def __init__(self, ctx, path):
    """Create a counter object.

    Args:
      ctx: A GSContext object.
      path: The path to the counter in Google Storage.
    """
    self.ctx = ctx
    self.path = path

  def Get(self):
    """Get the current value of a counter."""
    try:
      return int(self.ctx.Cat(self.path).output)
    except GSNoSuchKey:
      return 0

  def AtomicCounterOperation(self, default_value, operation):
    """Atomically set the counter value using |operation|.

    Args:
      default_value: Default value to use for counter, if counter
                     does not exist.
      operation: Function that takes the current counter value as a
                 parameter, and returns the new desired value.

    Returns:
      The new counter value. None if value could not be set.
    """
    generation, _ = self.ctx.GetGeneration(self.path)
    for _ in xrange(self.ctx.retries + 1):
      try:
        value = default_value if generation == 0 else operation(self.Get())
        self.ctx.Copy('-', self.path, input=str(value), version=generation)
        return value
      except (GSContextPreconditionFailed, GSNoSuchKey):
        # GSContextPreconditionFailed is thrown if another builder is also
        # trying to update the counter and we lost the race. GSNoSuchKey is
        # thrown if another builder deleted the counter. In either case, fetch
        # the generation again, and, if it has changed, try the copy again.
        new_generation, _ = self.ctx.GetGeneration(self.path)
        if new_generation == generation:
          raise
        generation = new_generation

  def Increment(self):
    """Increment the counter.

    Returns:
      The new counter value. None if value could not be set.
    """
    return self.AtomicCounterOperation(1, lambda x: x + 1)

  def Decrement(self):
    """Decrement the counter.

    Returns:
      The new counter value. None if value could not be set.
    """
    return self.AtomicCounterOperation(-1, lambda x: x - 1)

  def Reset(self):
    """Reset the counter to zero.

    Returns:
      The new counter value. None if value could not be set.
    """
    return self.AtomicCounterOperation(0, lambda x: 0)

  def StreakIncrement(self):
    """Increment the counter if it is positive, otherwise set it to 1.

    Returns:
      The new counter value. None if value could not be set.
    """
    return self.AtomicCounterOperation(1, lambda x: x + 1 if x > 0 else 1)

  def StreakDecrement(self):
    """Decrement the counter if it is negative, otherwise set it to -1.

    Returns:
      The new counter value. None if value could not be set.
    """
    return self.AtomicCounterOperation(-1, lambda x: x - 1 if x < 0 else -1)


class GSContext(object):
  """A class to wrap common google storage operations."""

  # Error messages that indicate an invalid BOTO config.
  AUTHORIZATION_ERRORS = ('no configured', 'detail=Authorization')

  DEFAULT_BOTO_FILE = os.path.expanduser('~/.boto')
  DEFAULT_GSUTIL_TRACKER_DIR = os.path.expanduser('~/.gsutil')
  # This is set for ease of testing.
  DEFAULT_GSUTIL_BIN = None
  DEFAULT_GSUTIL_BUILDER_BIN = '/b/build/third_party/gsutil/gsutil'
  # How many times to retry uploads.
  DEFAULT_RETRIES = 3

  # Multiplier for how long to sleep (in seconds) between retries; will delay
  # (1*sleep) the first time, then (2*sleep), continuing via attempt * sleep.
  DEFAULT_SLEEP_TIME = 60

  GSUTIL_TAR = 'gsutil_3.42.tar.gz'
  GSUTIL_URL = PUBLIC_BASE_HTTPS_URL + 'pub/%s' % GSUTIL_TAR

  RESUMABLE_UPLOAD_ERROR = ('Too many resumable upload attempts failed without '
                            'progress')
  RESUMABLE_DOWNLOAD_ERROR = ('Too many resumable download attempts failed '
                              'without progress')

  @classmethod
  def GetDefaultGSUtilBin(cls, cache_dir=None):
    if cls.DEFAULT_GSUTIL_BIN is None:
      if cache_dir is None:
        # Import here to avoid circular imports (commandline imports gs).
        from chromite.lib import commandline
        cache_dir = commandline.GetCacheDir()
      if cache_dir is not None:
        common_path = os.path.join(cache_dir, constants.COMMON_CACHE)
        tar_cache = cache.TarballCache(common_path)
        key = (cls.GSUTIL_TAR,)
        # The common cache will not be LRU, removing the need to hold a read
        # lock on the cached gsutil.
        ref = tar_cache.Lookup(key)
        ref.SetDefault(cls.GSUTIL_URL)
        cls.DEFAULT_GSUTIL_BIN = os.path.join(ref.path, 'gsutil', 'gsutil')
      else:
        # Check if the default gsutil path for builders exists. If
        # not, try locating gsutil. If none exists, simply use 'gsutil'.
        gsutil_bin = cls.DEFAULT_GSUTIL_BUILDER_BIN
        if not os.path.exists(gsutil_bin):
          gsutil_bin = osutils.Which('gsutil')
        if gsutil_bin is None:
          gsutil_bin = 'gsutil'
        cls.DEFAULT_GSUTIL_BIN = gsutil_bin

    return cls.DEFAULT_GSUTIL_BIN

  def __init__(self, boto_file=None, cache_dir=None, acl=None,
               dry_run=False, gsutil_bin=None, init_boto=False, retries=None,
               sleep=None):
    """Constructor.

    Args:
      boto_file: Fully qualified path to user's .boto credential file.
      cache_dir: The absolute path to the cache directory. Use the default
        fallback if not given.
      acl: If given, a canned ACL. It is not valid to pass in an ACL file
        here, because most gsutil commands do not accept ACL files. If you
        would like to use an ACL file, use the SetACL command instead.
      dry_run: Testing mode that prints commands that would be run.
      gsutil_bin: If given, the absolute path to the gsutil binary.  Else
        the default fallback will be used.
      init_boto: If set to True, GSContext will check during __init__ if a
        valid boto config is configured, and if not, will attempt to ask the
        user to interactively set up the boto config.
      retries: Number of times to retry a command before failing.
      sleep: Amount of time to sleep between failures.
    """
    if gsutil_bin is None:
      gsutil_bin = self.GetDefaultGSUtilBin(cache_dir)
    else:
      self._CheckFile('gsutil not found', gsutil_bin)
    self.gsutil_bin = gsutil_bin

    # The version of gsutil is retrieved on demand and cached here.
    self._gsutil_version = None

    # TODO (yjhong): disable parallel composite upload for now because
    # it is not backward compatible (older gsutil versions cannot
    # download files uploaded with this option enabled). Remove this
    # after all users transition to newer versions (3.37 and above).
    self.gsutil_flags = ['-o', 'GSUtil:parallel_composite_upload_threshold=0']

    # Set HTTP proxy if environment variable http_proxy is set
    # (crbug.com/325032).
    if 'http_proxy' in os.environ:
      url = urlparse.urlparse(os.environ['http_proxy'])
      if not url.hostname or (not url.username and url.password):
        logging.warning('GS_ERROR: Ignoring env variable http_proxy because it '
                        'is not properly set: %s', os.environ['http_proxy'])
      else:
        self.gsutil_flags += ['-o', 'Boto:proxy=%s' % url.hostname]
        if url.username:
          self.gsutil_flags += ['-o', 'Boto:proxy_user=%s' % url.username]
        if url.password:
          self.gsutil_flags += ['-o', 'Boto:proxy_pass=%s' % url.password]
        if url.port:
          self.gsutil_flags += ['-o', 'Boto:proxy_port=%d' % url.port]

    # Increase the number of retries. With 10 retries, Boto will try a total of
    # 11 times and wait up to 2**11 seconds (~30 minutes) in total, not
    # not including the time spent actually uploading or downloading.
    self.gsutil_flags += ['-o', 'Boto:num_retries=10']

    # Prefer boto_file if specified, else prefer the env then the default.
    default_boto = False
    if boto_file is None:
      boto_file = os.environ.get('BOTO_CONFIG')
      if boto_file is None:
        default_boto = True
        boto_file = self.DEFAULT_BOTO_FILE
    self.boto_file = boto_file

    self.acl = acl

    self.dry_run = dry_run
    self.retries = self.DEFAULT_RETRIES if retries is None else int(retries)
    self._sleep_time = self.DEFAULT_SLEEP_TIME if sleep is None else int(sleep)

    if init_boto:
      self._InitBoto()

    if not default_boto:
      self._CheckFile('Boto credentials not found', boto_file)

  @property
  def gsutil_version(self):
    """Return the version of the gsutil in this context."""
    if not self._gsutil_version:
      cmd = ['-q', 'version']

      # gsutil has been known to return version to stderr in the past, so
      # use combine_stdout_stderr=True.
      result = self.DoCommand(cmd, combine_stdout_stderr=True)

      # Expect output like: gsutil version 3.35
      match = re.search(r'^\s*gsutil\s+version\s+([\d.]+)', result.output,
                        re.IGNORECASE)
      if match:
        self._gsutil_version = match.group(1)
      else:
        raise GSContextException('Unexpected output format from "%s":\n%s.' %
                                 (result.cmdstr, result.output))

    return self._gsutil_version

  def _CheckFile(self, errmsg, afile):
    """Pre-flight check for valid inputs.

    Args:
      errmsg: Error message to display.
      afile: Fully qualified path to test file existance.
    """
    if not os.path.isfile(afile):
      raise GSContextException('%s, %s is not a file' % (errmsg, afile))

  def _TestGSLs(self):
    """Quick test of gsutil functionality."""
    result = self.DoCommand(['ls'], retries=0, debug_level=logging.DEBUG,
                            redirect_stderr=True, error_code_ok=True)
    return not (result.returncode == 1 and
                any(e in result.error for e in self.AUTHORIZATION_ERRORS))

  def _ConfigureBotoConfig(self):
    """Make sure we can access protected bits in GS."""
    print 'Configuring gsutil. **Please use your @google.com account.**'
    try:
      self.DoCommand(['config'], retries=0, debug_level=logging.CRITICAL,
                     print_cmd=False)
    finally:
      if (os.path.exists(self.boto_file) and not
          os.path.getsize(self.boto_file)):
        os.remove(self.boto_file)
        raise GSContextException('GS config could not be set up.')

  def _InitBoto(self):
    if not self._TestGSLs():
      self._ConfigureBotoConfig()

  def Cat(self, path, **kwargs):
    """Returns the contents of a GS object."""
    kwargs.setdefault('redirect_stdout', True)
    if not path.startswith(BASE_GS_URL):
      # gsutil doesn't support cat-ting a local path, so just run 'cat' in that
      # case.
      kwargs.pop('retries', None)
      kwargs.pop('headers', None)
      return cros_build_lib.RunCommand(['cat', path], **kwargs)
    return self.DoCommand(['cat', path], **kwargs)

  def CopyInto(self, local_path, remote_dir, filename=None, **kwargs):
    """Upload a local file into a directory in google storage.

    Args:
      local_path: Local file path to copy.
      remote_dir: Full gs:// url of the directory to transfer the file into.
      filename: If given, the filename to place the content at; if not given,
        it's discerned from basename(local_path).
      **kwargs: See Copy() for documentation.
    """
    filename = filename if filename is not None else local_path
    # Basename it even if an explicit filename was given; we don't want
    # people using filename as a multi-directory path fragment.
    return self.Copy(local_path,
                      '%s/%s' % (remote_dir, os.path.basename(filename)),
                      **kwargs)

  @staticmethod
  def _GetTrackerFilenames(dest_path):
    """Returns a list of gsutil tracker filenames.

    Tracker files are used by gsutil to resume downloads/uploads. This
    function does not handle parallel uploads.

    Args:
      dest_path: Either a GS path or an absolute local path.

    Returns:
      The list of potential tracker filenames.
    """
    dest = urlparse.urlsplit(dest_path)
    filenames = []
    if dest.scheme == 'gs':
      prefix = 'upload'
      bucket_name = dest.netloc
      object_name = dest.path.lstrip('/')
      filenames.append(
          re.sub(r'[/\\]', '_', 'resumable_upload__%s__%s.url' %
                 (bucket_name, object_name)))
    else:
      prefix = 'download'
      filenames.append(
          re.sub(r'[/\\]', '_', 'resumable_download__%s.etag' % dest.path))

    hashed_filenames = []
    for filename in filenames:
      if not isinstance(filename, unicode):
        filename = unicode(filename, 'utf8').encode('utf-8')
      m = hashlib.sha1(filename)
      hashed_filenames.append('%s_TRACKER_%s.%s' %
                              (prefix, m.hexdigest(), filename[-16:]))

    return hashed_filenames

  def _RetryFilter(self, e):
    """Function to filter retry-able RunCommandError exceptions.

    Args:
      e: Exception object to filter. Exception may be re-raised as
         as different type, if _RetryFilter determines a more appropriate
         exception type based on the contents of e.

    Returns:
      True for exceptions thrown by a RunCommand gsutil that should be retried.
    """
    if not retry_util.ShouldRetryCommandCommon(e):
      return False

    # e is guaranteed by above filter to be a RunCommandError

    if e.result.returncode < 0:
      logging.info('Child process received signal %d; not retrying.',
                   -e.result.returncode)
      return False

    error = e.result.error
    if error:
      if 'GSResponseError' in error:
        if 'code=PreconditionFailed' in error:
          raise GSContextPreconditionFailed(e)
        if 'code=NoSuchKey' in error:
          raise GSNoSuchKey(e)

      # If the file does not exist, one of the following errors occurs.
      if ('InvalidUriError:' in error or
          'Attempt to get key for' in error or
          'CommandException: No URIs matched' in error or
          'CommandException: One or more URIs matched no objects' in error or
          'CommandException: No such object' in error or
          'Some files could not be removed' in error or
          'does not exist' in error):
        raise GSNoSuchKey(e)

      logging.warning('GS_ERROR: %s', error)

      # TODO: Below is a list of known flaky errors that we should
      # retry. The list needs to be extended.

      # Temporary fix: remove the gsutil tracker files so that our retry
      # can hit a different backend. This should be removed after the
      # bug is fixed by the Google Storage team (see crbug.com/308300).
      if (self.RESUMABLE_DOWNLOAD_ERROR in error or
          self.RESUMABLE_UPLOAD_ERROR in error or
          'ResumableUploadException' in error or
          'ResumableDownloadException' in error):

        # Only remove the tracker files if we try to upload/download a file.
        if 'cp' in e.result.cmd[:-2]:
          # Assume a command: gsutil [options] cp [options] src_path dest_path
          # dest_path needs to be a fully qualified local path, which is already
          # required for GSContext.Copy().
          tracker_filenames = self._GetTrackerFilenames(e.result.cmd[-1])
          logging.info('Potential list of tracker files: %s',
                       tracker_filenames)
          for tracker_filename in tracker_filenames:
            tracker_file_path = os.path.join(self.DEFAULT_GSUTIL_TRACKER_DIR,
                                             tracker_filename)
            if os.path.exists(tracker_file_path):
              logging.info('Deleting gsutil tracker file %s before retrying.',
                           tracker_file_path)
              logging.info('The content of the tracker file: %s',
                           osutils.ReadFile(tracker_file_path))
              osutils.SafeUnlink(tracker_file_path)
        return True

      # We have seen flaky errors with 5xx return codes.
      if 'GSResponseError: status=5' in error:
        return True

    return False

  # TODO(mtennant): Make a private method.
  def DoCommand(self, gsutil_cmd, headers=(), retries=None, version=None,
                parallel=True, **kwargs):
    """Run a gsutil command, suppressing output, and setting retry/sleep.

    Args:
      gsutil_cmd: The (mostly) constructed gsutil subcommand to run.
      headers: A list of raw headers to pass down.
      parallel: Whether gsutil should enable parallel copy/update of multiple
        files.
      retries: How many times to retry this command (defaults to setting given
        at object creation).
      version: If given, the generation; essentially the timestamp of the last
        update.  Note this is not the same as sequence-number; it's
        monotonically increasing bucket wide rather than reset per file.
        The usage of this is if we intend to replace/update only if the version
        is what we expect.  This is useful for distributed reasons- for example,
        to ensure you don't overwrite someone else's creation, a version of
        0 states "only update if no version exists".

    Returns:
      A RunCommandResult object.
    """
    kwargs = kwargs.copy()
    kwargs.setdefault('redirect_stderr', True)

    cmd = [self.gsutil_bin]
    cmd += self.gsutil_flags
    for header in headers:
      cmd += ['-h', header]
    if version is not None:
      cmd += ['-h', 'x-goog-if-generation-match:%d' % int(version)]

    # Enable parallel copy/update of multiple files if stdin is not to
    # be piped to the command. This does not split a single file into
    # smaller components for upload.
    if parallel and kwargs.get('input') is None:
      cmd += ['-m']

    cmd.extend(gsutil_cmd)

    if retries is None:
      retries = self.retries

    extra_env = kwargs.pop('extra_env', {})
    extra_env.setdefault('BOTO_CONFIG', self.boto_file)

    if self.dry_run:
      logging.debug("%s: would've run: %s", self.__class__.__name__,
                    cros_build_lib.CmdToStr(cmd))
    else:
      try:
        return retry_util.GenericRetry(self._RetryFilter,
                                       retries, cros_build_lib.RunCommand,
                                       cmd, sleep=self._sleep_time,
                                       extra_env=extra_env, **kwargs)
      except cros_build_lib.RunCommandError as e:
        raise GSCommandError(e.msg, e.result, e.exception)

  def Copy(self, src_path, dest_path, acl=None, recursive=False,
           skip_symlinks=True, **kwargs):
    """Copy to/from GS bucket.

    Canned ACL permissions can be specified on the gsutil cp command line.

    More info:
    https://developers.google.com/storage/docs/accesscontrol#applyacls

    Args:
      src_path: Fully qualified local path or full gs:// path of the src file.
      dest_path: Fully qualified local path or full gs:// path of the dest
                 file.
      acl: One of the google storage canned_acls to apply.
      recursive: Whether to copy recursively.
      skip_symlinks: Skip symbolic links when copying recursively.

    Returns:
      Return the CommandResult from the run.

    Raises:
      RunCommandError if the command failed despite retries.
    """
    cmd = ['cp']
    if recursive:
      cmd.append('-r')
      if skip_symlinks:
        cmd.append('-e')

    acl = self.acl if acl is None else acl
    if acl is not None:
      cmd += ['-a', acl]

    cmd += ['--', src_path, dest_path]

    if not (src_path.startswith(BASE_GS_URL) or
            dest_path.startswith(BASE_GS_URL)):
      # Don't retry on local copies.
      kwargs.setdefault('retries', 0)

    return self.DoCommand(cmd, **kwargs)

  # TODO(mtennant): Merge with LS() after it supports returning details.
  def LSWithDetails(self, path, **kwargs):
    """Does a detailed directory listing of the given gs path.

    Args:
      path: The path to get a listing of.

    Returns:
      List of tuples, where each tuple is (gs path, file size in bytes integer,
        file modified time as datetime.datetime object).
    """
    kwargs['redirect_stdout'] = True
    result = self.DoCommand(['ls', '-l', '--', path], **kwargs)

    lines = result.output.splitlines()

    # Output like the followig is expected:
    #    99908  2014-03-01T05:50:08Z  gs://somebucket/foo/abc
    #    99908  2014-03-04T01:16:55Z  gs://somebucket/foo/def
    # TOTAL: 2 objects, 199816 bytes (495.36 KB)

    # The last line is expected to be a summary line.  Ignore it.
    url_tuples = []
    for line in lines[:-1]:
      match = LS_LA_RE.search(line)
      size, timestamp, url = (match.group(1), match.group(2), match.group(3))
      if timestamp:
        timestamp = datetime.datetime.strptime(timestamp, DATETIME_FORMAT)
      else:
        timestamp = None
      size = int(size) if size else None
      url_tuples.append((url, size, timestamp))

    return url_tuples

  # TODO(mtennant): Enhance to add details to returned results, such as
  # size, modified time, generation.
  def LS(self, path, raw=False, **kwargs):
    """Does a directory listing of the given gs path.

    Args:
      path: The path to get a listing of.
      raw: Return the raw CommandResult object instead of parsing it.
      kwargs: See options that DoCommand takes.

    Returns:
      If raw is False, a list of paths that matched |path|.  Might be more
      than one if a directory or path include wildcards/etc...
      If raw is True, then the CommandResult object.
    """
    kwargs['redirect_stdout'] = True
    if not path.startswith(BASE_GS_URL):
      # gsutil doesn't support listing a local path, so just run 'ls'.
      kwargs.pop('retries', None)
      kwargs.pop('headers', None)
      result = cros_build_lib.RunCommand(['ls', path], **kwargs)
    else:
      result = self.DoCommand(['ls', '--', path], **kwargs)

    if raw:
      return result
    else:
      # TODO: Process resulting lines when given -l/-a.
      # See http://crbug.com/342918 for more details.
      return result.output.splitlines()

  def DU(self, path, **kwargs):
    """Returns size of an object."""
    return self.DoCommand(['du', path], redirect_stdout=True, **kwargs)

  def SetACL(self, upload_url, acl=None):
    """Set access on a file already in google storage.

    Args:
      upload_url: gs:// url that will have acl applied to it.
      acl: An ACL permissions file or canned ACL.
    """
    if acl is None:
      if not self.acl:
        raise GSContextException(
            "SetAcl invoked w/out a specified acl, nor a default acl.")
      acl = self.acl

    self.DoCommand(['acl', 'set', acl, upload_url])

  def Exists(self, path, **kwargs):
    """Checks whether the given object exists.

    Args:
      path: Full gs:// url of the path to check.

    Returns:
      True if the path exists; otherwise returns False.
    """
    try:
      # Use 'gsutil stat' command to check for existence.  It is not
      # subject to caching behavior of 'gsutil ls', and it only requires
      # read access to the file, unlike 'gsutil acl get'.
      self.DoCommand(['stat', path], redirect_stdout=True, **kwargs)
    except GSNoSuchKey:
      # A path that does not exist will result in error output like:
      # InvalidUriError: Attempt to get key for "gs://foo/bar"
      # That will result in GSNoSuchKey.
      return False
    return True

  def Remove(self, path, ignore_missing=False):
    """Remove the specified file.

    Args:
      path: Full gs:// url of the file to delete.
      ignore_missing: Whether to suppress errors about missing files.
    """
    try:
      self.DoCommand(['rm', path])
    except GSNoSuchKey:
      if not ignore_missing:
        raise

  def GetGeneration(self, path):
    """Get the generation and metageneration of the given |path|.

    Returns:
      A tuple of the generation and metageneration.
    """
    def _Header(name):
      if res and res.returncode == 0 and res.output is not None:
        # Search for a header that looks like this:
        # header: x-goog-generation: 1378856506589000
        m = re.search(r'header: %s: (\d+)' % name, res.output)
        if m:
          return int(m.group(1))
      return 0

    try:
      res = self.DoCommand(['-d', 'acl', 'get', path],
                           error_code_ok=True, redirect_stdout=True)
    except GSNoSuchKey:
      # If a DoCommand throws an error, 'res' will be None, so _Header(...)
      # will return 0 in both of the cases below.
      pass

    return (_Header('x-goog-generation'), _Header('x-goog-metageneration'))

  def Counter(self, path):
    """Return a GSCounter object pointing at a |path| in Google Storage.

    Args:
      path: The path to the counter in Google Storage.
    """
    return GSCounter(self, path)

  def WaitForGsPaths(self, paths, timeout, period=10):
    """Wait until a list of files exist in GS.

    Args:
      paths: The list of files to wait for.
      timeout: Max seconds to wait for file to appear.
      period: How often to check for files while waiting.

    Raises:
      timeout_util.TimeoutError if the timeout is reached.
    """
    # Copy the list of URIs to wait for, so we don't modify the callers context.
    pending_paths = paths[:]

    def _CheckForExistence():
      pending_paths[:] = [x for x in pending_paths if not self.Exists(x)]

    def _Retry(_return_value):
      # Retry, if there are any pending paths left.
      return pending_paths

    timeout_util.WaitForSuccess(_Retry, _CheckForExistence,
                                timeout=timeout, period=period)


@contextlib.contextmanager
def TemporaryURL(prefix):
  """Context manager to generate a random URL.

  At the end, the URL will be deleted.
  """
  url = '%s/chromite-temp/%s/%s/%s' % (constants.TRASH_BUCKET, prefix,
                                       getpass.getuser(), uuid.uuid1())
  ctx = GSContext()
  ctx.Remove(url, ignore_missing=True)
  try:
    yield url
  finally:
    ctx.Remove(url, ignore_missing=True)
