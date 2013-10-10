#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import logging
import optparse
import os
import re
import sys
import time

from pylib import android_commands
from pylib import constants
from pylib import pexpect


def _GetSupportedBrowsers():
  # Add aliases for backwards compatibility.
  supported_browsers = {
    'stable': constants.PACKAGE_INFO['chrome_stable'],
    'beta': constants.PACKAGE_INFO['chrome_beta'],
    'dev': constants.PACKAGE_INFO['chrome_dev'],
    'build': constants.PACKAGE_INFO['chrome'],
  }
  supported_browsers.update(constants.PACKAGE_INFO)
  unsupported_browsers = ['content_browsertests', 'gtest', 'legacy_browser']
  for browser in unsupported_browsers:
    del supported_browsers[browser]
  return supported_browsers


def _StartTracing(adb, package_info, categories, continuous):
  adb.BroadcastIntent(package_info.package, 'GPU_PROFILER_START',
                      '-e categories "%s"' % categories,
                      '-e continuous' if continuous else '')


def _StopTracing(adb, package_info):
  adb.BroadcastIntent(package_info.package, 'GPU_PROFILER_STOP')


def _GetLatestTraceFileName(adb, check_for_multiple_traces=True):
  # Chrome logs two different messages related to tracing:
  #
  # 1. "Logging performance trace to file [...]"
  # 2. "Profiler finished. Results are in [...]"
  #
  # The first one is printed when tracing starts and the second one indicates
  # that the trace file is ready to be downloaded.
  #
  # We have to look for both of these messages to make sure we get the results
  # from the latest trace and that the trace file is complete.
  trace_start_re = re.compile(r'Logging performance trace to file')
  trace_finish_re = re.compile(r'Profiler finished[.] Results are in (.*)[.]')

  start_timeout = 5
  start_match = None
  finish_match = None

  while True:
    try:
      start_match = adb.WaitForLogMatch(trace_start_re, None,
                                        timeout=start_timeout)
    except pexpect.TIMEOUT:
      if start_match:
        break
      raise RuntimeError('Trace start marker not found. Is the correct version '
                         'of the browser running?')
    finish_match = adb.WaitForLogMatch(trace_finish_re, None, timeout=120)
    if not check_for_multiple_traces:
      break
    # Now that we've found one trace file, switch to polling for the rest of the
    # log to see if there are more.
    start_timeout = 1
  return finish_match.group(1)


def _DownloadTrace(adb, trace_file):
  trace_file = trace_file.replace('/storage/emulated/0/', '/sdcard/')
  host_file = os.path.abspath(os.path.basename(trace_file))
  adb.PullFileFromDevice(trace_file, host_file)
  return host_file


def _CompressFile(host_file):
  compressed_file = host_file + '.gz'
  with gzip.open(compressed_file, 'wb') as out:
    with open(host_file, 'rb') as input_file:
      out.write(input_file.read())
  os.unlink(host_file)
  return compressed_file


def _PrintMessage(heading, eol='\n'):
  sys.stdout.write(heading + eol)
  sys.stdout.flush()


def _DownloadLatestTrace(adb, compress, check_for_multiple_traces=True):
  _PrintMessage('Downloading trace...', eol='')
  trace_file = _GetLatestTraceFileName(adb, check_for_multiple_traces)
  host_file = _DownloadTrace(adb, trace_file)
  if compress:
    host_file = _CompressFile(host_file)
  _PrintMessage('done')
  _PrintMessage('Trace written to %s' % host_file)


def _CaptureAndDownloadTimedTrace(adb, package_info, categories, interval,
                                 continuous, compress):
  adb.StartMonitoringLogcat(clear=False)
  adb.SyncLogCat()

  try:
    _PrintMessage('Capturing %d-second trace. Press Ctrl-C to stop early...' \
        % interval, eol='')
    _StartTracing(adb, package_info, categories, continuous)
    time.sleep(interval)
  except KeyboardInterrupt:
    _PrintMessage('\nInterrupted, stopping...', eol='')
  _StopTracing(adb, package_info)
  _PrintMessage('done')

  # Wait a bit for the browser to finish writing the trace file.
  time.sleep(interval / 4 + 1)

  _DownloadLatestTrace(adb, compress, check_for_multiple_traces=False)


def _ComputeCategories(options):
  categories = [options.categories]
  if options.trace_cc:
    categories.append('disabled-by-default-cc.debug*')
  if options.trace_gpu:
    categories.append('disabled-by-default-gpu.debug*')
  return ",".join(categories)


def main():
  parser = optparse.OptionParser(description='Record about://tracing profiles '
                                 'from Android browsers. See http://dev.'
                                 'chromium.org/developers/how-tos/trace-event-'
                                 'profiling-tool for detailed instructions for '
                                 'profiling.')
  manual_options = optparse.OptionGroup(parser, 'Manual tracing')
  manual_options.add_option('--start', help='Start tracing.',
                            action='store_true')
  manual_options.add_option('--stop', help='Stop tracing.',
                            action='store_true')
  manual_options.add_option('-d', '--download', help='Download latest trace.',
                            action='store_true')
  parser.add_option_group(manual_options)

  auto_options = optparse.OptionGroup(parser, 'Automated tracing')
  auto_options.add_option('-t', '--time', help='Profile for N seconds and '
                          'download the resulting trace.', metavar='N',
                          type='float')
  parser.add_option_group(auto_options)

  categories = optparse.OptionGroup(parser, 'Trace categories')
  categories.add_option('-c', '--categories', help='Select categories to trace '
                        'with comma-delimited wildcards, e.g., '
                        '"*", "cat1*,-cat1a". Default is "*".', default='*')
  categories.add_option('--trace-cc', help='Enable extra trace categories for '
                        'compositor frame viewer data.', action='store_true')
  categories.add_option('--trace-gpu', help='Enable extra trace categories for '
                        'GPU data.', action='store_true')
  parser.add_option_group(categories)

  parser.add_option('-o', '--output', help='Save profile output to file. '
                    'Default is "/sdcard/Download/chrome-profile-results-*".')
  parser.add_option('--continuous', help='Using the trace buffer as a ring '
                    'buffer, continuously profile until stopped.',
                    action='store_true')
  browsers = sorted(_GetSupportedBrowsers().keys())
  parser.add_option('-b', '--browser', help='Select among installed browsers. '
                    'One of ' + ', '.join(browsers) + ', "stable" is used by '
                    'default.', type='choice', choices=browsers,
                    default='stable')
  parser.add_option('-v', '--verbose', help='Verbose logging.',
                    action='store_true')
  parser.add_option('-z', '--compress', help='Compress the resulting trace '
                    'with gzip. ', action='store_true')
  options, args = parser.parse_args()

  if not any([options.start, options.stop, options.time, options.download]):
    _PrintMessage('One of start/stop/download/time should be specified.')
    return 1

  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)

  adb = android_commands.AndroidCommands()
  categories = _ComputeCategories(options)
  package_info = _GetSupportedBrowsers()[options.browser]

  if options.start:
    _StartTracing(adb, package_info, categories, options.continuous)
  elif options.stop:
    _StopTracing(adb, package_info)
  elif options.download:
    _DownloadLatestTrace(adb, options.compress)
  elif options.time:
    _CaptureAndDownloadTimedTrace(adb, package_info, categories, options.time,
                                  options.continuous, options.compress)


if __name__ == '__main__':
  sys.exit(main())
