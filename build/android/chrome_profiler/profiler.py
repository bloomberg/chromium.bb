# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import os
import shutil
import sys
import zipfile

from chrome_profiler import ui
from chrome_profiler import util

from pylib import constants

sys.path.append(os.path.join(constants.DIR_SOURCE_ROOT,
                             'third_party',
                             'trace-viewer'))
# pylint: disable=F0401
from trace_viewer.build import trace2html


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


def _StartTracing(controllers, interval):
  for controller in controllers:
    controller.StartTracing(interval)


def _StopTracing(controllers):
  for controller in controllers:
    controller.StopTracing()


def _PullTraces(controllers, output, compress, write_json):
  ui.PrintMessage('Downloading...', eol='')
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
    result = output or 'chrome-combined-trace-%s.zip' % util.GetTraceTimestamp()
    _ArchiveFiles(trace_files, result)
  elif output:
    result = output
    shutil.move(trace_files[0], result)
  else:
    result = trace_files[0]

  ui.PrintMessage('done')
  ui.PrintMessage('Trace written to file://%s' % os.path.abspath(result))
  return result


def GetSupportedBrowsers():
  """Returns the package names of all supported browsers."""
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


def CaptureProfile(controllers, interval, output=None, compress=False,
                   write_json=False):
  """Records a profiling trace saves the result to a file.

  Args:
    controllers: List of tracing controllers.
    interval: Time interval to capture in seconds. An interval of None (or 0)
        continues tracing until stopped by the user.
    output: Output file name or None to use an automatically generated name.
    compress: If True, the result will be compressed either with gzip or zip
        depending on the number of captured subtraces.
    write_json: If True, prefer JSON output over HTML.

  Returns:
    Path to saved profile.
  """
  trace_type = ' + '.join(map(str, controllers))
  try:
    _StartTracing(controllers, interval)
    if interval:
      ui.PrintMessage('Capturing %d-second %s. Press Enter to stop early...' % \
          (interval, trace_type), eol='')
      ui.WaitForEnter(interval)
    else:
      ui.PrintMessage('Capturing %s. Press Enter to stop...' % \
          trace_type, eol='')
      raw_input()
  finally:
    _StopTracing(controllers)
  if interval:
    ui.PrintMessage('done')

  return _PullTraces(controllers, output, compress, write_json)
