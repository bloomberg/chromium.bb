#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all the buildbot steps for ChromeDriver except for update/compile."""

import bisect
import csv
import datetime
import glob
import json
import optparse
import os
import platform as platform_module
import re
import shutil
import StringIO
import sys
import tempfile
import time
import urllib2

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
GS_CHROMEDRIVER_BUCKET = 'gs://chromedriver'
GS_CHROMEDRIVER_DATA_BUCKET = 'gs://chromedriver-data'
GS_CHROMEDRIVER_RELEASE_URL = 'http://chromedriver.storage.googleapis.com'
GS_CONTINUOUS_URL = GS_CHROMEDRIVER_DATA_BUCKET + '/continuous'
GS_PREBUILTS_URL = GS_CHROMEDRIVER_DATA_BUCKET + '/prebuilts'
GS_SERVER_LOGS_URL = GS_CHROMEDRIVER_DATA_BUCKET + '/server_logs'
SERVER_LOGS_LINK = (
    'http://chromedriver-data.storage.googleapis.com/server_logs')
TEST_LOG_FORMAT = '%s_log.json'
GS_GIT_LOG_URL = (
    'https://chromium.googlesource.com/chromium/src/+/%s?format=json')
GS_SEARCH_PATTERN = (
    r'Cr-Commit-Position: refs/heads/master@{#(\d+)}')
CR_REV_URL = 'https://cr-rev.appspot.com/_ah/api/crrev/v1/redirect/%s'

SCRIPT_DIR = os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir, os.pardir,
                          os.pardir, os.pardir, os.pardir, 'scripts')
SITE_CONFIG_DIR = os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir,
                               os.pardir, os.pardir, os.pardir, os.pardir,
                               'site_config')
sys.path.append(SCRIPT_DIR)
sys.path.append(SITE_CONFIG_DIR)

import archive
import chrome_paths
from slave import gsutil_download
from slave import slave_utils
import util


def _ArchivePrebuilts(revision):
  """Uploads the prebuilts to google storage."""
  util.MarkBuildStepStart('archive prebuilts')
  zip_path = util.Zip(os.path.join(chrome_paths.GetBuildDir(['chromedriver']),
                                   'chromedriver'))
  if slave_utils.GSUtilCopy(
      zip_path,
      '%s/%s' % (GS_PREBUILTS_URL, 'r%s.zip' % revision)):
    util.MarkBuildStepError()


def _ArchiveServerLogs():
  """Uploads chromedriver server logs to google storage."""
  util.MarkBuildStepStart('archive chromedriver server logs')
  for server_log in glob.glob(os.path.join(tempfile.gettempdir(),
                                           'chromedriver_*')):
    base_name = os.path.basename(server_log)
    util.AddLink(base_name, '%s/%s' % (SERVER_LOGS_LINK, base_name))
    slave_utils.GSUtilCopy(
        server_log,
        '%s/%s' % (GS_SERVER_LOGS_URL, base_name),
        mimetype='text/plain')


def _DownloadPrebuilts():
  """Downloads the most recent prebuilts from google storage."""
  util.MarkBuildStepStart('Download latest chromedriver')

  zip_path = os.path.join(util.MakeTempDir(), 'build.zip')
  if gsutil_download.DownloadLatestFile(GS_PREBUILTS_URL,
                                        GS_PREBUILTS_URL + '/r',
                                        zip_path):
    util.MarkBuildStepError()

  util.Unzip(zip_path, chrome_paths.GetBuildDir(['host_forwarder']))


def _GetTestResultsLog(platform):
  """Gets the test results log for the given platform.

  Args:
    platform: The platform that the test results log is for.

  Returns:
    A dictionary where the keys are SVN revisions and the values are booleans
    indicating whether the tests passed.
  """
  temp_log = tempfile.mkstemp()[1]
  log_name = TEST_LOG_FORMAT % platform
  result = slave_utils.GSUtilDownloadFile(
      '%s/%s' % (GS_CHROMEDRIVER_DATA_BUCKET, log_name), temp_log)
  if result:
    return {}
  with open(temp_log, 'rb') as log_file:
    json_dict = json.load(log_file)
  # Workaround for json encoding dictionary keys as strings.
  return dict([(int(v[0]), v[1]) for v in json_dict.items()])


def _PutTestResultsLog(platform, test_results_log):
  """Pushes the given test results log to google storage."""
  temp_dir = util.MakeTempDir()
  log_name = TEST_LOG_FORMAT % platform
  log_path = os.path.join(temp_dir, log_name)
  with open(log_path, 'wb') as log_file:
    json.dump(test_results_log, log_file)
  if slave_utils.GSUtilCopyFile(log_path, GS_CHROMEDRIVER_DATA_BUCKET):
    raise Exception('Failed to upload test results log to google storage')


def _UpdateTestResultsLog(platform, revision, passed):
  """Updates the test results log for the given platform.

  Args:
    platform: The platform name.
    revision: The SVN revision number.
    passed: Boolean indicating whether the tests passed at this revision.
  """
  assert isinstance(revision, int), 'The revision must be an integer'
  log = _GetTestResultsLog(platform)
  if len(log) > 500:
    del log[min(log.keys())]
  assert revision not in log, 'Results already exist for revision %s' % revision
  log[revision] = bool(passed)
  _PutTestResultsLog(platform, log)


def _GetVersion():
  """Get the current chromedriver version."""
  with open(os.path.join(_THIS_DIR, 'VERSION'), 'r') as f:
    return f.read().strip()


def _GetSupportedChromeVersions():
  """Get the minimum and maximum supported Chrome versions.

  Returns:
    A tuple of the form (min_version, max_version).
  """
  # Minimum supported Chrome version is embedded as:
  # const int kMinimumSupportedChromeVersion[] = {27, 0, 1453, 0};
  with open(os.path.join(_THIS_DIR, 'chrome', 'version.cc'), 'r') as f:
    lines = f.readlines()
    chrome_min_version_line = [
        x for x in lines if 'kMinimumSupportedChromeVersion' in x]
  chrome_min_version = chrome_min_version_line[0].split('{')[1].split(',')[0]
  with open(os.path.join(chrome_paths.GetSrc(), 'chrome', 'VERSION'), 'r') as f:
    chrome_max_version = f.readlines()[0].split('=')[1].strip()
  return (chrome_min_version, chrome_max_version)


def _RevisionState(test_results_log, revision):
  """Check the state of tests at a given SVN revision.

  Considers tests as having passed at a revision if they passed at revisons both
  before and after.

  Args:
    test_results_log: A test results log dictionary from _GetTestResultsLog().
    revision: The revision to check at.

  Returns:
    'passed', 'failed', or 'unknown'
  """
  assert isinstance(revision, int), 'The revision must be an integer'
  keys = sorted(test_results_log.keys())
  # Return passed if the exact revision passed on Android.
  if revision in test_results_log:
    return 'passed' if test_results_log[revision] else 'failed'
  # Tests were not run on this exact revision on Android.
  index = bisect.bisect_right(keys, revision)
  # Tests have not yet run on Android at or above this revision.
  if index == len(test_results_log):
    return 'unknown'
  # No log exists for any prior revision, assume it failed.
  if index == 0:
    return 'failed'
  # Return passed if the revisions on both sides passed.
  if test_results_log[keys[index]] and test_results_log[keys[index - 1]]:
    return 'passed'
  return 'failed'


def _ArchiveGoodBuild(platform, revision):
  """Archive chromedriver binary if the build is green."""
  assert platform != 'android'
  util.MarkBuildStepStart('archive build')

  server_name = 'chromedriver'
  if util.IsWindows():
    server_name += '.exe'
  zip_path = util.Zip(os.path.join(chrome_paths.GetBuildDir([server_name]),
                                   server_name))

  build_name = 'chromedriver_%s_%s.%s.zip' % (
      platform, _GetVersion(), revision)
  build_url = '%s/%s' % (GS_CONTINUOUS_URL, build_name)
  if slave_utils.GSUtilCopy(zip_path, build_url):
    util.MarkBuildStepError()

  (latest_fd, latest_file) = tempfile.mkstemp()
  os.write(latest_fd, build_name)
  os.close(latest_fd)
  latest_url = '%s/latest_%s' % (GS_CONTINUOUS_URL, platform)
  if slave_utils.GSUtilCopy(latest_file, latest_url, mimetype='text/plain'):
    util.MarkBuildStepError()
  os.remove(latest_file)


def _WasReleased(version, platform):
  """Check if the specified version is released for the given platform."""
  result, _ = slave_utils.GSUtilListBucket(
      '%s/%s/chromedriver_%s.zip' % (GS_CHROMEDRIVER_BUCKET, version, platform),
      [])
  return result == 0


def _MaybeRelease(platform):
  """Releases a release candidate if conditions are right."""
  assert platform != 'android'

  version = _GetVersion()

  # Check if the current version has already been released.
  if _WasReleased(version, platform):
    return

  # Fetch Android test results.
  android_test_results = _GetTestResultsLog('android')

  # Fetch release candidates.
  result, output = slave_utils.GSUtilListBucket(
      '%s/chromedriver_%s_%s*' % (
          GS_CONTINUOUS_URL, platform, version),
      [])
  assert result == 0 and output, 'No release candidates found'
  candidate_pattern = re.compile(
      r'.*/chromedriver_%s_%s\.(\d+)\.zip$' % (platform, version))
  candidates = []
  for line in output.strip().split('\n'):
    result = candidate_pattern.match(line)
    if not result:
      print 'Ignored line "%s"' % line
      continue
    candidates.append(int(result.group(1)))

  # Release the latest candidate build that passed Android, if any.
  # In this way, if a hot fix is needed, we can delete the release from
  # the chromedriver bucket instead of bumping up the release version number.
  candidates.sort(reverse=True)
  for revision in candidates:
    android_result = _RevisionState(android_test_results, revision)
    if android_result == 'failed':
      print 'Android tests did not pass at revision', revision
    elif android_result == 'passed':
      print 'Android tests passed at revision', revision
      candidate = 'chromedriver_%s_%s.%s.zip' % (platform, version, revision)
      _Release('%s/%s' % (GS_CONTINUOUS_URL, candidate), version, platform)
      break
    else:
      print 'Android tests have not run at a revision as recent as', revision


def _Release(build, version, platform):
  """Releases the given candidate build."""
  release_name = 'chromedriver_%s.zip' % platform
  util.MarkBuildStepStart('releasing %s' % release_name)
  temp_dir = util.MakeTempDir()
  slave_utils.GSUtilCopy(build, temp_dir)
  zip_path = os.path.join(temp_dir, os.path.basename(build))

  if util.IsLinux():
    util.Unzip(zip_path, temp_dir)
    server_path = os.path.join(temp_dir, 'chromedriver')
    util.RunCommand(['strip', server_path])
    zip_path = util.Zip(server_path)

  slave_utils.GSUtilCopy(
      zip_path, '%s/%s/%s' % (GS_CHROMEDRIVER_BUCKET, version, release_name))

  _MaybeUploadReleaseNotes(version)
  _MaybeUpdateLatestRelease(version)


def _GetWebPageContent(url):
  """Return the content of the web page specified by the given url."""
  return urllib2.urlopen(url).read()


def _MaybeUploadReleaseNotes(version):
  """Upload release notes if conditions are right."""
  # Check if the current version has already been released.
  notes_name = 'notes.txt'
  notes_url = '%s/%s/%s' % (GS_CHROMEDRIVER_BUCKET, version, notes_name)
  prev_version = '.'.join([version.split('.')[0],
                           str(int(version.split('.')[1]) - 1)])
  prev_notes_url = '%s/%s/%s' % (
      GS_CHROMEDRIVER_BUCKET, prev_version, notes_name)

  result, _ = slave_utils.GSUtilListBucket(notes_url, [])
  if result == 0:
    return

  fixed_issues = []
  query = ('https://code.google.com/p/chromedriver/issues/csv?'
           'q=status%3AToBeReleased&colspec=ID%20Summary')
  issues = StringIO.StringIO(_GetWebPageContent(query).split('\n', 1)[1])
  for issue in csv.reader(issues):
    if not issue:
      continue
    issue_id = issue[0]
    desc = issue[1]
    labels = issue[2]
    fixed_issues += ['Resolved issue %s: %s [%s]' % (issue_id, desc, labels)]

  old_notes = ''
  temp_notes_fname = tempfile.mkstemp()[1]
  if not slave_utils.GSUtilDownloadFile(prev_notes_url, temp_notes_fname):
    with open(temp_notes_fname, 'rb') as f:
      old_notes = f.read()

  new_notes = '----------ChromeDriver v%s (%s)----------\n%s\n%s\n\n%s' % (
      version, datetime.date.today().isoformat(),
      'Supports Chrome v%s-%s' % _GetSupportedChromeVersions(),
      '\n'.join(fixed_issues),
      old_notes)
  with open(temp_notes_fname, 'w') as f:
    f.write(new_notes)

  if slave_utils.GSUtilCopy(temp_notes_fname, notes_url, mimetype='text/plain'):
    util.MarkBuildStepError()


def _MaybeUpdateLatestRelease(version):
  """Update the file LATEST_RELEASE with the latest release version number."""
  latest_release_fname = 'LATEST_RELEASE'
  latest_release_url = '%s/%s' % (GS_CHROMEDRIVER_BUCKET, latest_release_fname)

  # Check if LATEST_RELEASE is up-to-date.
  latest_released_version = _GetWebPageContent(
      '%s/%s' % (GS_CHROMEDRIVER_RELEASE_URL, latest_release_fname))
  if version == latest_released_version:
    return

  # Check if chromedriver was released on all supported platforms.
  supported_platforms = ['linux32', 'linux64', 'mac32', 'win32']
  for platform in supported_platforms:
    if not _WasReleased(version, platform):
      return

  util.MarkBuildStepStart('updating LATEST_RELEASE to %s' % version)

  temp_latest_release_fname = tempfile.mkstemp()[1]
  with open(temp_latest_release_fname, 'w') as f:
    f.write(version)
  if slave_utils.GSUtilCopy(temp_latest_release_fname, latest_release_url,
                            mimetype='text/plain'):
    util.MarkBuildStepError()


def _CleanTmpDir():
  tmp_dir = tempfile.gettempdir()
  print 'cleaning temp directory:', tmp_dir
  for file_name in os.listdir(tmp_dir):
    file_path = os.path.join(tmp_dir, file_name)
    if os.path.isdir(file_path):
      print 'deleting sub-directory', file_path
      shutil.rmtree(file_path, True)
    if file_name.startswith('chromedriver_'):
      print 'deleting file', file_path
      os.remove(file_path)


def _GetCommitPositionFromGitHash(snapshot_hashcode):
  json_url = GS_GIT_LOG_URL % snapshot_hashcode
  try:
    response = urllib2.urlopen(json_url)
  except urllib2.HTTPError as error:
    util.PrintAndFlush('HTTP Error %d' % error.getcode())
    return None
  except urllib2.URLError as error:
    util.PrintAndFlush('URL Error %s' % error.message)
    return None
  data = json.loads(response.read()[4:])
  if 'message' in data:
    message = data['message'].split('\n')
    message = [line for line in message if line.strip()]
    search_pattern = re.compile(GS_SEARCH_PATTERN)
    result = search_pattern.search(message[len(message)-1])
    if result:
      return result.group(1)
  util.PrintAndFlush('Failed to get svn revision number for %s' %
                     snapshot_hashcode)
  return None


def _GetGitHashFromCommitPosition(commit_position):
  json_url = CR_REV_URL % commit_position
  try:
    response = urllib2.urlopen(json_url)
  except urllib2.HTTPError as error:
    util.PrintAndFlush('HTTP Error %d' % error.getcode())
    return None
  except urllib2.URLError as error:
    util.PrintAndFlush('URL Error %s' % error.message)
    return None
  data = json.loads(response.read())
  if 'git_sha' in data:
    return data['git_sha']
  util.PrintAndFlush('Failed to get git hash for %s' % commit_position)
  return None


def _WaitForLatestSnapshot(revision):
  util.MarkBuildStepStart('wait_for_snapshot')
  def _IsRevisionNumber(revision):
    if isinstance(revision, int):
      return True
    else:
      return revision.isdigit()
  while True:
    snapshot_revision = archive.GetLatestSnapshotVersion()
    if not _IsRevisionNumber(snapshot_revision):
      snapshot_revision = _GetCommitPositionFromGitHash(snapshot_revision)
    if revision is not None and snapshot_revision is not None:
      if int(snapshot_revision) >= int(revision):
        break
      util.PrintAndFlush('Waiting for snapshot >= %s, found %s' %
                         (revision, snapshot_revision))
    time.sleep(60)
  util.PrintAndFlush('Got snapshot revision %s' % snapshot_revision)


def _AddToolsToPath(platform_name):
  """Add some tools like Ant and Java to PATH for testing steps to use."""
  paths = []
  error_message = ''
  if platform_name == 'win32':
    paths = [
        # Path to Ant and Java, required for the java acceptance tests.
        'C:\\Program Files (x86)\\Java\\ant\\bin',
        'C:\\Program Files (x86)\\Java\\jre\\bin',
    ]
    error_message = ('Java test steps will fail as expected and '
                     'they can be ignored.\n'
                     'Ant, Java or others might not be installed on bot.\n'
                     'Please refer to page "WATERFALL" on site '
                     'go/chromedriver.')
  if paths:
    util.MarkBuildStepStart('Add tools to PATH')
    path_missing = False
    for path in paths:
      if not os.path.isdir(path) or not os.listdir(path):
        print 'Directory "%s" is not found or empty.' % path
        path_missing = True
    if path_missing:
      print error_message
      util.MarkBuildStepError()
      return
    os.environ['PATH'] += os.pathsep + os.pathsep.join(paths)


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--android-packages',
      help=('Comma separated list of application package names, '
            'if running tests on Android.'))
  parser.add_option(
      '-r', '--revision', help='Chromium revision')
  parser.add_option(
      '', '--update-log', action='store_true',
      help='Update the test results log (only applicable to Android)')
  options, _ = parser.parse_args()

  bitness = '32'
  if util.IsLinux() and platform_module.architecture()[0] == '64bit':
    bitness = '64'
  platform = '%s%s' % (util.GetPlatformName(), bitness)
  if options.android_packages:
    platform = 'android'

  _CleanTmpDir()

  if not options.revision:
    commit_position = None
  elif options.revision.isdigit():
    commit_position = options.revision
  else:
    commit_position = _GetCommitPositionFromGitHash(options.revision)

  if platform == 'android':
    if not options.revision and options.update_log:
      parser.error('Must supply a --revision with --update-log')
    _DownloadPrebuilts()
  else:
    if not options.revision:
      parser.error('Must supply a --revision')
    if platform == 'linux64':
      _ArchivePrebuilts(commit_position)
    _WaitForLatestSnapshot(commit_position)

  _AddToolsToPath(platform)

  cmd = [
      sys.executable,
      os.path.join(_THIS_DIR, 'test', 'run_all_tests.py'),
  ]
  if platform == 'android':
    cmd.append('--android-packages=' + options.android_packages)

  passed = (util.RunCommand(cmd) == 0)

  _ArchiveServerLogs()

  if platform == 'android':
    if options.update_log:
      util.MarkBuildStepStart('update test result log')
      _UpdateTestResultsLog(platform, commit_position, passed)
  elif passed:
    _ArchiveGoodBuild(platform, commit_position)
    _MaybeRelease(platform)

  if not passed:
    # Make sure the build is red if there is some uncaught exception during
    # running run_all_tests.py.
    util.MarkBuildStepStart('run_all_tests.py')
    util.MarkBuildStepError()

  # Add a "cleanup" step so that errors from runtest.py or bb_device_steps.py
  # (which invoke this script) are kept in thier own build step.
  util.MarkBuildStepStart('cleanup')


if __name__ == '__main__':
  main()
