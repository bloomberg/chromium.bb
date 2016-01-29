#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Windows-only end-to-end integration test for Kasko and SyzyAsan.

This test ensures that the interface between SyzyAsan, Kasko and Chrome works
as expected. The test causes a crash that should be detected by SyzyAsan and
delivered via Kasko to a locally hosted test crash server.

Note that this test only works against non-component Release and Official builds
of Chrome with Chrome branding, and attempting to use it with anything else will
most likely lead to constant failures.

Typical usage (assuming in root 'src' directory):

- generate project files with the following GYP variables:
    syzyasan=1 win_z7=0 chromium_win_pch=0
- build the release Chrome binaries:
    ninja -C out\Release chrome.exe chromedriver.exe
- run the test:
    python chrome/test/kasko/syzyasan_integration_test.py
"""

import logging
import os
import optparse
import re
import shutil
import subprocess
import sys

# Bring in the Kasko module.
KASKO_DIR = os.path.join(os.path.dirname(__file__), 'py')
sys.path.append(KASKO_DIR)
import kasko


_LOGGER = logging.getLogger(os.path.basename(__file__))
_CHROME_DLL = 'chrome.dll'
_INSTRUMENT = 'instrument.exe'
_SYZYASAN_RTL = 'syzyasan_rtl.dll'


def _ParseCommandLine():
  self_dir = os.path.dirname(__file__)
  src_dir = os.path.abspath(os.path.join(self_dir, '..', '..', '..'))

  option_parser = kasko.config.GenerateOptionParser()
  option_parser.add_option('--instrumented-dir', dest='instrumented_dir',
      type='string',
      help='Path where instrumented binaries will be placed. If instrumented '
           'binaries already exist here they will be reused.')
  option_parser.add_option('--skip-instrumentation',
      dest='skip_instrumentation', action='store_true', default=False,
      help='Skips instrumentation if specified. To be used when testing '
           'against an already instrumented build of Chrome.')
  option_parser.add_option('--syzygy-dir', dest='syzygy_dir', type='string',
      default=os.path.join(src_dir, 'third_party', 'syzygy', 'binaries', 'exe'),
      help='Path to Syzygy binaries. By default will look in third_party.')
  options = kasko.config.ParseCommandLine(option_parser)

  if not os.path.isdir(options.syzygy_dir):
    option_parser.error('Invalid syzygy directory.')
  for basename in [_INSTRUMENT, _SYZYASAN_RTL]:
    path = os.path.join(options.syzygy_dir, basename)
    if not os.path.isfile(path):
      option_parser.error('Missing syzygy binary: %s' % path)

  _LOGGER.debug('Using syzygy path: %s', options.syzygy_dir)

  return options


def _DecorateFilename(name, deco):
  """Decorates a filename, transforming 'foo.baz.bar' to 'foo.dec.baz.bar'."""
  d = os.path.dirname(name)
  b = os.path.basename(name)
  b = b.split('.', 1)
  b.insert(1, deco)
  return os.path.join(d, '.'.join(b))


def _BackupFile(path, dst_dir):
  """Creates a backup of a file in the specified directory."""
  bak = os.path.abspath(os.path.join(dst_dir, os.path.basename(path)))
  if os.path.exists(bak):
    os.remove(bak)
  # Copy the file, with its permissions and timestamps, etc.
  _LOGGER.debug('Copying "%s" to "%s".' % (path, bak))
  shutil.copyfile(path, bak)
  shutil.copystat(path, bak)
  return bak


def _RestoreFile(path, backup):
  """Restores a file from its backup. Leaves the backup file."""
  if not os.path.exists(backup):
    raise Exception('Backup does not exist: %s' % backup)
  if os.path.exists(path):
    os.remove(path)
  _LOGGER.debug('Restoring "%s" from "%s".' % (path, backup))
  shutil.copyfile(backup, path)
  shutil.copystat(backup, path)


class _ScopedInstrumentedChrome(object):
  """SyzyAsan Instruments a Chrome installation in-place."""

  def __init__(self, chrome_dir, syzygy_dir, temp_dir, instrumented_dir=None,
               verbose=False, skip_instrumentation=False):
    self.chrome_dir_ = chrome_dir
    self.syzygy_dir_ = syzygy_dir
    self.temp_dir_ = temp_dir
    self.instrumented_dir_ = instrumented_dir
    self.verbose_ = verbose
    self.skip_instrumentation_ = skip_instrumentation

  def _ProduceInstrumentedBinaries(self):
    # Generate the instrumentation command-line. This will place the
    # instrumented binaries in the temp directory.
    instrument = os.path.abspath(os.path.join(self.syzygy_dir_, _INSTRUMENT))
    cmd = [instrument,
           '--mode=asan',
           '--input-image=%s' % self.chrome_dll_bak_,
           '--input-pdb=%s' % self.chrome_dll_pdb_bak_,
           '--output-image=%s' % self.chrome_dll_inst_,
           '--output-pdb=%s' % self.chrome_dll_pdb_inst_,
           '--no-augment-pdb']

    _LOGGER.debug('Instrumenting Chrome binaries.')

    # If in verbose mode then let the instrumentation produce output directly.
    if self.verbose_:
      result = subprocess.call(cmd)
    else:
      # Otherwise run the command with all output suppressed.
      proc = subprocess.call(cmd, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
      stdout, stderr = proc.communicate()
      result = proc.returncode
      if result != 0:
        sys.stdout.write(stdout)
        sys.stderr.write(stderr)

    if result != 0:
      raise Exception('Failed to instrument: %s' % self.chrome_dll_)

    return

  def __enter__(self):
    """In-place instruments a Chrome installation with SyzyAsan."""
    # Do nothing if instrumentation is to be skipped entirely.
    if self.skip_instrumentation_:
      _LOGGER.debug('Assuming binaries already instrumented.')
      return self

    # Build paths to the original Chrome binaries.
    self.chrome_dll_ = os.path.abspath(os.path.join(
        self.chrome_dir_, _CHROME_DLL))
    self.chrome_dll_pdb_ = self.chrome_dll_ + '.pdb'

    # Backup the original Chrome binaries to the temp directory.
    orig_dir = os.path.join(self.temp_dir_, 'orig')
    os.makedirs(orig_dir)
    self.chrome_dll_bak_ = _BackupFile(self.chrome_dll_, orig_dir)
    self.chrome_dll_pdb_bak_ = _BackupFile(self.chrome_dll_pdb_, orig_dir)

    # Generate the path to the instrumented binaries.
    inst_dir = os.path.join(self.temp_dir_, 'inst')
    if self.instrumented_dir_:
      inst_dir = self.instrumented_dir_
    if not os.path.isdir(inst_dir):
      os.makedirs(inst_dir)
    self.chrome_dll_inst_ = os.path.abspath(os.path.join(
        inst_dir, _DecorateFilename(_CHROME_DLL, 'inst')))
    self.chrome_dll_pdb_inst_ = os.path.abspath(os.path.join(
        inst_dir, _DecorateFilename(_CHROME_DLL + '.pdb', 'inst')))

    # Only generate the instrumented binaries if they don't exist.
    if (os.path.isfile(self.chrome_dll_inst_) and
        os.path.isfile(self.chrome_dll_pdb_inst_)):
      _LOGGER.debug('Using existing instrumented binaries.')
    else:
      self._ProduceInstrumentedBinaries()

    # Replace the original chrome binaries with the instrumented versions.
    _RestoreFile(self.chrome_dll_, self.chrome_dll_inst_)
    _RestoreFile(self.chrome_dll_pdb_, self.chrome_dll_pdb_inst_)

    # Copy the runtime library into the Chrome directory.
    syzyasan_rtl = os.path.abspath(os.path.join(self.syzygy_dir_,
                                                _SYZYASAN_RTL))
    self.syzyasan_rtl_ = os.path.abspath(os.path.join(self.chrome_dir_,
                                                      _SYZYASAN_RTL))
    _RestoreFile(self.syzyasan_rtl_, syzyasan_rtl)

    return self

  def __exit__(self, *args, **kwargs):
    # Do nothing if instrumentation is to be skipped entirely.
    if self.skip_instrumentation_:
      return

    # Remove the RTL and restore the original Chrome binaries.
    os.remove(self.syzyasan_rtl_)
    _RestoreFile(self.chrome_dll_, self.chrome_dll_bak_)
    _RestoreFile(self.chrome_dll_pdb_, self.chrome_dll_pdb_bak_)


def Main():
  options = _ParseCommandLine()

  # Generate a temporary directory for use in the tests.
  with kasko.util.ScopedTempDir() as temp_dir:
    # Prevent the temporary directory from self cleaning if requested.
    if options.keep_temp_dirs:
      temp_dir_path = temp_dir.release()
    else:
      temp_dir_path = temp_dir.path

    # Use the specified user data directory if requested.
    if options.user_data_dir:
      user_data_dir = options.user_data_dir
    else:
      user_data_dir = os.path.join(temp_dir_path, 'user-data-dir')

    kasko_dir = os.path.join(temp_dir_path, 'kasko')
    os.makedirs(kasko_dir)

    # Launch the test server.
    server = kasko.crash_server.CrashServer()
    with kasko.util.ScopedStartStop(server):
      _LOGGER.info('Started server on port %d', server.port)

      # Configure the environment so Chrome can find the test crash server.
      os.environ['KASKO_CRASH_SERVER_URL'] = (
          'http://127.0.0.1:%d/crash' % server.port)

      # Configure the environment to disable feature randomization, which can
      # result in Kasko being randomly disabled. Append to any existing options.
      k = 'SYZYGY_ASAN_OPTIONS'
      v = '--disable_feature_randomization'
      if k in os.environ:
        os.environ[k] += ' ' + v
      else:
        os.environ[k] = v

      # SyzyAsan instrument the Chrome installation.
      chrome_dir = os.path.dirname(options.chrome)
      with _ScopedInstrumentedChrome(chrome_dir, options.syzygy_dir,
          temp_dir_path, instrumented_dir=options.instrumented_dir,
          verbose=(options.log_level == logging.DEBUG),
          skip_instrumentation=options.skip_instrumentation) as asan_chrome:
        # Launch Chrome and navigate it to the test URL.
        chrome = kasko.process.ChromeInstance(options.chromedriver,
                                              options.chrome, user_data_dir)
        with kasko.util.ScopedStartStop(chrome):
          _LOGGER.info('Navigating to SyzyAsan debug URL')
          chrome.navigate_to('chrome://crash/browser-use-after-free')

          _LOGGER.info('Waiting for Kasko report')
          if not server.wait_for_report(10):
            raise Exception('No Kasko report received.')

    report = server.crash(0)
    kasko.report.LogCrashKeys(report)
    kasko.report.ValidateCrashReport(report, {'asan-error-type': 'SyzyAsan'})

    _LOGGER.info('Test passed successfully!')

    return 0


if __name__ == '__main__':
  sys.exit(Main())
