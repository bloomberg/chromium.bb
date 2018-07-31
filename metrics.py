#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import functools
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import urllib2

import detect_host_arch
import gclient_utils
import metrics_utils


DEPOT_TOOLS = os.path.dirname(os.path.abspath(__file__))
CONFIG_FILE = os.path.join(DEPOT_TOOLS, 'metrics.cfg')
UPLOAD_SCRIPT = os.path.join(DEPOT_TOOLS, 'upload_metrics.py')

DISABLE_METRICS_COLLECTION = os.environ.get('DEPOT_TOOLS_METRICS') == '0'
DEFAULT_COUNTDOWN = 10

INVALID_CONFIG_WARNING = (
    'WARNING: Your metrics.cfg file was invalid or nonexistent. A new one has '
    'been created'
)


class _Config(object):
  def __init__(self):
    self._initialized = False
    self._config = {}

  def _ensure_initialized(self):
    if self._initialized:
      return

    try:
      config = json.loads(gclient_utils.FileRead(CONFIG_FILE))
    except (IOError, ValueError):
      config = {}

    self._config = config.copy()

    if 'is-googler' not in self._config:
      # /should-upload is only accessible from Google IPs, so we only need to
      # check if we can reach the page. An external developer would get access
      # denied.
      try:
        req = urllib2.urlopen(metrics_utils.APP_URL + '/should-upload')
        self._config['is-googler'] = req.getcode() == 200
      except (urllib2.URLError, urllib2.HTTPError):
        self._config['is-googler'] = False

    # Make sure the config variables we need are present, and initialize them to
    # safe values otherwise.
    self._config.setdefault('countdown', DEFAULT_COUNTDOWN)
    self._config.setdefault('opt-in', None)

    if config != self._config:
      print INVALID_CONFIG_WARNING
      self._write_config()

    self._initialized = True

  def _write_config(self):
    gclient_utils.FileWrite(CONFIG_FILE, json.dumps(self._config))

  @property
  def is_googler(self):
    self._ensure_initialized()
    return self._config['is-googler']

  @property
  def opted_in(self):
    self._ensure_initialized()
    return self._config['opt-in']

  @opted_in.setter
  def opted_in(self, value):
    self._ensure_initialized()
    self._config['opt-in'] = value
    self._write_config()

  @property
  def countdown(self):
    self._ensure_initialized()
    return self._config['countdown']

  def decrease_countdown(self):
    self._ensure_initialized()
    if self.countdown == 0:
      return
    self._config['countdown'] -= 1
    self._write_config()


class MetricsCollector(object):
  def __init__(self):
    self._metrics_lock = threading.Lock()
    self._reported_metrics = {}
    self._config = _Config()
    self._collecting_metrics = False
    self._collect_custom_metrics = True

  @property
  def config(self):
    return self._config

  @property
  def collecting_metrics(self):
    return self._collecting_metrics

  def add(self, name, value):
    if self._collect_custom_metrics:
      with self._metrics_lock:
        self._reported_metrics[name] = value

  @contextlib.contextmanager
  def pause_metrics_collection(self):
    collect_custom_metrics = self._collect_custom_metrics
    self._collect_custom_metrics = False
    try:
      yield
    finally:
      self._collect_custom_metrics = collect_custom_metrics

  def _upload_metrics_data(self):
    """Upload the metrics data to the AppEngine app."""
    # We invoke a subprocess, and use stdin.write instead of communicate(),
    # so that we are able to return immediately, leaving the upload running in
    # the background.
    p = subprocess.Popen([sys.executable, UPLOAD_SCRIPT], stdin=subprocess.PIPE)
    p.stdin.write(json.dumps(self._reported_metrics))

  def _collect_metrics(self, func, command_name, *args, **kwargs):
    # If we're already collecting metrics, just execute the function.
    # e.g. git-cl split invokes git-cl upload several times to upload each
    # splitted CL.
    if self.collecting_metrics:
      # Don't collect metrics for this function.
      # e.g. Don't record the arguments git-cl split passes to git-cl upload.
      with self.pause_metrics_collection():
        return func(*args, **kwargs)

    self._collecting_metrics = True
    self.add('command', command_name)
    try:
      start = time.time()
      result = func(*args, **kwargs)
      exception = None
    # pylint: disable=bare-except
    except:
      exception = sys.exc_info()
    finally:
      self.add('execution_time', time.time() - start)

    exit_code = metrics_utils.return_code_from_exception(exception)
    self.add('exit_code', exit_code)

    # Add metrics regarding environment information.
    self.add('timestamp', metrics_utils.seconds_to_weeks(time.time()))
    self.add('python_version', metrics_utils.get_python_version())
    self.add('host_os', gclient_utils.GetMacWinOrLinux())
    self.add('host_arch', detect_host_arch.HostArch())
    self.add('depot_tools_age', metrics_utils.get_repo_timestamp(DEPOT_TOOLS))

    self._upload_metrics_data()
    if exception:
      raise exception[0], exception[1], exception[2]
    return result

  def collect_metrics(self, command_name):
    """A decorator used to collect metrics over the life of a function.

    This decorator executes the function and collects metrics about the system
    environment and the function performance.
    """
    def _decorator(func):
      # Do this first so we don't have to read, and possibly create a config
      # file.
      if DISABLE_METRICS_COLLECTION:
        return func
      # Don't collect the metrics unless the user is a googler, the user has
      # opted in, or the countdown has expired.
      if (not self.config.is_googler or self.config.opted_in == False
          or (self.config.opted_in is None and self.config.countdown > 0)):
        return func
      # Otherwise, collect the metrics.
      # Needed to preserve the __name__ and __doc__ attributes of func.
      @functools.wraps(func)
      def _inner(*args, **kwargs):
        return self._collect_metrics(func, command_name, *args, **kwargs)
      return _inner
    return _decorator

  @contextlib.contextmanager
  def print_notice_and_exit(self):
    """A context manager used to print the notice and terminate execution.

    This decorator executes the function and prints the monitoring notice if
    necessary. If an exception is raised, we will catch it, and print it before
    printing the metrics collection notice.
    This will call sys.exit() with an appropriate exit code to ensure the notice
    is the last thing printed."""
    # Needed to preserve the __name__ and __doc__ attributes of func.
    try:
      yield
      exception = None
    # pylint: disable=bare-except
    except:
      exception = sys.exc_info()

    # Print the exception before the metrics notice, so that the notice is
    # clearly visible even if gclient fails.
    if exception:
      if isinstance(exception[1], KeyboardInterrupt):
        sys.stderr.write('Interrupted\n')
      elif not isinstance(exception[1], SystemExit):
        traceback.print_exception(*exception)

    # Print the notice
    if (not DISABLE_METRICS_COLLECTION and self.config.is_googler
        and self.config.opted_in is None):
      metrics_utils.print_notice(self.config.countdown)
      self.config.decrease_countdown()

    sys.exit(metrics_utils.return_code_from_exception(exception))


collector = MetricsCollector()
