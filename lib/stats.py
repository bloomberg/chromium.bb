# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for uploading command stats to AppEngine."""

from __future__ import print_function

import contextlib
import os
import urllib
import urllib2

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import timeout_util


class Stats(object):
  """Entity object for a stats entry."""

  # These attributes correspond to the fields of a stats record.
  __slots__ = (
      'board',
      'cmd_args',
      'cmd_base',
      'cmd_line',
      'cpu_count',
      'cpu_type',
      'host',
      'package_count',
      'run_time',
      'username',
  )

  def __init__(self, **kwargs):
    """Initialize the record.

    **kwargs keys need to correspond to elements in __slots__.  These arguments
    can be lists:
    - cmd_args
    - cmd_base
    - cmd_line

    If unset, the |username| and |host|  attributes will be determined
    automatically.
    """
    if kwargs.get('username') is None:
      kwargs['username'] = git.GetProjectUserEmail(os.path.dirname(__file__))

    if kwargs.get('host') is None:
      kwargs['host'] = cros_build_lib.GetHostName(fully_qualified=True)

    for attr in ('cmd_args', 'cmd_base', 'cmd_line'):
      val = kwargs.get(attr)
      if isinstance(val, (list, tuple,)):
        kwargs[attr] = ' '.join(map(repr, val))

    for arg in self.__slots__:
      setattr(self, arg, kwargs.pop(arg, None))
    if kwargs:
      raise TypeError('Unknown options specified %r:' % kwargs)

  @property
  def data(self):
    """Retrieves a dictionary representing the fields that are set."""
    data = {}
    for arg in self.__slots__:
      val = getattr(self, arg)
      if val is not None:
        data[arg] = val
    return data

  @classmethod
  def SafeInit(cls, **kwargs):
    """Construct a Stats object, catching any exceptions.

    See Stats.__init__() for argument list.

    Returns:
      A Stats() instance if things went smoothly, and None if exceptions were
      caught in the process.
    """
    try:
      inst = cls(**kwargs)
    except Exception:
      logging.error('Exception during stats upload.', exc_info=True)
    else:
      return inst


class StatsUploader(object):
  """Functionality to upload the stats to the AppEngine server."""

  # To test with an app engine instance on localhost, set envvar
  # export CROS_BUILD_STATS_SITE="http://localhost:8080"
  _PAGE = 'upload_command_stats'
  _DEFAULT_SITE = 'https://chromiumos-build-stats.appspot.com'
  _SITE = os.environ.get('CROS_BUILD_STATS_SITE', _DEFAULT_SITE)
  URL = '%s/%s' % (_SITE, _PAGE)
  UPLOAD_TIMEOUT = 5

  _DISABLE_FILE = '~/.disable_build_stats_upload'

  _DOMAIN_WHITELIST = (constants.CORP_DOMAIN, constants.GOLO_DOMAIN)
  _EMAIL_WHITELIST = (constants.GOOGLE_EMAIL, constants.CHROMIUM_EMAIL)

  TIMEOUT_ERROR = 'Timed out during command stat upload - waited %s seconds'
  ENVIRONMENT_ERROR = 'Exception during command stat upload.'
  HTTPURL_ERROR = 'Exception during command stat upload to %s.'

  @classmethod
  def _UploadConditionsMet(cls, stats):
    """Return True if upload conditions are met."""
    def CheckDomain(hostname):
      return any(hostname.endswith(d) for d in cls._DOMAIN_WHITELIST)

    def CheckEmail(email):
      return any(email.endswith(e) for e in cls._EMAIL_WHITELIST)

    upload = False

    # Verify that host domain is in golo.chromium.org or corp.google.com.
    if not stats.host or not CheckDomain(stats.host):
      logging.debug('Host %s is not a Google machine.', stats.host)
    elif not stats.username:
      logging.debug('Unable to determine current "git id".')
    elif not CheckEmail(stats.username):
      logging.debug('%s is not a Google or Chromium user.', stats.username)
    elif os.path.exists(osutils.ExpandPath(cls._DISABLE_FILE)):
      logging.debug('Found %s', cls._DISABLE_FILE)
    else:
      upload = True

    if not upload:
      logging.debug('Skipping stats upload.')

    return upload

  @classmethod
  def Upload(cls, stats, url=None, timeout=None):
    """Upload |stats| to |url|.

    Does nothing if upload conditions aren't met.

    Args:
      stats: A Stats object to upload.
      url: The url to send the request to.
      timeout: A timeout value to set, in seconds.
    """
    if url is None:
      url = cls.URL
    if timeout is None:
      timeout = cls.UPLOAD_TIMEOUT

    if not cls._UploadConditionsMet(stats):
      return

    with timeout_util.Timeout(timeout):
      try:
        cls._Upload(stats, url)
      # Stats upload errors are silenced, for the sake of user experience.
      except timeout_util.TimeoutError:
        logging.debug(cls.TIMEOUT_ERROR, timeout)
      except urllib2.HTTPError as e:
        # HTTPError has a geturl() method, but it relies on self.url, which
        # is not always set.  In looking at source, self.filename equals url.
        logging.debug(cls.HTTPURL_ERROR, e.filename, exc_info=True)
      except EnvironmentError:
        logging.debug(cls.ENVIRONMENT_ERROR, exc_info=True)

  @classmethod
  def _Upload(cls, stats, url):
    logging.debug('Uploading command stats to %r', url)
    data = urllib.urlencode(stats.data)
    request = urllib2.Request(url)
    urllib2.urlopen(request, data)


UNCAUGHT_UPLOAD_ERROR = 'Uncaught command stats exception'


@contextlib.contextmanager
def UploadContext():
  """Provides a context where stats are uploaded in the background.

  Yields:
    A queue that accepts an arg-list of the format [stats, url, timeout].
  """
  try:
    # We need to use parallel.BackgroundTaskRunner, and not
    # parallel.RunParallelTasks, because with RunParallelTasks, both the
    # uploader and the subcommand are treated as background tasks, and the
    # subcommand will lose responsiveness, since its output will be buffered.
    with parallel.BackgroundTaskRunner(
        StatsUploader.Upload, processes=1) as queue:
      yield queue
  except parallel.BackgroundFailure as e:
    # Display unexpected errors, but don't propagate the error.
    # KeyboardInterrupts are OK to skip since the user initiated it.
    if (e.exc_infos and
        all(exc_info.type == KeyboardInterrupt for exc_info in e.exc_infos)):
      return
    logging.error('Uncaught command stats exception', exc_info=True)
