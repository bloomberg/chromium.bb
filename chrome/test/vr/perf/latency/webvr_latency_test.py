# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import motopho_thread as mt
import robot_arm as ra

import json
import glob
import logging
import numpy
import os
import re
import subprocess
import sys
import time


MOTOPHO_THREAD_TIMEOUT = 30


def GetTtyDevices(tty_pattern, vendor_ids):
  """Finds all devices connected to tty that match a pattern and device id.

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


class WebVrLatencyTest(object):
  """Base class for all WebVR latency tests.

  This is meant to be subclassed for each platform the test is run on. While
  the latency test itself is cross-platform, the setup and teardown for
  tests is platform-dependent.
  """
  def __init__(self, args):
    self.args = args
    self._num_samples = args.num_samples
    self._flicker_app_url = args.url
    assert (self._num_samples > 0),'Number of samples must be greater than 0'
    self._device_name = 'generic_device'

    # Connect to the Arduino that drives the servos
    devices = GetTtyDevices(r'ttyACM\d+', [0x2a03, 0x2341])
    assert (len(devices) == 1),'Found %d devices, expected 1' % len(devices)
    self.robot_arm = ra.RobotArm(devices[0])

  def RunTest(self):
    """Runs the steps to start Chrome, measure/save latency, and clean up."""
    self._Setup()
    self._Run()
    self._Teardown()

  def _Setup(self):
    """Perform any platform-specific setup."""
    raise NotImplementedError(
        'Platform-specific setup must be implemented in subclass')

  def _Run(self):
    """Run the latency test.

    Handles the actual latency measurement, which is identical across
    different platforms, as well as result saving.
    """
    # Motopho scripts use relative paths, so switch to the Motopho directory
    os.chdir(self.args.motopho_path)

    # Set up the thread that runs the Motopho script
    motopho_thread = mt.MotophoThread(self._num_samples)
    motopho_thread.start()

    # Run multiple times so we can get an average and standard deviation
    for _ in xrange(self._num_samples):
      self.robot_arm.ResetPosition()
      # Start the Motopho script
      motopho_thread.StartIteration()
      # Let the Motopho be stationary so the script can calculate the bias
      time.sleep(3)
      motopho_thread.BlockNextIteration()
      # Move so we can measure latency
      self.robot_arm.StartMotophoMovement()
      if not motopho_thread.WaitForIterationEnd(MOTOPHO_THREAD_TIMEOUT):
        # TODO(bsheedy): Look into ways to prevent Motopho from not sending any
        # data until unplugged and replugged into the machine after a reboot.
        logging.error('Motopho thread timeout, '
                      'Motopho may need to be replugged.')
      self.robot_arm.StopAllMovement()
      time.sleep(1)
    self._SaveResults(motopho_thread.latencies, motopho_thread.correlations)

  def _Teardown(self):
    """Performs any platform-specific teardown."""
    raise NotImplementedError(
        'Platform-specific setup must be implemented in subclass')

  def _RunCommand(self, cmd):
    """Runs the given cmd list and returns its output.

    Prints the command's output and exits if any error occurs.

    Returns:
      A string containing the stdout and stderr of the command.
    """
    try:
      return subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      logging.error('Failed command output: %s', e.output)
      raise e

  def _SetChromeCommandLineFlags(self, flags):
    raise NotImplementedError(
        'Command-line flag setting must be implemented in subclass')

  def _SaveResults(self, latencies, correlations):
    """Saves the results to a JSON file.

    Saved JSON object is compatible with Chrome perf dashboard if
    put in as the 'chart_data' value. Also logs the raw data and its
    average/standard deviation.
    """
    avg_latency = sum(latencies) / len(latencies)
    std_latency = numpy.std(latencies)
    avg_correlation = sum(correlations) / len(correlations)
    std_correlation = numpy.std(correlations)
    logging.info('Raw latencies: %s\nRaw correlations: %s\n'
                 'Avg latency: %f +/- %f\nAvg correlation: %f +/- %f',
                 str(latencies), str(correlations), avg_latency, std_latency,
                 avg_correlation, std_correlation)

    if not (self.args.output_dir and os.path.isdir(self.args.output_dir)):
      logging.warning('No output directory set, not saving results to file')
      return

    results = {
      'format_version': '1.0',
      'benchmark_name': 'webvr_latency',
      'benchmark_description': 'Measures the motion-to-photon latency of WebVR',
      'charts': {
        'correlation': {
          'summary': {
            'improvement_direction': 'up',
            'name': 'correlation',
            'std': std_correlation,
            'type': 'list_of_scalar_values',
            'units': '',
            'values': correlations,
          },
        },
        'latency': {
          'summary': {
            'improvement_direction': 'down',
            'name': 'latency',
            'std': std_latency,
            'type': 'list_of_scalar_values',
            'units': 'ms',
            'values': latencies,
          },
        }
      }
    }

    with file(os.path.join(self.args.output_dir,
                           self.args.results_file), 'w') as outfile:
      json.dump(results, outfile)
