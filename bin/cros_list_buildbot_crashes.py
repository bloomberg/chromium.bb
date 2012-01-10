#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for listing top buildbot crashes."""

from __future__ import print_function
import collections
import datetime
import multiprocessing
import logging
import optparse
import os
import re
import sys

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib


def ConvertGoogleStorageURLToHttpURL(url):
  return url.replace('gs://', 'http://sandbox.google.com/storage/')


def Worker(queue, fn):
  """Worker thread for passing queue entries to a function."""
  while True:
    result = queue.get()
    if result is None:
      queue.put(None)
      return
    fn(*result)


class CrashTriager(object):

  CRASH_PATTERN = re.compile(r'/([^/.]*)\.(\d+)[^/]*\.dmp\.txt$')
  STACK_TRACE_PATTERN = re.compile(r'Thread 0 ((?:[^\n]+\n)*)')
  FUNCTION_PATTERN = re.compile(r'\S+!\S+')

  def __init__(self, start_date, chrome_branch, all_programs, list_all, jobs):
    self.start_date = start_date
    self.chrome_branch = chrome_branch
    self.config_queue = multiprocessing.Queue()
    self.crash_triage_queue = multiprocessing.Queue()
    self.stack_trace_queue = multiprocessing.Queue()
    self.stack_traces = collections.defaultdict(list)
    self.all_programs = all_programs
    self.list_all = list_all
    self.jobs = jobs

  def Run(self):
    """Run the crash triager, printing the most common stack traces."""
    # Launch processes to start processing and printing crash lists.
    self.ScheduleCrashListProcessing()
    crash_list_workers = []
    crash_triage_workers = []
    for _ in range(self.jobs):
      crash_list_workers.append(self._CreateCrashListWorker())
      crash_triage_workers.append(self._CreateCrashDownloadWorker())
    for w in crash_list_workers + crash_triage_workers:
      w.start()
    printer = multiprocessing.Process(target=self._StackTracePrinter)
    printer.start()

    # After workers have finished listing the crashes, report that there
    # are no crashes left to triage.
    for w in crash_list_workers:
      w.join()
    self.crash_triage_queue.put(None)

    # After workers have finished triaging the crashes, report that there
    # are no stack traces left to print.
    for w in crash_triage_workers:
      w.join()
    self.stack_trace_queue.put(None)

    # Wait for the stack traces to be printed.
    printer.join()

  def ScheduleCrashListProcessing(self):
    """Schedule various bots for crash list processing."""
    for bot_id, build_config in cbuildbot_config.config.iteritems():
      if build_config['vm_tests']:
        self.config_queue.put((bot_id, build_config))
    self.config_queue.put(None)

  def _GetGSPath(self, bot_id, build_config):
    """Get the Google Storage path where crashes are stored for a given bot.

    Args:
      bot_id: Gather crashes from this bot id.
      build_config: Configuration options for this bot.
    """
    if build_config['gs_path'] == cbuildbot_config.GS_PATH_DEFAULT:
      gsutil_archive = 'gs://chromeos-image-archive/' + bot_id
    else:
      gsutil_archive = build_config['gs_path']
    return gsutil_archive

  def _ListCrashesForBot(self, bot_id, build_config):
    """List all crashes for the specified bot.

    Example output line: [
      'gs://chromeos-image-archive/amd64-generic-full/R18-1414.0.0-a1-b537/' +
      'chrome.20111207.181520.2533.dmp.txt'
    ]

    Args:
      bot_id: Gather crashes from this bot id.
      build_config: Configuration options for this bot.
    """
    chrome_branch = self.chrome_branch
    gsutil_archive = self._GetGSPath(bot_id, build_config)
    pattern = '%s/R%s-*.dmp.txt' % (gsutil_archive, chrome_branch)
    out = cros_build_lib.RunCommand(['gsutil', 'ls', pattern],
                                    error_code_ok=True,
                                    redirect_stdout=True,
                                    redirect_stderr=True,
                                    print_cmd=False)
    if out.returncode == 0:
      return out.output.split('\n')
    return []

  def _ProcessCrashListForBot(self, bot_id, build_config):
    """Process crashes for a given bot.

    Args:
      bot_id: Gather crashes from this bot id.
      build_config: Configuration options for this bot.
    """
    for line in self._ListCrashesForBot(bot_id, build_config):
      m = self.CRASH_PATTERN.search(line)
      if m is None: continue
      program, crash_date = m.groups()
      if self.all_programs or program == 'chrome':
        crash_date_obj = datetime.datetime.strptime(crash_date, '%Y%m%d')
        if self.start_date <= crash_date_obj:
          self.crash_triage_queue.put((program, crash_date, line))

  def _CreateCrashListWorker(self):
    """Create a worker process for processing crash lists."""
    worker_args = (self.config_queue, self._ProcessCrashListForBot)
    return multiprocessing.Process(target=Worker, args=worker_args)

  def _GetStackTrace(self, crash_report_url):
    """Retrieve a stack trace using gsutil cat.

    Args:
      crash_report_url: The URL where the crash is stored.
    """
    out = cros_build_lib.RunCommand(['gsutil', 'cat', crash_report_url],
                                    error_code_ok=True,
                                    redirect_stdout=True,
                                    redirect_stderr=True,
                                    print_cmd=False)
    return out

  def _DownloadStackTrace(self, program, crash_date, url):
    """Download a crash report, queuing up the stack trace info.

    Args:
      program: The program that crashed.
      crash_date: The date of the crash.
      url: The URL where the crash is stored.
    """
    out = self._GetStackTrace(url)
    if out.returncode == 0:
      self.stack_trace_queue.put((program, crash_date, url, out.output))

  def _CreateCrashDownloadWorker(self):
    """Create a worker process for downloading stack traces."""
    worker_args = (self.crash_triage_queue, self._DownloadStackTrace)
    return multiprocessing.Process(target=Worker, args=worker_args)

  def _ProcessStackTrace(self, program, date, url, output):
    """Process a stack trace that has been downloaded.

    Args:
      program: The program that crashed.
      date: The date of the crash.
      url: The URL where the crash is stored.
      output: The content of the stack trace.
    """
    signature = 'uncategorized'
    m = self.STACK_TRACE_PATTERN.search(output)
    functions = []
    if m:
      trace = m.group(1)
      functions = self.FUNCTION_PATTERN.findall(trace)
    if functions and not functions[0].endswith('!__libc_start_main'):
      signature = functions[0]
    stack_len = len(functions)
    self.stack_traces[(program, signature)].append((date, stack_len, url))

  def _PrintStackTraces(self):
    """Print all stack traces."""

    # Print header.
    if self.list_all:
      print('Crash count, program, function, date, URL')
    else:
      print('Crash count, program, function, first crash, last crash, URL')

    # Print details about stack traces.
    stack_traces = sorted(self.stack_traces.iteritems(),
                          key=lambda x: len(x[1]), reverse=True)
    for (program, signature), crashes in stack_traces:
      if self.list_all:
        for crash in sorted(crashes, reverse=True):
          crash_url = ConvertGoogleStorageURLToHttpURL(crash[2])
          output = (str(len(crashes)), program, signature, crash[0], crash_url)
          print(*output, sep=', ')
      else:
        first_date = min(x[0] for x in crashes)
        last_date = max(x[0] for x in crashes)
        crash_url = ConvertGoogleStorageURLToHttpURL(max(crashes)[2])
        output = (str(len(crashes)), program, signature, first_date, last_date,
                  crash_url)
        print(*output, sep=', ')

  def _StackTracePrinter(self):
    Worker(self.stack_trace_queue, self._ProcessStackTrace)
    self._PrintStackTraces()


def _GetChromeBranch():
  """Get the current Chrome branch."""
  version_file = os.path.join(constants.SOURCE_ROOT, constants.VERSION_FILE)
  version_info = manifest_version.VersionInfo(version_file=version_file)
  return version_info.chrome_branch


def _CreateParser():
  """Generate and return the parser with all the options."""
  # Parse options
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage=usage)

  # Main options
  parser.add_option('', '--days',  dest='days', default=7, type='int',
                    help=('Number of days to look at for crash info.'))
  parser.add_option('', '--chrome_branch',  dest='chrome_branch',
                    default=_GetChromeBranch(),
                    help=('Chrome branch to look at for crash info.'))
  parser.add_option('', '--all_programs', action='store_true',
                    dest='all_programs', default=False,
                    help=('Show crashes in programs other than Chrome.'))
  parser.add_option('', '--list', action='store_true', dest='list_all',
                    default=False,
                    help=('List all stack traces found (not just one).'))
  parser.add_option('', '--jobs',  dest='jobs', default=32, type='int',
                    help=('Number of processes to run in parallel.'))
  return parser


def Main(argv, stderr):
  # Setup boto config for gsutil.
  boto_config = os.path.abspath(os.path.join(constants.SOURCE_ROOT,
      'src/private-overlays/chromeos-overlay/googlestorage_account.boto'))
  if os.path.isfile(boto_config):
    os.environ['BOTO_CONFIG'] = boto_config
  else:
    print('Cannot find %s' % boto_config, file=stderr)
    print('This function requires a private checkout.', file=stderr)
    print('See http://goto/chromeos-building', file=stderr)
    sys.exit(1)

  logging.disable(level=logging.INFO)
  parser = _CreateParser()
  (options, _) = parser.parse_args(argv)
  since = datetime.datetime.today() - datetime.timedelta(days=options.days)
  triager = CrashTriager(since, options.chrome_branch, options.all_programs,
                         options.list_all, options.jobs)
  triager.Run()


if __name__ == '__main__':
  Main(sys.argv, sys.stderr)
