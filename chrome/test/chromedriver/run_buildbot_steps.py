#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all the buildbot steps for ChromeDriver except for update/compile."""

import optparse
import os
import subprocess
import sys
import urllib2
import zipfile

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import chrome_paths
from common import util

GS_BUCKET = 'gs://chromedriver-prebuilts'
GS_ZIP_PREFIX = 'chromedriver2_prebuilts'
SLAVE_SCRIPT_DIR = os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir,
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'scripts', 'slave')
UPLOAD_SCRIPT = os.path.join(SLAVE_SCRIPT_DIR, 'skia', 'upload_to_bucket.py')
DOWNLOAD_SCRIPT = os.path.join(SLAVE_SCRIPT_DIR, 'gsutil_download.py')


def Archive(revision):
  print '@@@BUILD_STEP archive@@@'
  prebuilts = ['libchromedriver2.so', 'chromedriver2_server',
               'chromedriver2_unittests', 'chromedriver2_tests']
  build_dir = chrome_paths.GetBuildDir(prebuilts[0:1])
  zip_name = '%s_r%s.zip' % (GS_ZIP_PREFIX, revision)
  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, zip_name)
  print 'Zipping prebuilts %s' % zip_path
  f = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  for prebuilt in prebuilts:
    f.write(os.path.join(build_dir, prebuilt), prebuilt)
  f.close()

  cmd = [
    sys.executable,
    UPLOAD_SCRIPT,
    '--source_filepath=%s' % zip_path,
    '--dest_gsbase=%s' % GS_BUCKET
  ]
  if util.RunCommand(cmd):
    print '@@@STEP_FAILURE@@@'


def Download():
  print '@@@BUILD_STEP Download chromedriver prebuilts@@@'

  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, 'chromedriver2_prebuilts.zip')
  cmd = [
    sys.executable,
    DOWNLOAD_SCRIPT,
    '--url=%s' % GS_BUCKET,
    '--partial-name=%s' % GS_ZIP_PREFIX,
    '--dst=%s' % zip_path
  ]
  if util.RunCommand(cmd):
    print '@@@STEP_FAILURE@@@'

  build_dir = chrome_paths.GetBuildDir(['host_forwarder'])
  print 'Unzipping prebuilts %s to %s' % (zip_path, build_dir)
  f = zipfile.ZipFile(zip_path, 'r')
  f.extractall(build_dir)
  f.close()


def MaybeRelease(revision):
  # Version is embedded as: const char kChromeDriverVersion[] = "0.1";
  with open(os.path.join(_THIS_DIR, 'chrome', 'version.cc'), 'r') as f:
    version_line = filter(lambda x: 'kChromeDriverVersion' in x, f.readlines())
  version = version_line[0].split('"')[1]

  bitness = '32'
  if util.IsLinux():
    bitness = '64'
  zip_name = 'chromedriver2_%s%s_%s.zip' % (
      util.GetPlatformName(), bitness, version)

  site = 'https://code.google.com/p/chromedriver/downloads/list'
  s = urllib2.urlopen(site)
  downloads = s.read()
  s.close()

  if zip_name in downloads:
    return 0

  print '@@@BUILD_STEP releasing %s@@@' % zip_name
  if util.IsWindows():
    server_orig_name = 'chromedriver2_server.exe'
    server_name = 'chromedriver.exe'
  else:
    server_orig_name = 'chromedriver2_server'
    server_name = 'chromedriver'
  server = os.path.join(chrome_paths.GetBuildDir([server_orig_name]),
                        server_orig_name)

  print 'Zipping ChromeDriver server', server
  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, zip_name)
  f = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  f.write(server, server_name)
  f.close()

  cmd = [
    sys.executable,
    os.path.join(_THIS_DIR, 'third_party', 'googlecode',
                 'googlecode_upload.py'),
    '--summary', 'version of ChromeDriver2 r%s' % revision,
    '--project', 'chromedriver',
    '--user', 'chromedriver.bot@gmail.com',
    '--label', 'Release',
    zip_path
  ]
  with open(os.devnull, 'wb') as no_output:
    if subprocess.Popen(cmd, stdout=no_output, stderr=no_output).wait():
      print '@@@STEP_FAILURE@@@'


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--android-package',
      help='Application package name, if running tests on Android.')
  parser.add_option(
      '-r', '--revision', type='string', default=None,
      help='Chromium revision')
  options, _ = parser.parse_args()

  if options.android_package:
    Download()
  else:
    if options.revision is None:
      parser.error('Must supply a --revision')

    platform = util.GetPlatformName()
    if 'linux' in platform:
      Archive(options.revision)

  cmd = [
    sys.executable,
    os.path.join(_THIS_DIR, 'run_all_tests.py'),
  ]
  if options.android_package:
    cmd.append('--android-package=' + options.android_package)

  passed = (util.RunCommand(cmd) == 0)

  if not options.android_package and passed:
    MaybeRelease(options.revision)


if __name__ == '__main__':
  main()
