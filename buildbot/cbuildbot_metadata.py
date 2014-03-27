# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing class for recording metadata about a run."""

import collections
import datetime
import functools
import json
import logging
import math
import os
import re
import time

from chromite.buildbot import cbuildbot_archive
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import parallel
from chromite.lib import toolchain

# Number of parallel processes used when uploading/downloading GS files.
MAX_PARALLEL = 40

ARCHIVE_ROOT = 'gs://chromeos-image-archive/%(target)s'
METADATA_URL_GLOB = os.path.join(ARCHIVE_ROOT, 'R%(milestone)s**metadata.json')
LATEST_URL = os.path.join(ARCHIVE_ROOT, 'LATEST-master')


class CBuildbotMetadata(object):
  """Class for recording metadata about a run."""

  def __init__(self, metadata_dict=None, multiprocess_manager=None):
    """Constructor for CBuildbotMetadata.

    Args:
      metadata_dict: Optional dictionary containing initial metadata,
                     as returned by loading metadata from json.
      multiprocess_manager: Optional multiprocess.Manager instance. If
                            supplied, the metadata instance will use
                            multiprocess containers so that its state
                            is correctly synced across processes.
    """
    super(CBuildbotMetadata, self).__init__()
    if multiprocess_manager:
      self._metadata_dict = multiprocess_manager.dict()
      self._cl_action_list = multiprocess_manager.list()
    else:
      self._metadata_dict = {}
      self._cl_action_list = []

    if metadata_dict:
      self.UpdateWithDict(metadata_dict)

  def UpdateWithDict(self, metadata_dict):
    """Update metadata dictionary with values supplied in |metadata_dict|

    This method is effectively the inverse of GetDict. Existing key-values
    in metadata will be overwritten by those supplied in |metadata_dict|,
    with the exception of the cl_actions list which will be extended with
    the contents (if any) of the supplied dict's cl_actions list.

    Args:
      metadata_dict: A dictionary of key-value pairs to be added this
                     metadata instance. Keys should be strings, values
                     should be json-able.

    Returns:
      self
    """
    # This is effectively the inverse of the dictionary construction in GetDict,
    # to reconstruct the correct internal representation of a metadata
    # object.
    metadata_dict = metadata_dict.copy()
    cl_action_list = metadata_dict.pop('cl_actions', None)
    self._metadata_dict.update(metadata_dict)
    if cl_action_list:
      self._cl_action_list.extend(cl_action_list)

    return self

  def GetDict(self):
    """Returns a dictionary representation of metadata."""
    # CL actions are be stored in self._cl_action_list instead of
    # in self._metadata_dict['cl_actions'], because _cl_action_list
    # is potentially a multiprocess.lis. So, _cl_action_list needs to
    # be copied into a normal list.
    temp = self._metadata_dict.copy()
    temp['cl_actions'] = list(self._cl_action_list)
    return temp

  def GetJSON(self):
    """Return a JSON string representation of metadata."""
    return json.dumps(self.GetDict())

  def RecordCLAction(self, change, action, timestamp=None, reason=''):
    """Record an action that was taken on a CL, to the metadata.

    Args:
      change: A GerritPatch object for the change acted on.
      action: The action taken, should be one of constants.CL_ACTIONS
      timestamp: An integer timestamp such as int(time.time()) at which
                 the action was taken. Default: Now.
      reason: Description of the reason the action was taken. Default: ''

    Returns:
      self
    """
    cl_action = (self._ChangeAsSmallDictionary(change),
                 action,
                 timestamp or int(time.time()),
                 reason or '')

    self._cl_action_list.append(cl_action)
    return self

  @staticmethod
  def _ChangeAsSmallDictionary(change):
    """Returns a small dictionary representation of a gerrit change.

    Args:
      change: A GerritPatch or GerritPatchTuple object.

    Returns:
      A dictionary of the form {'gerrit_number': change.gerrit_number,
                                'patch_number': change.patch_number,
                                'internal': change.internal}
    """
    return  {'gerrit_number': change.gerrit_number,
             'patch_number': change.patch_number,
             'internal': change.internal}

  @staticmethod
  def GetMetadataDict(builder_run, build_root, get_changes_from_pool,
                      get_statuses_from_slaves, config=None, stage=None,
                      final_status=None, sync_instance=None,
                      completion_instance=None):
    """Return a metadata dictionary summarizing a build.

    This method replaces code that used to exist in the ArchivingStageMixin
    class from cbuildbot_stage. It contains all the Archive-stage-time
    metadata construction logic. The logic here is intended to be gradually
    refactored out so that the metadata is constructed gradually by the
    stages that are responsible for pieces of data, as they run.

    Args:
      builder_run: BuilderRun instance for this run.
      build_root: Path to build root for this run.
      get_changes_from_pool: If True, information about patches in the
                             sync_instance.pool will be recorded.
      get_statuses_from_slaves: If True, status information of slave
                                builders will be recorded.
      config: The build config for this run.  Defaults to self._run.config.
      stage: The stage name that this metadata file is being uploaded for.
      final_status: Whether the build passed or failed. If None, the build
                    will be treated as still running.
      sync_instance: The stage instance that was used for syncing the source
                     code. This should be a derivative of SyncStage. If None,
                     the list of commit queue patches will not be included
                     in the metadata.
      completion_instance: The stage instance that was used to wait for slave
                           completion. Used to add slave build information to
                           master builder's metadata. If None, no such status
                           information will be included. It not None, this
                           should be a derivative of
                           MasterSlaveSyncCompletionStage.

    Returns:
       A metadata dictionary suitable to be json-serialized.
    """
    config = config or builder_run.config

    start_time = results_lib.Results.start_time
    current_time = datetime.datetime.now()
    start_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=start_time)
    current_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=current_time)
    duration = '%s' % (current_time - start_time,)

    sdk_verinfo = cros_build_lib.LoadKeyValueFile(
        os.path.join(build_root, constants.SDK_VERSION_FILE),
        ignore_missing=True)
    verinfo = builder_run.GetVersionInfo(build_root)
    platform_tag = getattr(builder_run.attrs, 'release_tag')
    if not platform_tag:
      platform_tag = verinfo.VersionString()
    metadata = {
        # Version of the metadata format.
        'metadata-version': '2',
        # Data for this build.
        'bot-config': config['name'],
        'bot-hostname': cros_build_lib.GetHostName(fully_qualified=True),
        'boards': config['boards'],
        'build-number': builder_run.buildnumber,
        'builder-name': os.environ.get('BUILDBOT_BUILDERNAME', ''),
        'status': {
            'current-time': current_time_stamp,
            'status': final_status if final_status else 'running',
            'summary': stage or '',
        },
        'time': {
            'start': start_time_stamp,
            'finish': current_time_stamp if final_status else '',
            'duration': duration,
        },
        'version': {
            'chrome': builder_run.attrs.chrome_version,
            'full': builder_run.GetVersion(),
            'milestone': verinfo.chrome_branch,
            'platform': platform_tag,
        },
        # Data for the toolchain used.
        'sdk-version': sdk_verinfo.get('SDK_LATEST_VERSION', '<unknown>'),
        'toolchain-url': sdk_verinfo.get('TC_PATH', '<unknown>'),
    }
    if len(config['boards']) == 1:
      toolchains = toolchain.GetToolchainsForBoard(config['boards'][0],
                                                   buildroot=build_root)
      metadata['toolchain-tuple'] = (
          toolchain.FilterToolchains(toolchains, 'default', True).keys() +
          toolchain.FilterToolchains(toolchains, 'default', False).keys())

    metadata['results'] = []
    for entry in results_lib.Results.Get():
      timestr = datetime.timedelta(seconds=math.ceil(entry.time))
      if entry.result in results_lib.Results.NON_FAILURE_TYPES:
        status = constants.FINAL_STATUS_PASSED
      else:
        status = constants.FINAL_STATUS_FAILED
      metadata['results'].append({
          'name': entry.name,
          'status': status,
          # The result might be a custom exception.
          'summary': str(entry.result),
          'duration': '%s' % timestr,
          'board': entry.board,
          'description': entry.description,
          'log': builder_run.ConstructDashboardURL(stage=entry.name),
      })

    if get_changes_from_pool:
      changes = []
      pool = sync_instance.pool
      for change in pool.changes:
        details = {'gerrit_number': change.gerrit_number,
                   'patch_number': change.patch_number,
                   'internal': change.internal}
        for latest_patchset_only in (False, True):
          prefix = '' if latest_patchset_only else 'total_'
          for status in (pool.STATUS_FAILED, pool.STATUS_PASSED):
            count = pool.GetCLStatusCount(pool.bot, change, status,
                                          latest_patchset_only)
            details['%s%s' % (prefix, status.lower())] = count
        changes.append(details)
      metadata['changes'] = changes

    # If we were a CQ master, then include a summary of the status of slave cq
    # builders in metadata
    if get_statuses_from_slaves:
      statuses = completion_instance.GetSlaveStatuses()
      if not statuses:
        logging.warning('completion_instance did not have any statuses '
                        'to report. Will not add slave status to metadata.')

      metadata['slave_targets'] = {}
      for builder, status in statuses.iteritems():
        metadata['slave_targets'][builder] = status.AsFlatDict()

    return metadata


# The graphite graphs use seconds since epoch start as time value.
EPOCH_START = datetime.datetime(1970, 1, 1)

# Formats we like for output.
NICE_DATE_FORMAT = '%Y/%m/%d'
NICE_TIME_FORMAT = '%H:%M:%S'
NICE_DATETIME_FORMAT = NICE_DATE_FORMAT + ' ' + NICE_TIME_FORMAT


# TODO(akeshet): Merge this class into CBuildbotMetadata.
class BuildData(object):
  """Class for examining metadata from a prior run.

  The raw metadata dict can be accessed at self.metadata_dict or via []
  and get() on a BuildData object.  Some values from metadata_dict are
  also surfaced through the following list of supported properties:

  build_number
  stages
  slaves
  chromeos_version
  chrome_version
  bot_id
  status
  start_datetime
  finish_datetime
  start_date_str
  start_time_str
  start_datetime_str
  finish_date_str
  finish_time_str
  finish_datetime_str
  runtime_seconds
  runtime_minutes
  epoch_time_seconds
  count_changes
  run_date
  """

  __slots__ = (
      'gathered_dict',  # Dict with gathered data (sheets/carbon version).
      'gathered_url',   # URL to metadata.json.gathered location in GS.
      'metadata_dict',  # Dict representing metadata data from JSON.
      'metadata_url',   # URL to metadata.json location in GS.
  )

  # Regexp for parsing datetimes as stored in metadata.json.  Example text:
  # Fri, 14 Feb 2014 17:00:49 -0800 (PST)
  DATETIME_RE = re.compile(r'^(.+)\s-\d\d\d\d\s\(P\wT\)$')

  SHEETS_VER_KEY = 'sheets_version'
  CARBON_VER_KEY = 'carbon_version'

  @staticmethod
  def ReadMetadataURLs(urls, gs_ctx=None, exclude_running=True):
    """Read a list of metadata.json URLs and return BuildData objects.

    Args:
      urls: List of metadata.json GS URLs.
      gs_ctx: A GSContext object to use.  If not provided gs.GSContext will
        be called to get a GSContext to use.
      exclude_running: If True the metadata for builds that are still running
        will be skipped.

    Returns:
      List of BuildData objects.
    """
    gs_ctx = gs_ctx or gs.GSContext()
    cros_build_lib.Info('Reading %d metadata URLs using %d processes now.',
                        len(urls), MAX_PARALLEL)

    def _ReadMetadataURL(url):
      # Read the metadata.json URL and parse json into a dict.
      metadata_dict = json.loads(gs_ctx.Cat(url, print_cmd=False).output)

      # Read the file next to url which indicates whether the metadata has
      # been gathered before, and with what stats version.
      gathered_dict = {}
      gathered_url = url + '.gathered'
      if gs_ctx.Exists(gathered_url, print_cmd=False):
        gathered_dict = json.loads(gs_ctx.Cat(gathered_url,
                                              print_cmd=False).output)

      sheets_version = gathered_dict.get(BuildData.SHEETS_VER_KEY)
      carbon_version = gathered_dict.get(BuildData.CARBON_VER_KEY)

      bd = BuildData(url, metadata_dict, sheets_version=sheets_version,
                     carbon_version=carbon_version)
      if not (sheets_version is None and carbon_version is None):
        cros_build_lib.Debug('Read %s:\n'
                             '  build_number=%d, sheets v%d, carbon v%d', url,
                             bd.build_number, sheets_version, carbon_version)
      else:
        cros_build_lib.Debug('Read %s:\n  build_number=%d, ungathered',
                             url, bd.build_number)

      return bd

    steps = [functools.partial(_ReadMetadataURL, url) for url in urls]
    builds = parallel.RunParallelSteps(steps, max_parallel=MAX_PARALLEL,
                                       return_values=True)

    if exclude_running:
      builds = [b for b in builds if b.status != 'running']

    return builds

  @staticmethod
  def MarkBuildsGathered(builds, sheets_version, carbon_version, gs_ctx=None):
    """Mark specified |builds| as processed for the given stats versions.

    Args:
      builds: List of BuildData objects.
      sheets_version: The Google Sheets version these builds are now processed
        for.
      carbon_version: The Carbon/Graphite version these builds are now
        processed for.
      gs_ctx: A GSContext object to use, if set.
    """
    gs_ctx = gs_ctx or gs.GSContext()

    # Filter for builds that were not already on these versions.
    builds = [b for b in builds
              if b.sheets_version != sheets_version or
              b.carbon_version != carbon_version]
    if builds:
      log_ver_str = 'Sheets v%d, Carbon v%d' % (sheets_version, carbon_version)
      cros_build_lib.Info('Marking %d builds gathered (for %s) using %d'
                          ' processes now.', len(builds), log_ver_str,
                          MAX_PARALLEL)

      def _MarkGathered(build):
        build.MarkGathered(sheets_version, carbon_version)
        json_text = json.dumps(build.gathered_dict.copy())
        gs_ctx.Copy('-', build.gathered_url, input=json_text, print_cmd=False)
        cros_build_lib.Debug('Marked build_number %d processed for %s.',
                             build.build_number, log_ver_str)

      steps = [functools.partial(_MarkGathered, build) for build in builds]
      parallel.RunParallelSteps(steps, max_parallel=MAX_PARALLEL)

  def __init__(self, metadata_url, metadata_dict, carbon_version=None,
               sheets_version=None):
    self.metadata_url = metadata_url
    self.metadata_dict = metadata_dict

    # If a stats version is not specified default to -1 so that the initial
    # version (version 0) will be considered "newer".
    self.gathered_url = metadata_url + '.gathered'
    self.gathered_dict = {
        self.CARBON_VER_KEY: -1 if carbon_version is None else carbon_version,
        self.SHEETS_VER_KEY: -1 if sheets_version is None else sheets_version,
    }

  def MarkGathered(self, sheets_version, carbon_version):
    """Mark this build as processed for the given stats versions."""
    self.gathered_dict[self.SHEETS_VER_KEY] = sheets_version
    self.gathered_dict[self.CARBON_VER_KEY] = carbon_version

  def __getitem__(self, key):
    """Relay dict-like access to self.metadata_dict."""
    return self.metadata_dict[key]

  def get(self, key, default=None):
    """Relay dict-like access to self.metadata_dict."""
    return self.metadata_dict.get(key, default)

  @property
  def sheets_version(self):
    return self.gathered_dict[self.SHEETS_VER_KEY]

  @property
  def carbon_version(self):
    return self.gathered_dict[self.CARBON_VER_KEY]

  @property
  def build_number(self):
    return int(self['build-number'])

  @property
  def stages(self):
    return self['results']

  @property
  def slaves(self):
    return self.get('slave_targets', [])

  @property
  def chromeos_version(self):
    return self['version']['full']

  @property
  def chrome_version(self):
    return self['version']['chrome']

  @property
  def bot_id(self):
    return self['bot-config']

  @property
  def status(self):
    return self['status']['status']

  @classmethod
  def _ToDatetime(cls, time_str):
    match = cls.DATETIME_RE.search(time_str)
    if match:
      return datetime.datetime.strptime(match.group(1), '%a, %d %b %Y %H:%M:%S')
    else:
      raise ValueError('Unexpected metadata datetime format: %s' % time_str)

  @property
  def start_datetime(self):
    return self._ToDatetime(self['time']['start'])

  @property
  def finish_datetime(self):
    return self._ToDatetime(self['time']['finish'])

  @property
  def start_date_str(self):
    return self.start_datetime.strftime(NICE_DATE_FORMAT)

  @property
  def start_time_str(self):
    return self.start_datetime.strftime(NICE_TIME_FORMAT)

  @property
  def start_datetime_str(self):
    return self.start_datetime.strftime(NICE_DATETIME_FORMAT)

  @property
  def finish_date_str(self):
    return self.finish_datetime.strftime(NICE_DATE_FORMAT)

  @property
  def finish_time_str(self):
    return self.finish_datetime.strftime(NICE_TIME_FORMAT)

  @property
  def finish_datetime_str(self):
    return self.finish_datetime.strftime(NICE_DATETIME_FORMAT)

  def GetChangelistsStr(self):
    cl_strs = []
    for cl_dict in self.metadata_dict['changes']:
      cl_strs.append('%s%s:%s' %
                     ('*' if cl_dict['internal'] == 'true' else '',
                      cl_dict['gerrit_number'], cl_dict['patch_number']))

    return ' '.join(cl_strs)

  def GetFailedStages(self, with_urls=False):
    """Get names of all failed stages, optionally with URLs for each.

    Args:
      with_urls: If True then also return URLs.  See Returns.

    Returns:
      If with_urls is False, return list of stage names.  Otherwise, return list
        of tuples (stage name, stage URL).
    """
    def _Failed(stage):
      # This can be more discerning in the future, such as for optional stages.
      return stage['status'] == 'failed'

    if with_urls:
      # The "log" url includes "/logs/stdio" on the end.  Strip that off.
      return [(s['name'], os.path.dirname(os.path.dirname(s['log'])))
              for s in self.stages if _Failed(s)]
    else:
      return [s['name'] for s in self.stages if _Failed(s)]

  def GetFailedSlaves(self, with_urls=False):
    def _Failed(slave):
      return slave['status'] == 'fail'

    # Older metadata has no slave_targets entry.
    slaves = self.slaves
    if with_urls:
      return [(name, slave['dashboard_url'])
              for name, slave in slaves.iteritems() if _Failed(slave)]
    else:
      return [name for name, slave in slaves.iteritems() if _Failed(slave)]

    return []

  @property
  def runtime_seconds(self):
    return (self.finish_datetime - self.start_datetime).seconds

  @property
  def runtime_minutes(self):
    return self.runtime_seconds / 60

  @property
  def epoch_time_seconds(self):
    # End time seconds since 1/1/1970, for some reason.
    return int((self.finish_datetime - EPOCH_START).total_seconds())

  @property
  def count_changes(self):
    if not self.metadata_dict.get('changes', None):
      return 0

    return len(self.metadata_dict['changes'])

  @property
  def run_date(self):
    return self.finish_datetime.strftime('%d.%m.%Y')

  def Passed(self):
    """Return True if this represents a successful run."""
    return 'passed' == self.metadata_dict['status']['status'].strip()



def FindLatestFullVersion(builder, version):
  """Find the latest full version number built by |builder| on |version|.

  Args:
    builder: Builder to load information from. E.g. daisy-release
    version: Version that we are interested in. E.g. 5602.0.0

  Returns:
    The latest corresponding full version number, including milestone prefix.
    E.g. R35-5602.0.0. For some builders, this may also include a -rcN or
    -bNNNN suffix.
  """
  gs_ctx = gs.GSContext()
  config = cbuildbot_config.config[builder]
  base_url = cbuildbot_archive.GetBaseUploadURI(config)
  latest_file_url = os.path.join(base_url, 'LATEST-%s' % version)
  try:
    return gs_ctx.Cat(latest_file_url).output.strip()
  except gs.GSNoSuchKey:
    return None


def GetBuildMetadata(builder, full_version):
  """Fetch the metadata.json object for |builder| and |full_version|.

  Args:
    builder: Builder to load information from. E.g. daisy-release
    full_version: Version that we are interested in, including milestone
        prefix. E.g. R35-5602.0.0. For some builders, this may also include a
        -rcN or -bNNNN suffix.

  Returns:
    A newly created CBuildbotMetadata object with the metadata from the given
    |builder| and |full_version|.
  """
  gs_ctx = gs.GSContext()
  config = cbuildbot_config.config[builder]
  base_url = cbuildbot_archive.GetBaseUploadURI(config)
  try:
    archive_url = os.path.join(base_url, full_version)
    metadata_url = os.path.join(archive_url, constants.METADATA_JSON)
    output = gs_ctx.Cat(metadata_url).output
    return CBuildbotMetadata(json.loads(output))
  except gs.GSNoSuchKey:
    return None


class MetadataException(Exception):
  """Base exception class for exceptions in this module."""


class GetMilestoneError(MetadataException):
  """Base exception class for exceptions in this module."""


def GetLatestMilestone():
  """Get the latest milestone from CQ Master LATEST-master file."""
  # Use CQ Master target to get latest milestone.
  latest_url = LATEST_URL % {'target': constants.CQ_MASTER}
  gs_ctx = gs.GSContext()

  cros_build_lib.Info('Getting latest milestone from %s', latest_url)
  try:
    content = gs_ctx.Cat(latest_url).output.strip()

    # Expected syntax is like the following: "R35-1234.5.6-rc7".
    assert content.startswith('R')
    milestone = content.split('-')[0][1:]
    cros_build_lib.Info('Latest milestone determined to be: %s', milestone)
    return int(milestone)

  except gs.GSNoSuchKey:
    raise GetMilestoneError('LATEST file missing: %s' % latest_url)


def GetMetadataURLsSince(target, start_date):
  """Get metadata.json URLs for |target| since |start_date|.

  The modified time of the GS files is used to compare with start_date, so
  the completion date of the builder run is what is important here.

  Args:
    target: Builder target name.
    start_date: datetime.date object.

  Returns:
    Metadata urls for runs found.
  """
  urls = []
  milestone = GetLatestMilestone()
  gs_ctx = gs.GSContext()
  while True:
    base_url = METADATA_URL_GLOB % {'target': target, 'milestone': milestone}
    cros_build_lib.Info('Getting %s builds for R%d from "%s"',
                        target, milestone, base_url)

    try:
      # Get GS URLs as tuples (url, size, modified datetime).  We want the
      # datetimes to quickly know when we are done collecting URLs.
      url_details = gs_ctx.LSWithDetails(base_url)
    except gs.GSNoSuchKey:
      # We ran out of metadata to collect.  Stop searching back in time.
      cros_build_lib.Info('No %s builds found for $%d.  I will not continue'
                          ' search to older milestones.', target, milestone)
      break

    # Sort by timestamp.
    url_details = sorted(url_details, key=lambda x: x[2], reverse=True)

    # See if we have gone far enough back by checking datetime of oldest URL
    # in the current batch.
    if url_details[-1][2].date() < start_date:
      # We want a subset of these URLs, then we are done.
      urls.extend([url for (url, _size, dt) in url_details
                   if dt.date() >= start_date])
      break

    else:
      # Accept all these URLs, then continue on to the next milestone.
      urls.extend([url for (url, _size, _dt) in url_details])
      milestone -= 1
      cros_build_lib.Info('Continuing on to R%d.', milestone)

  return urls


GerritPatchTuple = collections.namedtuple('GerritPatchTuple',
                                          'gerrit_number patch_number internal')
GerritChangeTuple = collections.namedtuple('GerritChangeTuple',
                                           'gerrit_number internal')
CLActionTuple = collections.namedtuple('CLActionTuple',
                                       'change action timestamp reason')
CLActionWithBuildTuple = collections.namedtuple('CLActionWithBuildTuple',
                                                'change action timestamp '
                                                'reason build_number')
