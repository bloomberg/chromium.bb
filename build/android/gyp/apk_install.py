#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Installs an APK.

"""

import optparse
import os
import re
import subprocess
import sys

from util import build_utils
from util import md5_check

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib import android_commands
from pylib.utils import apk_helper


def GetMetadata(apk_package):
  """Gets the metadata on the device for the apk_package apk."""
  adb = android_commands.AndroidCommands()
  output = adb.RunShellCommand('ls -l /data/app/')
  # Matches lines like:
  # -rw-r--r-- system   system    7376582 2013-04-19 16:34 org.chromium.chrome.testshell.apk
  # -rw-r--r-- system   system    7376582 2013-04-19 16:34 org.chromium.chrome.testshell-1.apk
  apk_matcher = lambda s: re.match('.*%s(-[0-9]*)?.apk$' % apk_package, s)
  matches = filter(apk_matcher, output)
  return matches[0] if matches else None


def HasInstallMetadataChanged(apk_package, metadata_path):
  """Checks if the metadata on the device for apk_package has changed."""
  if not os.path.exists(metadata_path):
    return True

  with open(metadata_path, 'r') as expected_file:
    return expected_file.read() != GetMetadata(apk_package)


def RecordInstallMetadata(apk_package, metadata_path):
  """Records the metadata from the device for apk_package."""
  metadata = GetMetadata(apk_package)
  if not metadata:
    raise 'APK install failed unexpectedly.'

  with open(metadata_path, 'w') as outfile:
    outfile.write(metadata)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--android-sdk-tools',
      help='Path to Android SDK tools.')
  parser.add_option('--apk-path',
      help='Path to .apk to install.')
  parser.add_option('--install-record',
      help='Path to install record (touched only when APK is installed).')
  parser.add_option('--stamp',
      help='Path to touch on success.')
  options, _ = parser.parse_args()

  # TODO(cjhopman): Should this install to all devices/be configurable?
  install_cmd = [
      os.path.join(options.android_sdk_tools, 'adb'),
      'install', '-r',
      options.apk_path]

  serial_number = android_commands.AndroidCommands().Adb().GetSerialNumber()
  apk_package = apk_helper.GetPackageName(options.apk_path)

  metadata_path = '%s.%s.device.time.stamp' % (options.apk_path, serial_number)

  # If the APK on the device does not match the one that was last installed by
  # the build, then the APK has to be installed (regardless of the md5 record).
  force_install = HasInstallMetadataChanged(apk_package, metadata_path)

  def Install():
    build_utils.CheckCallDie(install_cmd)
    RecordInstallMetadata(apk_package, metadata_path)
    build_utils.Touch(options.install_record)


  record_path = '%s.%s.md5.stamp' % (options.apk_path, serial_number)
  md5_check.CallAndRecordIfStale(
      Install,
      record_path=record_path,
      input_paths=[options.apk_path],
      input_strings=install_cmd,
      force=force_install)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
