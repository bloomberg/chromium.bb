# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for automatically measuring motion-to-photon latency for VR.

Doing so requires two specialized pieces of hardware. The first is a Motopho,
which when used with a VR flicker app, finds the delay between movement and
the test device's screen updating in response to the movement. The second is
a set of servos, which physically moves the test device and Motopho during the
latency test.
"""

import argparse
import glob
import httplib
import logging
import os
import re
import serial
import subprocess
import sys
import threading
import time

# RobotArm connection constants
BAUD_RATE = 115200
CONNECTION_TIMEOUT = 3.0
NUM_TRIES = 5
# Motopho constants
DEFAULT_ADB_PATH = os.path.join(os.path.expanduser('~'),
                                'tools/android/android-sdk-linux',
                                'platform-tools/adb')
# TODO(bsheedy): See about adding tool via DEPS instead of relying on it
# existing on the bot already
DEFAULT_MOTOPHO_PATH = os.path.join(os.path.expanduser('~'), 'motopho/Motopho')
MOTOPHO_THREAD_TIMEOUT = 30

class MotophoThread(threading.Thread):
  """Handles the running of the Motopho script and extracting results."""
  def __init__(self):
    threading.Thread.__init__(self)
    self._latency = None
    self._max_correlation = None

  def run(self):
    motopho_output = ""
    try:
      motopho_output = subprocess.check_output(["./motophopro_nograph"],
                                               stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      logging.error('Failed to run Motopho script: %s', e.output)
      raise e

    if "FAIL" in motopho_output:
      logging.error('Failed to get latency, logging raw output: %s',
                    motopho_output)
      raise RuntimeError('Failed to get latency - correlation likely too low')

    self._latency = None
    self._max_correlation = None
    for line in motopho_output.split("\n"):
      if 'Motion-to-photon latency:' in line:
        self._latency = float(line.split(" ")[-2])
      if 'Max correlation is' in line:
        self._max_correlation = float(line.split(' ')[-1])
      if self._latency and self._max_correlation:
        break;

  @property
  def latency(self):
    return self._latency

  @property
  def max_correlation(self):
    return self._max_correlation


class RobotArm():
  """Handles the serial communication with the servos/arm used for movement."""
  def __init__(self, device_name, num_tries, baud, timeout):
    self._connection = None
    connected = False
    for _ in xrange(num_tries):
      try:
        self._connection = serial.Serial('/dev/' + device_name,
                                         baud,
                                         timeout=timeout)
      except serial.SerialException as e:
        pass
      if self._connection and 'Enter parameters' in self._connection.read(1024):
        connected = True
        break
    if not connected:
      raise serial.SerialException('Failed to connect to the robot arm.')

  def StartMotophoMovement(self):
    if not self._connection:
      return
    self._connection.write('9\n')

  def StopAllMovement(self):
    if not self._connection:
      return
    self._connection.write('0\n')


def GetParsedArgs():
  """Parses the command line arguments passed to the script.

  Fails if any unknown arguments are present.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('--adb-path',
                      type=os.path.realpath,
                      help='The absolute path to adb',
                      default=DEFAULT_ADB_PATH)
  parser.add_argument('--motopho-path',
                      type=os.path.realpath,
                      help='The absolute path to the directory with Motopho '
                           'scripts',
                      default=DEFAULT_MOTOPHO_PATH)
  parser.add_argument('--output-dir',
                      type=os.path.realpath,
                      help='The directory where the script\'s output files '
                           'will be saved')
  parser.add_argument('-v', '--verbose',
                      dest='verbose_count', default=0, action='count',
                      help='Verbose level (multiple times for more)')
  (args, unknown_args) = parser.parse_known_args()
  SetLogLevel(args.verbose_count)
  if unknown_args:
    parser.error('Received unknown arguments: %s' % ' '.join(unknown_args))
  return args


def SetLogLevel(verbose_count):
  """Sets the log level based on the command line arguments."""
  log_level = logging.WARNING
  if verbose_count == 1:
    log_level = logging.INFO
  elif verbose_count >= 2:
    log_level = logging.DEBUG
  logger = logging.getLogger()
  logger.setLevel(log_level)


def GetTtyDevices(tty_pattern, vendor_ids):
  """Find all devices connected to tty that match a pattern and device id.

  If a serial device is connected to the computer via USB, this function
  will check all tty devices that match tty_pattern, and return the ones
  that have vendor identification number in the list vendor_ids.

  Args:
    tty_pattern: The search pattern, such as r'ttyACM\d+'.
    vendor_ids: The list of 16-bit USB vendor ids, such as [0x2a03].

  Returns:
    A list of strings of tty devices, for example ['ttyACM0'].
  """
  product_string = 'PRODUCT='
  sys_class_dir = '/sys/class/tty/'

  tty_devices = glob.glob(sys_class_dir + '*')

  matcher = re.compile('.*' + tty_pattern)
  tty_matches = [x for x in tty_devices if matcher.search(x)]
  tty_matches = [x[len(sys_class_dir):] for x in tty_matches]

  found_devices = []
  for match in tty_matches:
    class_filename = sys_class_dir + match + '/device/uevent'
    with open(class_filename, 'r') as uevent_file:
      # Look for the desired product id in the uevent text.
      for line in uevent_file:
        if product_string in line:
          ids = line[len(product_string):].split('/')
          ids = [int(x, 16) for x in ids]

          for desired_id in vendor_ids:
            if desired_id in ids:
              found_devices.append(match)

  return found_devices


def RunCommand(cmd):
  """Runs the given cmd list.

  Prints the command's output and exits if any error occurs.
  """
  try:
    subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Failed command output: %s', e.output)
    raise e


def SetChromeCommandLineFlags(adb_path, flags):
  """Sets the given Chrome command line flags."""
  RunCommand([adb_path,
                'shell', "echo 'chrome " + ' '.join(flags) + "' > "
                + '/data/local/tmp/chrome-command-line'])


def main():
  args = GetParsedArgs()

  RunCommand([args.adb_path, 'root'])
  RunCommand([args.adb_path, 'install', '-r', 'apks/ChromePublic.apk'])
  # Force WebVR support and don't have first run experience
  SetChromeCommandLineFlags(args.adb_path, ['--enable-webvr', '--disable-fre'])

  # Motopho scripts use relative paths, so switch to the Motopho directory
  os.chdir(args.motopho_path)

  # Connect to the Arduino that drives the servos
  devices = GetTtyDevices(r'ttyACM\d+', [0x2a03, 0x2341])
  if len(devices) != 1:
    logging.error('Found %d devices, expected 1', len(devices))
    return 1
  robot_arm = RobotArm(devices[0], NUM_TRIES, BAUD_RATE, CONNECTION_TIMEOUT)

  # Wake the device
  RunCommand([args.adb_path, 'shell', 'input', 'keyevent', 'KEYCODE_WAKEUP'])
  # Sleep a bit, otherwise WebGL can crash when Canary starts
  time.sleep(1)

  # Start Chrome and go to the flicker app
  # TODO(bsheedy): See about having versioned copies of the flicker app instead
  # of using personal github.
  RunCommand([args.adb_path, 'shell', 'am', 'start',
               '-a', 'android.intent.action.MAIN',
               '-n', 'org.chromium.chrome/com.google.android.apps.chrome.Main',
               'https://weableandbob.github.io/Motopho/flicker_apps/webvr/webvr-flicker-app-klaus.html?polyfill=0\&canvasClickPresents=1'])
  time.sleep(10)

  # Tap the screen to start presenting
  RunCommand(
      [args.adb_path, 'shell', 'input', 'touchscreen', 'tap', '800', '800'])
  # Wait for VR to fully start up
  time.sleep(5)

  # Start the Motopho script
  motopho_thread = MotophoThread()
  motopho_thread.start()
  # Let the Motopho be stationary so the script can calculate its bias
  time.sleep(3)

  # Move so we can measure latency
  robot_arm.StartMotophoMovement()
  motopho_thread.join(MOTOPHO_THREAD_TIMEOUT)
  if motopho_thread.isAlive():
    # TODO(bsheedy): Look into ways to prevent Motopho from not sending any
    # data until unplugged and replugged into the machine after a reboot.
    logging.error('Motopho thread timeout, Motopho may need to be replugged.')
  robot_arm.StopAllMovement()

  logging.info('Latency: %s', motopho_thread.latency)
  logging.info('Max correlation: %s', motopho_thread.max_correlation)

  # TODO(bsheedy): Change this to output JSON compatible with the performance
  # dashboard.
  if args.output_dir and os.path.isdir(args.output_dir):
    with file(os.path.join(args.output_dir, 'output.txt'), 'w') as outfile:
      outfile.write('Latency: %s\nMax correlation: %s\n' %
                    (motopho_thread.latency, motopho_thread.max_correlation))

  # Exit VR and Close Chrome
  # TODO(bsheedy): See about closing current tab before exiting so they don't
  # pile up over time.
  RunCommand([args.adb_path, 'shell', 'input', 'keyevent', 'KEYCODE_BACK'])
  RunCommand([args.adb_path, 'shell', 'am', 'force-stop',
                'org.chromium.chrome'])

  # Turn off the screen
  RunCommand([args.adb_path, 'shell', 'input', 'keyevent', 'KEYCODE_POWER'])

  return 0

if __name__ == '__main__':
  sys.exit(main())
