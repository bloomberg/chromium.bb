#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for uploading command stats to AppEngine."""

import logging
import os
import urllib
import urllib2

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils

# To test with an app engine instance on localhost, set envvar
# export CROS_BUILD_STATS_SITE="http://localhost:8080"

PAGE = 'upload_command_stats'
DEFAULT_SITE = 'https://chromiumos-build-stats.appspot.com'
SITE = os.environ.get('CROS_BUILD_STATS_SITE', DEFAULT_SITE)
URL = '%s/%s' % (SITE, PAGE)
UPLOAD_TIMEOUT = 5


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
    for arg in self.__slots__:
      setattr(self, arg, kwargs.pop(arg, None))
    if kwargs:
      raise TypeError('Unknown options specified %r:' % kwargs)

    # pylint: disable=E0203
    if self.username is None:
      self.username = git.GetProjectUserEmail(os.path.dirname(__file__))

    # pylint: disable=E0203
    if self.host is None:
      self.host = cros_build_lib.GetHostName(fully_qualified=True)

    for arg in ('cmd_args', 'cmd_base', 'cmd_line'):
      val = getattr(self, arg)
      if isinstance(val, (list, tuple,)):
        setattr(self, arg, ' '.join(map(repr, val)))

  @property
  def data(self):
    """Retrieves a dictionary representing the fields that are set."""
    data = {}
    for arg in self.__slots__:
      val = getattr(self, arg)
      if val is not None:
        data[arg] = val
    return data


class StatsUploader(object):
  """Functionality to upload the stats to the AppEngine server."""

  _DISABLE_FILE = '~/.disable_build_stats_upload'

  _DOMAIN_WHITELIST = (constants.CORP_DOMAIN, constants.GOLO_DOMAIN)
  _EMAIL_WHITELIST = (constants.GOOGLE_EMAIL, constants.CHROMIUM_EMAIL)

  TIMEOUT_ERROR = 'Timed out during command stat upload - waited %s seconds'
  ENVIRONMENT_ERROR = 'Exception during command stat upload.'

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
  def Upload(cls, stats, url=URL, timeout=UPLOAD_TIMEOUT):
    """Upload |stats| to |url|.

      Does nothing if upload conditions aren't met.

      Arguments:
        stats: A Stats object to upload.
        url: The url to send the request to.
        timeout: A timeout value to set, in seconds.
    """
    if not cls._UploadConditionsMet(stats):
      return

    with cros_build_lib.SubCommandTimeout(timeout):
      try:
        cls._Upload(stats, url=url)
      # Stats upload errors are silenced, for the sake of user experience.
      except cros_build_lib.TimeoutError:
        logging.debug(cls.TIMEOUT_ERROR, timeout)
      except EnvironmentError:
        logging.debug(cls.ENVIRONMENT_ERROR, exc_info=True)

  @classmethod
  def _Upload(cls, stats, url=URL):
    logging.debug('Uploading command stats to %r', url)
    data = urllib.urlencode(stats.data)
    request = urllib2.Request(url)
    urllib2.urlopen(request, data)
