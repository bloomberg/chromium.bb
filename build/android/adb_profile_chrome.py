#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import gzip
import logging
import optparse
import os
import re
import select
import shutil
import sys
import threading
import time
import webbrowser
import zipfile
import zlib

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import pexpect

_TRACE_VIEWER_ROOT = os.path.join(constants.DIR_SOURCE_ROOT,
                                  'third_party', 'trace-viewer')
sys.path.append(_TRACE_VIEWER_ROOT)
from trace_viewer.build import trace2html

_DEFAULT_CHROME_CATEGORIES = '_DEFAULT_CHROME_CATEGORIES'


def _GetTraceTimestamp():
 return time.strftime('%Y-%m-%d-%H%M%S', time.localtime())


class ChromeTracingController(object):
  def __init__(self, adb, package_info, categories, ring_buffer):
    self._adb = adb
    self._package_info = package_info
    self._categories = categories
    self._ring_buffer = ring_buffer
    self._trace_file = None
    self._trace_interval = None
    self._trace_start_re = \
       re.compile(r'Logging performance trace to file: (.*)')
    self._trace_finish_re = \
       re.compile(r'Profiler finished[.] Results are in (.*)[.]')
    self._adb.StartMonitoringLogcat(clear=False)

  def __str__(self):
    return 'chrome trace'

  def StartTracing(self, interval):
    self._trace_interval = interval
    self._adb.SyncLogCat()
    self._adb.BroadcastIntent(self._package_info.package, 'GPU_PROFILER_START',
                              '-e categories "%s"' % ','.join(self._categories),
                              '-e continuous' if self._ring_buffer else '')
    # Chrome logs two different messages related to tracing:
    #
    # 1. "Logging performance trace to file [...]"
    # 2. "Profiler finished. Results are in [...]"
    #
    # The first one is printed when tracing starts and the second one indicates
    # that the trace file is ready to be pulled.
    try:
      self._trace_file = self._adb.WaitForLogMatch(self._trace_start_re,
                                                   None,
                                                   timeout=5).group(1)
    except pexpect.TIMEOUT:
      raise RuntimeError('Trace start marker not found. Is the correct version '
                         'of the browser running?')

  def StopTracing(self):
    if not self._trace_file:
      return
    self._adb.BroadcastIntent(self._package_info.package, 'GPU_PROFILER_STOP')
    self._adb.WaitForLogMatch(self._trace_finish_re, None, timeout=120)

  def PullTrace(self):
    # Wait a bit for the browser to finish writing the trace file.
    time.sleep(self._trace_interval / 4 + 1)

    trace_file = self._trace_file.replace('/storage/emulated/0/', '/sdcard/')
    host_file = os.path.join(os.path.curdir, os.path.basename(trace_file))
    self._adb.PullFileFromDevice(trace_file, host_file)
    return host_file


_SYSTRACE_OPTIONS = [
    # Compress the trace before sending it over USB.
    '-z',
    # Use a large trace buffer to increase the polling interval.
    '-b', '16384'
]

# Interval in seconds for sampling systrace data.
_SYSTRACE_INTERVAL = 15


class SystraceController(object):
  def __init__(self, adb, categories, ring_buffer):
    self._adb = adb
    self._categories = categories
    self._ring_buffer = ring_buffer
    self._done = threading.Event()
    self._thread = None
    self._trace_data = None

  def __str__(self):
    return 'systrace'

  @staticmethod
  def GetCategories(adb):
    return adb.RunShellCommand('atrace --list_categories')

  def StartTracing(self, interval):
    self._thread = threading.Thread(target=self._CollectData)
    self._thread.start()

  def StopTracing(self):
    self._done.set()

  def PullTrace(self):
    self._thread.join()
    self._thread = None
    if self._trace_data:
      output_name = 'systrace-%s' % _GetTraceTimestamp()
      with open(output_name, 'w') as out:
        out.write(self._trace_data)
      return output_name

  def _RunATraceCommand(self, command):
    # We use a separate interface to adb because the one from AndroidCommands
    # isn't re-entrant.
    device = ['-s', self._adb.GetDevice()] if self._adb.GetDevice() else []
    cmd = ['adb'] + device + ['shell', 'atrace', '--%s' % command] + \
        _SYSTRACE_OPTIONS + self._categories
    return cmd_helper.GetCmdOutput(cmd)

  def _CollectData(self):
    trace_data = []
    self._RunATraceCommand('async_start')
    try:
      while not self._done.is_set():
        self._done.wait(_SYSTRACE_INTERVAL)
        if not self._ring_buffer or self._done.is_set():
          trace_data.append(
              self._DecodeTraceData(self._RunATraceCommand('async_dump')))
    finally:
      trace_data.append(
          self._DecodeTraceData(self._RunATraceCommand('async_stop')))
    self._trace_data = ''.join([zlib.decompress(d) for d in trace_data])

  @staticmethod
  def _DecodeTraceData(trace_data):
    try:
      trace_start = trace_data.index('TRACE:')
    except ValueError:
      raise RuntimeError('Systrace start marker not found')
    trace_data = trace_data[trace_start + 6:]

    # Collapse CRLFs that are added by adb shell.
    if trace_data.startswith('\r\n'):
      trace_data = trace_data.replace('\r\n', '\n')

    # Skip the initial newline.
    return trace_data[1:]


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


def _CompressFile(host_file, output):
  with gzip.open(output, 'wb') as out:
    with open(host_file, 'rb') as input_file:
      out.write(input_file.read())
  os.unlink(host_file)


def _ArchiveFiles(host_files, output):
  with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as z:
    for host_file in host_files:
      z.write(host_file)
      os.unlink(host_file)


def _PackageTracesAsHtml(trace_files, html_file):
  with open(html_file, 'w') as f:
    trace2html.WriteHTMLForTracesToFile(trace_files, f)
  for trace_file in trace_files:
    os.unlink(trace_file)


def _PrintMessage(heading, eol='\n'):
  sys.stdout.write('%s%s' % (heading, eol))
  sys.stdout.flush()


def _WaitForEnter(timeout):
  select.select([sys.stdin], [], [], timeout)


def _StartTracing(controllers, interval):
  for controller in controllers:
    controller.StartTracing(interval)


def _StopTracing(controllers):
  for controller in controllers:
    controller.StopTracing()


def _PullTraces(controllers, output, compress, write_json):
  _PrintMessage('Downloading...', eol='')
  trace_files = []
  for controller in controllers:
    trace_files.append(controller.PullTrace())

  if not write_json:
    html_file = os.path.splitext(trace_files[0])[0] + '.html'
    _PackageTracesAsHtml(trace_files, html_file)
    trace_files = [html_file]

  if compress and len(trace_files) == 1:
    result = output or trace_files[0] + '.gz'
    _CompressFile(trace_files[0], result)
  elif len(trace_files) > 1:
    result = output or 'chrome-combined-trace-%s.zip' % _GetTraceTimestamp()
    _ArchiveFiles(trace_files, result)
  elif output:
    result = output
    shutil.move(trace_files[0], result)
  else:
    result = trace_files[0]

  _PrintMessage('done')
  _PrintMessage('Trace written to file://%s' % os.path.abspath(result))
  return result


def _CaptureAndPullTrace(controllers, interval, output, compress, write_json):
  trace_type = ' + '.join(map(str, controllers))
  try:
    _StartTracing(controllers, interval)
    if interval:
      _PrintMessage('Capturing %d-second %s. Press Enter to stop early...' % \
          (interval, trace_type), eol='')
      _WaitForEnter(interval)
    else:
      _PrintMessage('Capturing %s. Press Enter to stop...' % trace_type, eol='')
      raw_input()
  finally:
    _StopTracing(controllers)
  if interval:
    _PrintMessage('done')

  return _PullTraces(controllers, output, compress, write_json)


def _ComputeChromeCategories(options):
  categories = []
  if options.trace_frame_viewer:
    categories.append('disabled-by-default-cc.debug')
  if options.trace_ubercompositor:
    categories.append('disabled-by-default-cc.debug*')
  if options.trace_gpu:
    categories.append('disabled-by-default-gpu.debug*')
  if options.trace_flow:
    categories.append('disabled-by-default-toplevel.flow')
  if options.chrome_categories:
    categories += options.chrome_categories.split(',')
  return categories


def _ComputeSystraceCategories(options):
  if not options.systrace_categories:
    return []
  return options.systrace_categories.split(',')


def main():
  parser = optparse.OptionParser(description='Record about://tracing profiles '
                                 'from Android browsers. See http://dev.'
                                 'chromium.org/developers/how-tos/trace-event-'
                                 'profiling-tool for detailed instructions for '
                                 'profiling.')

  timed_options = optparse.OptionGroup(parser, 'Timed tracing')
  timed_options.add_option('-t', '--time', help='Profile for N seconds and '
                          'download the resulting trace.', metavar='N',
                           type='float')
  parser.add_option_group(timed_options)

  cont_options = optparse.OptionGroup(parser, 'Continuous tracing')
  cont_options.add_option('--continuous', help='Profile continuously until '
                          'stopped.', action='store_true')
  cont_options.add_option('--ring-buffer', help='Use the trace buffer as a '
                          'ring buffer and save its contents when stopping '
                          'instead of appending events into one long trace.',
                          action='store_true')
  parser.add_option_group(cont_options)

  categories = optparse.OptionGroup(parser, 'Trace categories')
  categories.add_option('-c', '--categories', help='Select Chrome tracing '
                        'categories with comma-delimited wildcards, '
                        'e.g., "*", "cat1*,-cat1a". Omit this option to trace '
                        'Chrome\'s default categories. Chrome tracing can be '
                        'disabled with "--categories=\'\'".',
                        metavar='CHROME_CATEGORIES', dest='chrome_categories',
                        default=_DEFAULT_CHROME_CATEGORIES)
  categories.add_option('-s', '--systrace', help='Capture a systrace with the '
                        'chosen comma-delimited systrace categories. You can '
                        'also capture a combined Chrome + systrace by enabling '
                        'both types of categories. Use "list" to see the '
                        'available categories. Systrace is disabled by '
                        'default.', metavar='SYS_CATEGORIES',
                        dest='systrace_categories', default='')
  categories.add_option('--trace-cc',
                        help='Deprecated, use --trace-frame-viewer.',
                        action='store_true')
  categories.add_option('--trace-frame-viewer',
                        help='Enable enough trace categories for '
                        'compositor frame viewing.', action='store_true')
  categories.add_option('--trace-ubercompositor',
                        help='Enable enough trace categories for '
                        'ubercompositor frame data.', action='store_true')
  categories.add_option('--trace-gpu', help='Enable extra trace categories for '
                        'GPU data.', action='store_true')
  categories.add_option('--trace-flow', help='Enable extra trace categories '
                        'for IPC message flows.', action='store_true')
  parser.add_option_group(categories)

  output_options = optparse.OptionGroup(parser, 'Output options')
  output_options.add_option('-o', '--output', help='Save trace output to file.')
  output_options.add_option('--json', help='Save trace as raw JSON instead of '
                            'HTML.', action='store_true')
  output_options.add_option('--view', help='Open resulting trace file in a '
                            'browser.', action='store_true')
  parser.add_option_group(output_options)

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
  if options.trace_cc:
    parser.parse_error("""--trace-cc is deprecated.

For basic jank busting uses, use  --trace-frame-viewer
For detailed study of ubercompositor, pass --trace-ubercompositor.

When in doubt, just try out --trace-frame-viewer.
""")

  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)

  adb = android_commands.AndroidCommands()
  if options.systrace_categories in ['list', 'help']:
    _PrintMessage('\n'.join(SystraceController.GetCategories(adb)))
    return 0

  if not options.time and not options.continuous:
    _PrintMessage('Time interval or continuous tracing should be specified.')
    return 1

  chrome_categories = _ComputeChromeCategories(options)
  systrace_categories = _ComputeSystraceCategories(options)
  package_info = _GetSupportedBrowsers()[options.browser]

  if chrome_categories and 'webview' in systrace_categories:
    logging.warning('Using the "webview" category in systrace together with '
                    'Chrome tracing results in duplicate trace events.')

  controllers = []
  if chrome_categories:
    controllers.append(ChromeTracingController(adb,
                                               package_info,
                                               chrome_categories,
                                               options.ring_buffer))
  if systrace_categories:
    controllers.append(SystraceController(adb,
                                          systrace_categories,
                                          options.ring_buffer))

  if not controllers:
    _PrintMessage('No trace categories enabled.')
    return 1

  if options.output:
    options.output = os.path.expanduser(options.output)
  result = _CaptureAndPullTrace(controllers,
                                options.time if not options.continuous else 0,
                                options.output,
                                options.compress,
                                options.json)
  if options.view:
    if sys.platform == 'darwin':
      os.system('/usr/bin/open %s' % os.path.abspath(result))
    else:
      webbrowser.open(result)


if __name__ == '__main__':
  sys.exit(main())
