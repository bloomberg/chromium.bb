# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library to make common google storage operations more reliable."""

from __future__ import print_function

import collections
import contextlib
import datetime
import errno
import getpass
import hashlib
import os
import re
import tempfile
import urlparse

from chromite.cbuildbot import constants
from chromite.lib import cache
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import retry_stats
from chromite.lib import retry_util
from chromite.lib import timeout_util


PUBLIC_BASE_HTTPS_URL = 'https://commondatastorage.googleapis.com/'
PRIVATE_BASE_HTTPS_URL = 'https://storage.cloud.google.com/'
# TODO(akeshet): this is a workaround for b/27653354. If that is ultimately
# fixed, revisit this workaround.
PRIVATE_BASE_HTTPS_DOWNLOAD_URL = (
    'https://pantheon.corp.google.com/storage/browser/')
BASE_GS_URL = 'gs://'

# Format used by "gsutil ls -l" when reporting modified time.
DATETIME_FORMAT = '%Y-%m-%dT%H:%M:%SZ'

# Regexp for parsing each line of output from "gsutil ls -l".
# This regexp is prepared for the generation and meta_generation values,
# too, even though they are not expected until we use "-a".
#
# A detailed listing looks like:
#    99908  2014-03-01T05:50:08Z  gs://bucket/foo/abc#1234  metageneration=1
#                                 gs://bucket/foo/adir/
#    99908  2014-03-04T01:16:55Z  gs://bucket/foo/def#5678  metageneration=1
# TOTAL: 2 objects, 199816 bytes (495.36 KB)
LS_LA_RE = re.compile(
    r'^\s*(?P<content_length>\d*?)\s+'
    r'(?P<creation_time>\S*?)\s+'
    r'(?P<url>[^#$]+).*?'
    r'('
    r'#(?P<generation>\d+)\s+'
    r'meta_?generation=(?P<metageneration>\d+)'
    r')?\s*$')
LS_RE = re.compile(r'^\s*(?P<content_length>)(?P<creation_time>)(?P<url>.*)'
                   r'(?P<generation>)(?P<metageneration>)\s*$')


def PathIsGs(path):
  """Determine if a path is a Google Storage URI."""
  return path.startswith(BASE_GS_URL)


def CanonicalizeURL(url, strict=False):
  """Convert provided URL to gs:// URL, if it follows a known format.

  Args:
    url: URL to canonicalize.
    strict: Raises exception if URL cannot be canonicalized.
  """
  for prefix in (PUBLIC_BASE_HTTPS_URL, PRIVATE_BASE_HTTPS_URL):
    if url.startswith(prefix):
      return url.replace(prefix, BASE_GS_URL, 1)

  if not PathIsGs(url) and strict:
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


# Detailed results of GSContext.Stat.
#
# The fields directory correspond to gsutil stat results.
#
#  Field name        Type         Example
#   creation_time     datetime     Sat, 23 Aug 2014 06:53:20 GMT
#   content_length    int          74
#   content_type      string       application/octet-stream
#   hash_crc32c       string       BBPMPA==
#   hash_md5          string       ms+qSYvgI9SjXn8tW/5UpQ==
#   etag              string       CNCgocbmqMACEAE=
#   generation        int          1408776800850000
#   metageneration    int          1
#
# Note: We omit a few stat fields as they are not always available, and we
# have no callers that want this currently.
#
#   content_language  string/None  en   # This field may be None.
GSStatResult = collections.namedtuple(
    'GSStatResult',
    ('creation_time', 'content_length', 'content_type', 'hash_crc32c',
     'hash_md5', 'etag', 'generation', 'metageneration'))


# Detailed results of GSContext.List.
GSListResult = collections.namedtuple(
    'GSListResult',
    ('url', 'creation_time', 'content_length', 'generation', 'metageneration'))


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
      return int(self.ctx.Cat(self.path))
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
  DEFAULT_GSUTIL_TRACKER_DIR = os.path.expanduser('~/.gsutil/tracker-files')
  # This is set for ease of testing.
  DEFAULT_GSUTIL_BIN = None
  DEFAULT_GSUTIL_BUILDER_BIN = '/b/build/third_party/gsutil/gsutil'
  # How many times to retry uploads.
  DEFAULT_RETRIES = 3

  # Multiplier for how long to sleep (in seconds) between retries; will delay
  # (1*sleep) the first time, then (2*sleep), continuing via attempt * sleep.
  DEFAULT_SLEEP_TIME = 60

  GSUTIL_VERSION = '4.19'
  GSUTIL_TAR = 'gsutil_%s.tar.gz' % GSUTIL_VERSION
  GSUTIL_URL = (PUBLIC_BASE_HTTPS_URL +
                'chromeos-mirror/gentoo/distfiles/%s' % GSUTIL_TAR)
  GSUTIL_API_SELECTOR = 'JSON'

  RESUMABLE_UPLOAD_ERROR = ('Too many resumable upload attempts failed without '
                            'progress')
  RESUMABLE_DOWNLOAD_ERROR = ('Too many resumable download attempts failed '
                              'without progress')

  @classmethod
  def GetDefaultGSUtilBin(cls, cache_dir=None):
    if cls.DEFAULT_GSUTIL_BIN is None:
      if cache_dir is None:
        cache_dir = path_util.GetCacheDir()
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

    # Increase the number of retries. With 10 retries, Boto will try a total of
    # 11 times and wait up to 2**11 seconds (~30 minutes) in total, not
    # not including the time spent actually uploading or downloading.
    self.gsutil_flags = ['-o', 'Boto:num_retries=10']

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

    # Prefer boto_file if specified, else prefer the env then the default.
    if boto_file is None:
      boto_file = os.environ.get('BOTO_CONFIG')
    if boto_file is None and os.path.isfile(self.DEFAULT_BOTO_FILE):
      # Only set boto file to DEFAULT_BOTO_FILE if it exists.
      boto_file = self.DEFAULT_BOTO_FILE

    self.boto_file = boto_file

    self.acl = acl

    self.dry_run = dry_run
    self.retries = self.DEFAULT_RETRIES if retries is None else int(retries)
    self._sleep_time = self.DEFAULT_SLEEP_TIME if sleep is None else int(sleep)

    if init_boto and not dry_run:
      # We can't really expect gsutil to even be present in dry_run mode.
      self._InitBoto()

  @property
  def gsutil_version(self):
    """Return the version of the gsutil in this context."""
    if not self._gsutil_version:
      if self.dry_run:
        self._gsutil_version = self.GSUTIL_VERSION
      else:
        cmd = ['-q', 'version']

        # gsutil has been known to return version to stderr in the past, so
        # use combine_stdout_stderr=True.
        result = self.DoCommand(cmd, combine_stdout_stderr=True,
                                redirect_stdout=True)

        # Expect output like: 'gsutil version 3.35' or 'gsutil version: 4.5'.
        match = re.search(r'^\s*gsutil\s+version:?\s+([\d.]+)', result.output,
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
    print('Configuring gsutil. **Please use your @google.com account.**')
    try:
      if not self.boto_file:
        self.boto_file = self.DEFAULT_BOTO_FILE
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
    if not PathIsGs(path):
      # gsutil doesn't support cat-ting a local path, so read it ourselves.
      try:
        return osutils.ReadFile(path)
      except Exception as e:
        if getattr(e, 'errno', None) == errno.ENOENT:
          raise GSNoSuchKey('%s: file does not exist' % path)
        else:
          raise GSContextException(str(e))
    elif self.dry_run:
      return ''
    else:
      return self.DoCommand(['cat', path], **kwargs).output

  def CopyInto(self, local_path, remote_dir, filename=None, **kwargs):
    """Upload a local file into a directory in google storage.

    Args:
      local_path: Local file path to copy.
      remote_dir: Full gs:// url of the directory to transfer the file into.
      filename: If given, the filename to place the content at; if not given,
        it's discerned from basename(local_path).
      **kwargs: See Copy() for documentation.

    Returns:
      The generation of the remote file.
    """
    filename = filename if filename is not None else local_path
    # Basename it even if an explicit filename was given; we don't want
    # people using filename as a multi-directory path fragment.
    return self.Copy(local_path,
                     '%s/%s' % (remote_dir, os.path.basename(filename)),
                     **kwargs)

  @staticmethod
  def GetTrackerFilenames(dest_path):
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
          re.sub(r'[/\\]', '_', 'resumable_upload__%s__%s__%s.url' %
                 (bucket_name, object_name, GSContext.GSUTIL_API_SELECTOR)))
    else:
      prefix = 'download'
      filenames.append(
          re.sub(r'[/\\]', '_', 'resumable_download__%s__%s.etag' %
                 (dest.path, GSContext.GSUTIL_API_SELECTOR)))

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
      # gsutil usually prints PreconditionException when a precondition fails.
      # It may also print "ResumableUploadAbortException: 412 Precondition
      # Failed", so the logic needs to be a little more general.
      if 'PreconditionException' in error or '412 Precondition Failed' in error:
        raise GSContextPreconditionFailed(e)

      # If the file does not exist, one of the following errors occurs. The
      # "stat" command leaves off the "CommandException: " prefix, but it also
      # outputs to stdout instead of stderr and so will not be caught here
      # regardless.
      if ('CommandException: No URLs matched' in error or
          'NotFoundException:' in error or
          'One or more URLs matched no objects' in error):
        raise GSNoSuchKey(e)

      logging.warning('GS_ERROR: %s', error)

      # TODO: Below is a list of known flaky errors that we should
      # retry. The list needs to be extended.

      # Temporary fix: remove the gsutil tracker files so that our retry
      # can hit a different backend. This should be removed after the
      # bug is fixed by the Google Storage team (see crbug.com/308300).
      RESUMABLE_ERROR_MESSAGE = (
          self.RESUMABLE_DOWNLOAD_ERROR,
          self.RESUMABLE_UPLOAD_ERROR,
          'ResumableUploadException',
          'ResumableUploadAbortException',
          'ResumableDownloadException',
          'ssl.SSLError: The read operation timed out',
          'Unable to find the server',
          'doesn\'t match cloud-supplied digest',
      )
      if any(x in error for x in RESUMABLE_ERROR_MESSAGE):
        # Only remove the tracker files if we try to upload/download a file.
        if 'cp' in e.result.cmd[:-2]:
          # Assume a command: gsutil [options] cp [options] src_path dest_path
          # dest_path needs to be a fully qualified local path, which is already
          # required for GSContext.Copy().
          tracker_filenames = self.GetTrackerFilenames(e.result.cmd[-1])
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

      # We have seen flaky errors with 5xx return codes
      # See b/17376491 for the "JSON decoding" error.
      # We have seen transient Oauth 2.0 credential errors (crbug.com/414345).
      TRANSIENT_ERROR_MESSAGE = (
          'ServiceException: 5',
          'Failure: No JSON object could be decoded',
          'Oauth 2.0 User Account',
          'InvalidAccessKeyId',
          'socket.error: [Errno 104] Connection reset by peer',
          'Received bad request from server',
      )
      if any(x in error for x in TRANSIENT_ERROR_MESSAGE):
        return True

    return False

  # TODO(mtennant): Make a private method.
  def DoCommand(self, gsutil_cmd, headers=(), retries=None, version=None,
                parallel=False, **kwargs):
    """Run a gsutil command, suppressing output, and setting retry/sleep.

    Args:
      gsutil_cmd: The (mostly) constructed gsutil subcommand to run.
      headers: A list of raw headers to pass down.
      parallel: Whether gsutil should enable parallel copy/update of multiple
        files. NOTE: This option causes gsutil to use significantly more
        memory, even if gsutil is only uploading one file.
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
    if self.boto_file and os.path.isfile(self.boto_file):
      extra_env.setdefault('BOTO_CONFIG', self.boto_file)

    if self.dry_run:
      logging.debug("%s: would've run: %s", self.__class__.__name__,
                    cros_build_lib.CmdToStr(cmd))
    else:
      try:
        return retry_stats.RetryWithStats(retry_stats.GSUTIL,
                                          self._RetryFilter,
                                          retries, cros_build_lib.RunCommand,
                                          cmd, sleep=self._sleep_time,
                                          extra_env=extra_env, **kwargs)
      except cros_build_lib.RunCommandError as e:
        raise GSCommandError(e.msg, e.result, e.exception)

  def Copy(self, src_path, dest_path, acl=None, recursive=False,
           skip_symlinks=True, auto_compress=False, **kwargs):
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
      auto_compress: Automatically compress with gzip when uploading.

    Returns:
      The generation of the remote file.

    Raises:
      RunCommandError if the command failed despite retries.
    """
    # -v causes gs://bucket/path#generation to be listed in output.
    cmd = ['cp', '-v']

    # Certain versions of gsutil (at least 4.3) assume the source of a copy is
    # a directory if the -r option is used. If it's really a file, gsutil will
    # look like it's uploading it but not actually do anything. We'll work
    # around that problem by surpressing the -r flag if we detect the source
    # is a local file.
    if recursive and not os.path.isfile(src_path):
      cmd.append('-r')
      if skip_symlinks:
        cmd.append('-e')

    if auto_compress:
      # Pass the suffix without the '.' as that is what gsutil wants.
      suffix = os.path.splitext(src_path)[1]
      if not suffix:
        raise ValueError('src file "%s" needs an extension to compress' %
                         (src_path,))
      cmd += ['-z', suffix[1:]]

    acl = self.acl if acl is None else acl
    if acl is not None:
      cmd += ['-a', acl]

    with cros_build_lib.ContextManagerStack() as stack:
      # Write the input into a tempfile if possible. This is needed so that
      # gsutil can retry failed requests.
      if src_path == '-' and kwargs.get('input') is not None:
        f = stack.Add(tempfile.NamedTemporaryFile)
        f.write(kwargs['input'])
        f.flush()
        del kwargs['input']
        src_path = f.name

      cmd += ['--', src_path, dest_path]

      if not (PathIsGs(src_path) or PathIsGs(dest_path)):
        # Don't retry on local copies.
        kwargs.setdefault('retries', 0)

      kwargs['capture_output'] = True
      try:
        result = self.DoCommand(cmd, **kwargs)
        if self.dry_run:
          return None

        # Now we parse the output for the current generation number.  Example:
        #   Created: gs://chromeos-throw-away-bucket/foo#1360630664537000.1
        m = re.search(r'Created: .*#(\d+)([.](\d+))?$', result.error)
        if m:
          return int(m.group(1))
        else:
          return None
      except GSNoSuchKey:
        # If the source was a local file, the error is a quirk of gsutil 4.5
        # and should be ignored. If the source was remote, there might
        # legitimately be no such file. See crbug.com/393419.
        if os.path.isfile(src_path):
          return None
        raise

  def CreateWithContents(self, gs_uri, contents, **kwargs):
    """Creates the specified file with specified contents.

    Args:
      gs_uri: The URI of a file on Google Storage.
      contents: String with contents to write to the file.
      kwargs: See additional options that Copy takes.

    Raises:
      See Copy.
    """
    self.Copy('-', gs_uri, input=contents, **kwargs)

  # TODO: Merge LS() and List()?
  def LS(self, path, **kwargs):
    """Does a directory listing of the given gs path.

    Args:
      path: The path to get a listing of.
      kwargs: See options that DoCommand takes.

    Returns:
      A list of paths that matched |path|.  Might be more than one if a
      directory or path include wildcards/etc...
    """
    if self.dry_run:
      return []

    if not PathIsGs(path):
      # gsutil doesn't support listing a local path, so just run 'ls'.
      kwargs.pop('retries', None)
      kwargs.pop('headers', None)
      result = cros_build_lib.RunCommand(['ls', path], **kwargs)
      return result.output.splitlines()
    else:
      return [x.url for x in self.List(path, **kwargs)]

  def List(self, path, details=False, **kwargs):
    """Does a directory listing of the given gs path.

    Args:
      path: The path to get a listing of.
      details: Whether to include size/timestamp info.
      kwargs: See options that DoCommand takes.

    Returns:
      A list of GSListResult objects that matched |path|.  Might be more
      than one if a directory or path include wildcards/etc...
    """
    ret = []
    if self.dry_run:
      return ret

    cmd = ['ls']
    if details:
      cmd += ['-l']
    cmd += ['--', path]

    # We always request the extended details as the overhead compared to a plain
    # listing is negligible.
    kwargs['redirect_stdout'] = True
    lines = self.DoCommand(cmd, **kwargs).output.splitlines()

    if details:
      # The last line is expected to be a summary line.  Ignore it.
      lines = lines[:-1]
      ls_re = LS_LA_RE
    else:
      ls_re = LS_RE

    # Handle optional fields.
    intify = lambda x: int(x) if x else None

    # Parse out each result and build up the results list.
    for line in lines:
      match = ls_re.search(line)
      if not match:
        raise GSContextException('unable to parse line: %s' % line)
      if match.group('creation_time'):
        timestamp = datetime.datetime.strptime(match.group('creation_time'),
                                               DATETIME_FORMAT)
      else:
        timestamp = None

      ret.append(GSListResult(
          content_length=intify(match.group('content_length')),
          creation_time=timestamp,
          url=match.group('url'),
          generation=intify(match.group('generation')),
          metageneration=intify(match.group('metageneration'))))

    return ret

  def GetSize(self, path, **kwargs):
    """Returns size of a single object (local or GS)."""
    if not PathIsGs(path):
      return os.path.getsize(path)
    else:
      return self.Stat(path, **kwargs).content_length

  def Move(self, src_path, dest_path, **kwargs):
    """Move/rename to/from GS bucket.

    Args:
      src_path: Fully qualified local path or full gs:// path of the src file.
      dest_path: Fully qualified local path or full gs:// path of the dest file.
    """
    cmd = ['mv', '--', src_path, dest_path]
    return self.DoCommand(cmd, **kwargs)

  def SetACL(self, upload_url, acl=None):
    """Set access on a file already in google storage.

    Args:
      upload_url: gs:// url that will have acl applied to it.
      acl: An ACL permissions file or canned ACL.
    """
    if acl is None:
      if not self.acl:
        raise GSContextException(
            'SetAcl invoked w/out a specified acl, nor a default acl.')
      acl = self.acl

    self.DoCommand(['acl', 'set', acl, upload_url])

  def ChangeACL(self, upload_url, acl_args_file=None, acl_args=None):
    """Change access on a file already in google storage with "acl ch".

    Args:
      upload_url: gs:// url that will have acl applied to it.
      acl_args_file: A file with arguments to the gsutil acl ch command. The
                     arguments can be spread across multiple lines. Comments
                     start with a # character and extend to the end of the
                     line. Exactly one of this argument or acl_args must be
                     set.
      acl_args: A list of arguments for the gsutil acl ch command. Exactly
                one of this argument or acl_args must be set.
    """
    if acl_args_file and acl_args:
      raise GSContextException(
          'ChangeACL invoked with both acl_args and acl_args set.')
    if not acl_args_file and not acl_args:
      raise GSContextException(
          'ChangeACL invoked with neither acl_args nor acl_args set.')

    if acl_args_file:
      lines = osutils.ReadFile(acl_args_file).splitlines()
      # Strip out comments.
      lines = [x.split('#', 1)[0].strip() for x in lines]
      acl_args = ' '.join([x for x in lines if x]).split()

    self.DoCommand(['acl', 'ch'] + acl_args + [upload_url])

  def Exists(self, path, **kwargs):
    """Checks whether the given object exists.

    Args:
      path: Local path or gs:// url to check.
      kwargs: Flags to pass to DoCommand.

    Returns:
      True if the path exists; otherwise returns False.
    """
    if not PathIsGs(path):
      return os.path.exists(path)

    try:
      self.Stat(path, **kwargs)
    except GSNoSuchKey:
      return False

    return True

  def Remove(self, path, recursive=False, ignore_missing=False, **kwargs):
    """Remove the specified file.

    Args:
      path: Full gs:// url of the file to delete.
      recursive: Remove recursively starting at path.
      ignore_missing: Whether to suppress errors about missing files.
      kwargs: Flags to pass to DoCommand.
    """
    cmd = ['rm']
    if 'recurse' in kwargs:
      raise TypeError('"recurse" has been renamed to "recursive"')
    if recursive:
      cmd.append('-R')
    cmd.append(path)
    try:
      self.DoCommand(cmd, **kwargs)
    except GSNoSuchKey:
      if not ignore_missing:
        raise

  def GetGeneration(self, path):
    """Get the generation and metageneration of the given |path|.

    Returns:
      A tuple of the generation and metageneration.
    """
    try:
      res = self.Stat(path)
    except GSNoSuchKey:
      return 0, 0

    return res.generation, res.metageneration

  def Stat(self, path, **kwargs):
    """Stat a GS file, and get detailed information.

    Args:
      path: A GS path for files to Stat. Wildcards are NOT supported.
      kwargs: Flags to pass to DoCommand.

    Returns:
      A GSStatResult object with all fields populated.

    Raises:
      Assorted GSContextException exceptions.
    """
    try:
      res = self.DoCommand(['stat', '--', path], redirect_stdout=True, **kwargs)
    except GSCommandError as e:
      # Because the 'gsutil stat' command logs errors itself (instead of
      # raising errors internally like other commands), we have to look
      # for errors ourselves.  See the related bug report here:
      # https://github.com/GoogleCloudPlatform/gsutil/issues/288
      # Example line:
      # No URLs matched gs://bucket/file
      if e.result.error.startswith('No URLs matched'):
        raise GSNoSuchKey(path)

      # No idea what this is, so just choke.
      raise

    # In dryrun mode, DoCommand doesn't return an object, so we need to fake
    # out the behavior ourselves.
    if self.dry_run:
      return GSStatResult(
          creation_time=datetime.datetime.now(),
          content_length=0,
          content_type='application/octet-stream',
          hash_crc32c='AAAAAA==',
          hash_md5='',
          etag='',
          generation=0,
          metageneration=0)

    # We expect Stat output like the following. However, the Content-Language
    # line appears to be optional based on how the file in question was
    # created.
    #
    # gs://bucket/path/file:
    #     Creation time:      Sat, 23 Aug 2014 06:53:20 GMT
    #     Content-Language:   en
    #     Content-Length:     74
    #     Content-Type:       application/octet-stream
    #     Hash (crc32c):      BBPMPA==
    #     Hash (md5):         ms+qSYvgI9SjXn8tW/5UpQ==
    #     ETag:               CNCgocbmqMACEAE=
    #     Generation:         1408776800850000
    #     Metageneration:     1

    if not res.output.startswith('gs://'):
      raise GSContextException('Unexpected stat output: %s' % res.output)

    def _GetField(name, optional=False):
      m = re.search(r'%s:\s*(.+)' % re.escape(name), res.output)
      if m:
        return m.group(1)
      elif optional:
        return None
      else:
        raise GSContextException('Field "%s" missing in "%s"' %
                                 (name, res.output))

    return GSStatResult(
        creation_time=datetime.datetime.strptime(
            _GetField('Creation time'), '%a, %d %b %Y %H:%M:%S %Z'),
        content_length=int(_GetField('Content-Length')),
        content_type=_GetField('Content-Type'),
        hash_crc32c=_GetField('Hash (crc32c)'),
        hash_md5=_GetField('Hash (md5)', optional=True),
        etag=_GetField('ETag'),
        generation=int(_GetField('Generation')),
        metageneration=int(_GetField('Metageneration')))

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
                                       getpass.getuser(),
                                       cros_build_lib.GetRandomString())
  ctx = GSContext()
  ctx.Remove(url, ignore_missing=True, recursive=True)
  try:
    yield url
  finally:
    ctx.Remove(url, ignore_missing=True, recursive=True)
