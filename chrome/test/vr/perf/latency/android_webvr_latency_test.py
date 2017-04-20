# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import webvr_latency_test

import logging
import os
import time


DEFAULT_SCREEN_WIDTH = 720
DEFAULT_SCREEN_HEIGHT = 1280
NUM_VR_ENTRY_ATTEMPTS = 5


class AndroidWebVrLatencyTest(webvr_latency_test.WebVrLatencyTest):
  """Android implementation of the WebVR latency test."""
  def __init__(self, args):
    super(AndroidWebVrLatencyTest, self).__init__(args)
    self._device_name = self._Adb(['shell', 'getprop',
                                  'ro.product.name']).strip()

  def _Setup(self):
    self._Adb(['root'])

    # Install the latest VrCore and Chrome APKs
    self._Adb(['install', '-r', '-d',
               '../../third_party/gvr-android-sdk/test-apks/vr_services'
               '/vr_services_current.apk'])
    self._SaveInstalledVrCoreVersion()
    # TODO(bsheedy): Make APK path configurable so usable with other channels
    self._Adb(['install', '-r', 'apks/ChromePublic.apk'])

    # Force WebVR support, remove open tabs, and don't have first run
    # experience.
    self._SetChromeCommandLineFlags(['--enable-webvr', '--no-restore-state',
                                     '--disable-fre'])
    # Wake up the device and sleep, otherwise WebGL can crash on startup.
    self._Adb(['shell', 'input', 'keyevent', 'KEYCODE_WAKEUP'])
    time.sleep(1)

    # Start Chrome
    self._Adb(['shell', 'am', 'start',
               '-a', 'android.intent.action.MAIN',
               '-n', 'org.chromium.chrome/com.google.android.apps.chrome.Main',
               self._flicker_app_url])
    time.sleep(10)

    # Tap the center of the screen to start presenting.
    # It's technically possible that the screen tap won't enter VR on the first
    # time, so try several times by checking for the logcat output from
    # entering VR
    (width, height) = self._GetScreenResolution()
    entered_vr = False
    for _ in xrange(NUM_VR_ENTRY_ATTEMPTS):
      self._Adb(['logcat', '-c'])
      self._Adb(['shell', 'input', 'touchscreen', 'tap', str(width/2),
                 str(height/2)])
      time.sleep(5)
      output = self._Adb(['logcat', '-d'])
      if 'Initialized GVR version' in output:
        entered_vr = True
        break
      logging.warning('Failed to enter VR, retrying')
    if not entered_vr:
      raise RuntimeError('Failed to enter VR after %d attempts'
                         % NUM_VR_ENTRY_ATTEMPTS)

  def _Teardown(self):
    # Exit VR and close Chrome
    self._Adb(['shell', 'input', 'keyevent', 'KEYCODE_BACK'])
    self._Adb(['shell', 'am', 'force-stop', 'org.chromium.chrome'])
    # Turn off the screen
    self._Adb(['shell', 'input', 'keyevent', 'KEYCODE_POWER'])

  def _Adb(self, cmd):
    """Runs the given command via adb.

    Returns:
      A string containing the stdout and stderr of the adb command.
    """
    # TODO(bsheedy): Maybe migrate to use Devil (overkill?)
    return self._RunCommand([self.args.adb_path] + cmd)

  def _SaveInstalledVrCoreVersion(self):
    """Retrieves the VrCore version and saves it for dashboard uploading."""
    output = self._Adb(['shell', 'dumpsys', 'package', 'com.google.vr.vrcore'])
    version = None
    for line in output.split('\n'):
      if 'versionName' in line:
        version = line.split('=')[1]
        break
    if version:
      logging.info('VrCore version is %s', version)
    else:
      logging.info('VrCore version not retrieved')
      version = '0'
    if not (self.args.output_dir and os.path.isdir(self.args.output_dir)):
      logging.warning('No output directory, not saving VrCore version')
      return
    with file(os.path.join(self.args.output_dir,
                           self.args.vrcore_version_file), 'w') as outfile:
      outfile.write(version)

  def _SetChromeCommandLineFlags(self, flags):
    """Sets the Chrome command line flags to the given list."""
    self._Adb(['shell', "echo 'chrome " + ' '.join(flags) + "' > "
               + '/data/local/tmp/chrome-command-line'])

  def _GetScreenResolution(self):
    """Retrieves the device's screen resolution, or a default if not found.

    Returns:
      A tuple (width, height).
    """
    output = self._Adb(['shell', 'dumpsys', 'display'])
    width = None
    height = None
    for line in output.split('\n'):
      if 'mDisplayWidth' in line:
        width = int(line.split('=')[1])
      elif 'mDisplayHeight' in line:
        height = int(line.split('=')[1])
      if width and height:
        break
    if not width:
      logging.warning('Could not get actual screen width, defaulting to %d',
                      DEFAULT_SCREEN_WIDTH)
      width = DEFAULT_SCREEN_WIDTH
    if not height:
      logging.warning('Could not get actual screen height, defaulting to %d',
                      DEFAULT_SCREEN_HEIGHT)
      height = DEFAULT_SCREEN_HEIGHT
    return (width, height)
