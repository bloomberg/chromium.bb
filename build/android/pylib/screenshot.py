# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import time

import android_commands
import cmd_helper


def _GetTimestamp():
 return time.strftime('%Y-%m-%d-%H%M%S', time.localtime())


def _EnsureHostDirectory(host_file):
  host_dir = os.path.dirname(os.path.abspath(host_file))
  if not os.path.exists(host_dir):
    os.makedirs(host_dir)


def TakeScreenshot(adb, host_file):
  """Saves a screenshot image to |host_file| on the host.

  Args:
    adb: AndroidCommands instance.
    host_file: Path to the image file to store on the host.
  """
  host_file = os.path.abspath(host_file or
                              'screenshot-%s.png' % _GetTimestamp())
  _EnsureHostDirectory(host_file)
  device_file = '%s/screenshot.png' % adb.GetExternalStorage()
  adb.RunShellCommand('/system/bin/screencap -p %s' % device_file)
  adb.PullFileFromDevice(device_file, host_file)
  adb.RunShellCommand('rm -f "%s"' % device_file)
  return host_file


class VideoRecorder(object):
  """Records a screen capture video from an Android Device (KitKat or newer).

  Args:
    adb: AndroidCommands instance.
    host_file: Path to the video file to store on the host.
    megabits_per_second: Video bitrate in megabits per second. Allowed range
                         from 0.1 to 100 mbps.
    size: Video frame size tuple (width, height) or None to use the device
          default.
    rotate: If True, the video will be rotated 90 degrees.
  """
  def __init__(self, adb, host_file, megabits_per_second=4, size=None,
               rotate=False):
    self._adb = adb
    self._device_file = '%s/screen-recording.mp4' % adb.GetExternalStorage()
    self._host_file = host_file or 'screen-recording-%s.mp4' % _GetTimestamp()
    self._host_file = os.path.abspath(self._host_file)
    self._recorder = None
    self._recorder_pids = None
    self._recorder_stdout = None
    self._is_started = False

    self._args = ['adb']
    if self._adb.GetDevice():
      self._args += ['-s', self._adb.GetDevice()]
    self._args += ['shell', 'screenrecord', '--verbose']
    self._args += ['--bit-rate', str(megabits_per_second * 1000 * 1000)]
    if size:
      self._args += ['--size', '%dx%d' % size]
    if rotate:
      self._args += ['--rotate']
    self._args += [self._device_file]

  def Start(self):
    """Start recording video."""
    _EnsureHostDirectory(self._host_file)
    self._recorder_stdout = tempfile.mkstemp()[1]
    self._recorder = cmd_helper.Popen(
        self._args, stdout=open(self._recorder_stdout, 'w'))
    self._recorder_pids = self._adb.ExtractPid('screenrecord')
    if not self._recorder_pids:
      raise RuntimeError('Recording failed. Is your device running Android '
                         'KitKat or later?')

  def IsStarted(self):
    if not self._is_started:
      for line in open(self._recorder_stdout):
        self._is_started = line.startswith('Content area is ')
        if self._is_started:
          break
    return self._is_started

  def Stop(self):
    """Stop recording video."""
    os.remove(self._recorder_stdout)
    self._is_started = False
    if not self._recorder or not self._recorder_pids:
      return
    self._adb.RunShellCommand('kill -SIGINT ' + ' '.join(self._recorder_pids))
    self._recorder.wait()

  def Pull(self):
    """Pull resulting video file from the device.

    Returns:
      Output video file name on the host.
    """
    self._adb.PullFileFromDevice(self._device_file, self._host_file)
    self._adb.RunShellCommand('rm -f "%s"' % self._device_file)
    return self._host_file
