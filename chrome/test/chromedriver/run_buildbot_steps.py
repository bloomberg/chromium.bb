#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all the buildbot steps for ChromeDriver except for update/compile."""

import bisect
import csv
import datetime
import json
import optparse
import os
import platform as platform_module
import re
import shutil
import StringIO
import subprocess
import sys
import tempfile
import time
import urllib2
import zipfile

import archive
import chrome_paths
import util

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
GS_ARCHIVE_BUCKET = 'gs://chromedriver-prebuilts'
GS_ZIP_PREFIX = 'chromedriver_prebuilts'
GS_RC_BUCKET = 'gs://chromedriver-rc'
GS_RELEASE_PATH = GS_RC_BUCKET + '/releases'
RC_LOG_FORMAT = '%s_log.json'
RC_ZIP_FORMAT = 'chromedriver_%s_%s_%s.zip'
RC_ZIP_GLOB = 'chromedriver_%s_%s_*'
RELEASE_ZIP_FORMAT = 'chromedriver_%s_%s.zip'

SCRIPT_DIR = os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir, os.pardir,
                          os.pardir, os.pardir, os.pardir, 'scripts')
SITE_CONFIG_DIR = os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir,
                               os.pardir, os.pardir, os.pardir, os.pardir,
                               'site_config')
sys.path.append(SCRIPT_DIR)
sys.path.append(SITE_CONFIG_DIR)
from slave import gsutil_download
from slave import slave_utils


def ArchivePrebuilts(revision):
  """Uploads the prebuilts to google storage."""
  util.MarkBuildStepStart('archive')
  prebuilts = ['chromedriver',
               'chromedriver_unittests', 'chromedriver_tests']
  build_dir = chrome_paths.GetBuildDir(prebuilts[0:1])
  zip_name = '%s_r%s.zip' % (GS_ZIP_PREFIX, revision)
  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, zip_name)
  print 'Zipping prebuilts %s' % zip_path
  f = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  for prebuilt in prebuilts:
    f.write(os.path.join(build_dir, prebuilt), prebuilt)
  f.close()
  if slave_utils.GSUtilCopyFile(zip_path, GS_ARCHIVE_BUCKET):
    util.MarkBuildStepError()


def DownloadPrebuilts():
  """Downloads the most recent prebuilts from google storage."""
  util.MarkBuildStepStart('Download chromedriver prebuilts')

  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, 'chromedriver_prebuilts.zip')
  if gsutil_download.DownloadLatestFile(GS_ARCHIVE_BUCKET, GS_ZIP_PREFIX,
                                        zip_path):
    util.MarkBuildStepError()

  build_dir = chrome_paths.GetBuildDir(['host_forwarder'])
  print 'Unzipping prebuilts %s to %s' % (zip_path, build_dir)
  f = zipfile.ZipFile(zip_path, 'r')
  f.extractall(build_dir)
  f.close()
  # Workaround for Python bug: http://bugs.python.org/issue15795
  os.chmod(os.path.join(build_dir, 'chromedriver'), 0700)


def GetDownloads():
  """Gets the chromedriver google code downloads page."""
  result, output = slave_utils.GSUtilListBucket(GS_RELEASE_PATH + '/*', ['-l'])
  if result:
    return ''
  return output


def GetTestResultsLog(platform):
  """Gets the test results log for the given platform.

  Returns:
    A dictionary where the keys are SVN revisions and the values are booleans
    indicating whether the tests passed.
  """
  temp_log = tempfile.mkstemp()[1]
  log_name = RC_LOG_FORMAT % platform
  result = slave_utils.GSUtilDownloadFile(
      '%s/%s' % (GS_RC_BUCKET, log_name), temp_log)
  if result:
    return {}
  with open(temp_log, 'rb') as log_file:
    json_dict = json.load(log_file)
  # Workaround for json encoding dictionary keys as strings.
  return dict([(int(v[0]), v[1]) for v in json_dict.items()])


def _PutTestResultsLog(platform, test_results_log):
  """Pushes the given test results log to google storage."""
  temp_dir = util.MakeTempDir()
  log_name = RC_LOG_FORMAT % platform
  log_path = os.path.join(temp_dir, log_name)
  with open(log_path, 'wb') as log_file:
    json.dump(test_results_log, log_file)
  if slave_utils.GSUtilCopyFile(log_path, GS_RC_BUCKET):
    raise Exception('Failed to upload test results log to google storage')


def UpdateTestResultsLog(platform, revision, passed):
  """Updates the test results log for the given platform.

  Args:
    platform: The platform name.
    revision: The SVN revision number.
    passed: Boolean indicating whether the tests passed at this revision.
  """
  assert isinstance(revision, int), 'The revision must be an integer'
  log = GetTestResultsLog(platform)
  if len(log) > 500:
    del log[min(log.keys())]
  assert revision not in log, 'Results already exist for revision %s' % revision
  log[revision] = bool(passed)
  _PutTestResultsLog(platform, log)


def GetVersion():
  """Get the current chromedriver version."""
  with open(os.path.join(_THIS_DIR, 'VERSION'), 'r') as f:
    return f.read().strip()


def GetSupportedChromeVersions():
  """Get the minimum and maximum supported Chrome versions.

  Returns:
    A tuple of the form (min_version, max_version).
  """
  # Minimum supported Chrome version is embedded as:
  # const int kMinimumSupportedChromeVersion[] = {27, 0, 1453, 0};
  with open(os.path.join(_THIS_DIR, 'chrome', 'version.cc'), 'r') as f:
    lines = f.readlines()
    chrome_min_version_line = filter(
        lambda x: 'kMinimumSupportedChromeVersion' in x, lines)
  chrome_min_version = chrome_min_version_line[0].split('{')[1].split(',')[0]
  with open(os.path.join(chrome_paths.GetSrc(), 'chrome', 'VERSION'), 'r') as f:
    chrome_max_version = f.readlines()[0].split('=')[1].strip()
  return (chrome_min_version, chrome_max_version)


def GetReleaseCandidateUrl(platform):
  """Get the google storage url for the current version's release candidate."""
  rc_glob = RC_ZIP_GLOB % (platform, GetVersion())
  rc_url = '%s/%s' % (GS_RC_BUCKET, rc_glob)
  result, output = slave_utils.GSUtilListBucket(rc_url, ['-l'])
  assert result == 0 and output, 'Release candidate doesn\'t exist'
  rc_list = [b.split()[2] for b in output.split('\n') if GS_RC_BUCKET in b]
  assert len(rc_list) == 1, 'Only one release candidate should exist'
  return rc_list[0]


def ReleaseCandidateExists(platform):
  """Return whether a release candidate exists for the current version."""
  try:
    GetReleaseCandidateUrl(platform)
    return True
  except AssertionError:
    return False


def DeleteReleaseCandidate(platform):
  """Delete the release candidate for the current version."""
  rc_url = GetReleaseCandidateUrl(platform)
  result = slave_utils.GSUtilDeleteFile(rc_url)
  assert result == 0, 'Failed to delete %s' % rc_url


def DownloadReleaseCandidate(platform):
  """Download the release candidate for the current version."""
  release_name = RELEASE_ZIP_FORMAT % (platform, GetVersion())
  rc_url = GetReleaseCandidateUrl(platform)
  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, release_name)
  slave_utils.GSUtilDownloadFile(rc_url, zip_path)
  return zip_path


def GetReleaseCandidateRevision(platform):
  """Get the SVN revision for the current release candidate."""
  rc_url = GetReleaseCandidateUrl(platform)
  match = re.search(RC_ZIP_FORMAT % (platform, GetVersion(), '(\d+)'), rc_url)
  assert match, 'Bad format for release candidate zip name: %s' % rc_url
  return int(match.group(1))


def _RevisionState(test_results_log, revision):
  """Check the state of tests at a given SVN revision.

  Considers tests as having passed at a revision if they passed at revisons both
  before and after.

  Args:
    test_results_log: A test results log dictionary from GetTestResultsLog().
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


def _MaybeUpdateReleaseCandidate(platform, revision):
  """Uploads a release candidate if one doesn't already exist."""
  assert platform != 'android'
  version = GetVersion()
  if not ReleaseCandidateExists(platform):
    util.MarkBuildStepStart('upload release candidate')
    rc_file = _ConstructReleaseCandidate(platform, revision)
    if slave_utils.GSUtilCopyFile(rc_file, GS_RC_BUCKET):
      util.MarkBuildStepError()
  else:
    print 'Release candidate already exists'


def _MaybeRelease(platform):
  """Releases the release candidate if conditions are right."""
  assert platform != 'android'
  if not ReleaseCandidateExists(platform):
    return
  revision = GetReleaseCandidateRevision(platform)
  android_tests = _RevisionState(GetTestResultsLog('android'), revision)
  if android_tests == 'failed':
    print 'Android tests did not pass at revision %s' % revision
    version = GetVersion()
    DeleteReleaseCandidate(platform)
  elif android_tests == 'passed':
    print 'Android tests passed at revision %s' % revision
    Release(platform, revision)
  else:
    print 'Android tests have not run at a revision as recent as %s' % revision


def _ConstructReleaseCandidate(platform, revision):
  """Constructs a release candidate zip from the current build."""
  zip_name = RC_ZIP_FORMAT % (platform, GetVersion(), revision)
  if util.IsWindows():
    server_name = 'chromedriver.exe'
  else:
    server_name = 'chromedriver'
  server = os.path.join(chrome_paths.GetBuildDir([server_name]), server_name)

  print 'Zipping ChromeDriver server', server
  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, zip_name)
  f = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  f.write(server, server_name)
  f.close()
  return zip_path


def Release(platform, revision):
  """Releases the current release candidate if it hasn't been released yet."""
  version = GetVersion()
  chrome_min_version, chrome_max_version = GetSupportedChromeVersions()
  zip_name = RELEASE_ZIP_FORMAT % (platform, GetVersion())

  if zip_name in GetDownloads():
    print 'chromedriver version %s has already been released for %s' % (
        version, platform)
    return 0
  util.MarkBuildStepStart('releasing %s' % zip_name)
  zip_path = DownloadReleaseCandidate(platform)
  if slave_utils.GSUtilCopyFile(zip_path, GS_RELEASE_PATH):
    util.MarkBuildStepError()
  _MaybeUploadReleaseNotes(version)


def _MaybeUploadReleaseNotes(version):
  """Upload release notes if conditions are right."""
  name_template = 'release_notes_%s.txt'
  new_name = name_template % version
  prev_version = '.'.join([version.split('.')[0],
                          str(int(version.split('.')[1]) - 1)])
  old_name = name_template % prev_version

  fixed_issues = []
  query = ('https://code.google.com/p/chromedriver/issues/csv?'
           'q=status%3AToBeReleased&colspec=ID%20Summary')
  issues = StringIO.StringIO(urllib2.urlopen(query).read().split('\n', 1)[1])
  for issue in csv.reader(issues):
    if not issue:
      continue
    id = issue[0]
    desc = issue[1]
    labels = issue[2]
    fixed_issues += ['Resolved issue %s: %s [%s]' % (id, desc, labels)]

  old_notes = ''
  temp_notes_fname = tempfile.mkstemp()[1]
  if not slave_utils.GSUtilDownloadFile('%s/%s' % (GS_RELEASE_PATH, old_name),
                                        temp_notes_fname):
    with open(temp_notes_fname, 'rb') as f:
      old_notes = f.read()
  new_notes = '----------ChromeDriver v%s (%s)----------\n%s\n\n%s' % (
      version, datetime.date.today().isoformat(),
      '\n'.join(fixed_issues),
      old_notes)
  release_notes_txt = os.path.join(util.MakeTempDir(), new_name)
  with open(release_notes_txt, 'w') as f:
    f.write(new_notes)

  if new_name in GetDownloads():
    return
  if slave_utils.GSUtilCopyFile(release_notes_txt, GS_RELEASE_PATH):
    util.MarkBuildStepError()


def KillChromes():
  chrome_map = {
      'win': 'chrome.exe',
      'mac': 'Chromium',
      'linux': 'chrome',
  }
  if util.IsWindows():
    cmd = ['taskkill', '/F', '/IM']
  else:
    cmd = ['killall', '-9']
  cmd.append(chrome_map[util.GetPlatformName()])
  util.RunCommand(cmd)


def CleanTmpDir():
  tmp_dir = tempfile.gettempdir()
  print 'cleaning temp directory:', tmp_dir
  for file_name in os.listdir(tmp_dir):
    if os.path.isdir(os.path.join(tmp_dir, file_name)):
      print 'deleting sub-directory', file_name
      shutil.rmtree(os.path.join(tmp_dir, file_name), True)


def WaitForLatestSnapshot(revision):
  util.MarkBuildStepStart('wait_for_snapshot')
  while True:
    snapshot_revision = archive.GetLatestRevision(archive.Site.SNAPSHOT)
    if int(snapshot_revision) >= int(revision):
      break
    util.PrintAndFlush('Waiting for snapshot >= %s, found %s' %
                       (revision, snapshot_revision))
    time.sleep(60)
  util.PrintAndFlush('Got snapshot revision %s' % snapshot_revision)


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--android-packages',
      help='Comma separated list of application package names, '
           'if running tests on Android.')
  parser.add_option(
      '-r', '--revision', type='int', help='Chromium revision')
  parser.add_option('', '--update-log', action='store_true',
      help='Update the test results log (only applicable to Android)')
  options, _ = parser.parse_args()

  bitness = '32'
  if util.IsLinux() and platform_module.architecture()[0] == '64bit':
    bitness = '64'
  platform = '%s%s' % (util.GetPlatformName(), bitness)
  if options.android_packages:
    platform = 'android'

  if platform != 'android':
    KillChromes()
  CleanTmpDir()

  if platform == 'android':
    if not options.revision and options.update_log:
      parser.error('Must supply a --revision with --update-log')
    DownloadPrebuilts()
  else:
    if not options.revision:
      parser.error('Must supply a --revision')
    if platform == 'linux64':
      ArchivePrebuilts(options.revision)

    WaitForLatestSnapshot(options.revision)

  cmd = [
      sys.executable,
      os.path.join(_THIS_DIR, 'test', 'run_all_tests.py'),
  ]
  if platform == 'android':
    cmd.append('--android-packages=' + options.android_packages)

  passed = (util.RunCommand(cmd) == 0)

  if platform == 'android':
    if options.update_log:
      util.MarkBuildStepStart('update test result log')
      UpdateTestResultsLog(platform, options.revision, passed)
  elif passed:
    _MaybeUpdateReleaseCandidate(platform, options.revision)
    _MaybeRelease(platform)


if __name__ == '__main__':
  main()
