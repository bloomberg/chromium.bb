# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for inframon's command-line flag based configuration."""

from __future__ import print_function

import argparse
import contextlib
import multiprocessing
import os
import socket
import signal
import time
import Queue

from chromite.lib import cros_logging as logging
from chromite.lib import metrics
from chromite.lib import parallel

try:
  from infra_libs.ts_mon import config
  import googleapiclient.discovery
except (ImportError, RuntimeError) as e:
  config = None
  logging.warning('Failed to import ts_mon, monitoring is disabled: %s', e)


_WasSetup = False

FLUSH_INTERVAL = 60


@contextlib.contextmanager
def TrivialContextManager():
  """Context manager with no side effects."""
  yield


def SetupTsMonGlobalState(service_name,
                          short_lived=False,
                          indirect=False,
                          auto_flush=True,
                          debug_file=None,
                          suppress_exception=True):
  """Uses a dummy argument parser to get the default behavior from ts-mon.

  Args:
    service_name: The name of the task we are sending metrics from.
    short_lived: Whether this process is short-lived and should use the autogen
                 hostname prefix.
    indirect: Whether to create a metrics.METRICS_QUEUE object and a separate
              process for indirect metrics flushing. Useful for forking,
              because forking would normally create a duplicate ts_mon thread.
    auto_flush: Whether to create a thread to automatically flush metrics every
                minute.
    debug_file: If non-none, send metrics to this path instead of to PubSub.
    suppress_exception: True to silence any exception during the setup. Default
                        is set to True.
  """
  if not config:
    return TrivialContextManager()

  if indirect:
    return _CreateTsMonFlushingProcess([service_name],
                                       {'short_lived': short_lived,
                                        'debug_file': debug_file})

  # google-api-client has too much noisey logging.
  googleapiclient.discovery.logger.setLevel(logging.WARNING)
  parser = argparse.ArgumentParser()
  config.add_argparse_options(parser)
  args = [
      '--ts-mon-target-type', 'task',
      '--ts-mon-task-service-name', service_name,
      '--ts-mon-task-job-name', service_name,
  ]

  if debug_file:
    args.extend(['--ts-mon-endpoint', 'file://' + debug_file])

  # Short lived processes will have autogen: prepended to their hostname and
  # use task-number=PID to trigger shorter retention policies under
  # chrome-infra@, and used by a Monarch precomputation to group across the
  # task number.
  # Furthermore, we assume they manually call ts_mon.Flush(), because the
  # ts_mon thread will drop messages if the process exits before it flushes.
  if short_lived:
    auto_flush = False
    fqdn = socket.getfqdn().lower()
    host = fqdn.split('.')[0]
    args.extend(['--ts-mon-task-hostname', 'autogen:' + host,
                 '--ts-mon-task-number', str(os.getpid())])

  args.extend(['--ts-mon-flush', 'auto' if auto_flush else 'manual'])

  try:
    config.process_argparse_options(parser.parse_args(args=args))
    logging.notice('ts_mon was set up.')
    global _WasSetup  # pylint: disable=global-statement
    _WasSetup = True
  except Exception as e:
    logging.warning('Failed to configure ts_mon, monitoring is disabled: %s', e,
                    exc_info=True)
    if not suppress_exception:
      raise


  return TrivialContextManager()


@contextlib.contextmanager
def _CreateTsMonFlushingProcess(setup_args, setup_kwargs):
  """Creates a separate process to flush ts_mon metrics.

  Useful for multiprocessing scenarios where we don't want multiple ts-mon
  threads send contradictory metrics. Instead, functions in
  chromite.lib.metrics will send their calls to a Queue, which is consumed by a
  dedicated flushing process.

  Args:
    setup_args: Arguments sent to SetupTsMonGlobalState in the child process
    setup_kwargs: Keyword arguments sent to SetupTsMonGlobalState in the child
      process

  Side effects:
    Sets chromite.lib.metrics.MESSAGE_QUEUE, which causes the metric functions
    to send their calls to the Queue instead of creating the metrics.
  """
  # If this is nested, we don't need to create another queue and another
  # message consumer. Do nothing to continue to use the existing queue.
  if metrics.MESSAGE_QUEUE or metrics.FLUSHING_PROCESS:
    return

  with parallel.Manager() as manager:
    message_q = manager.Queue()

    metrics.FLUSHING_PROCESS = multiprocessing.Process(
        target=lambda: _ConsumeMessages(message_q, setup_args, setup_kwargs))
    metrics.FLUSHING_PROCESS.start()

    # this makes the chromite.lib.metric functions use the queue.
    # note - we have to do this *after* forking the ConsumeMessages process.
    metrics.MESSAGE_QUEUE = message_q

    try:
      yield message_q
    finally:
      _CleanupMetricsFlushingProcess()


def _CleanupMetricsFlushingProcess():
  """Sends sentinal value to flushing process and .joins it."""
  # Now that there is no longer a process to listen to the Queue, re-set it
  # to None so that any future metrics are created within this process.
  message_q = metrics.MESSAGE_QUEUE
  flushing_process = metrics.FLUSHING_PROCESS
  metrics.MESSAGE_QUEUE = None
  metrics.FLUSHING_PROCESS = None

  # If the process has already died, we don't need to try to clean it up.
  if not flushing_process.is_alive():
    return

  # Send the sentinal value for "flush one more time and exit".
  try:
    message_q.put(None)
  # If the flushing process quits, the message Queue can become full.
  except IOError:
    if not flushing_process.is_alive():
      return

  logging.info("Waiting for ts_mon flushing process to finish...")
  flushing_process.join(timeout=FLUSH_INTERVAL*2)
  if flushing_process.is_alive():
    flushing_process.terminate()
  if flushing_process.exitcode:
    logging.warning("ts_mon_config flushing process did not exit cleanly.")
  logging.info("Finished waiting for ts_mon process.")


def _WaitToFlush(last_flush, reset_after=()):
  """Sleeps until the next time we can call metrics.Flush(), then flushes.

  Args:
    last_flush: timestamp of the last flush
    reset_after: A list of metrics to reset after the flush.
  """
  time_delta = time.time() - last_flush
  time.sleep(max(0, FLUSH_INTERVAL - time_delta))
  metrics.Flush(reset_after=reset_after)


def _FlushIfReady(pending, last_flush, reset_after=()):
  """Call metrics.Flush() if we are ready and have pending metrics.

  This allows us to only call flush every FLUSH_INTERVAL seconds.

  Args:
    pending: bool indicating whether there are pending metrics to flush.
    last_flush: time stamp of the last time flush() was called.
    reset_after: A list of metrics to reset after the flush.
  """
  now = time.time()
  time_delta = now - last_flush
  if time_delta > FLUSH_INTERVAL and pending:
    last_flush = now
    time_delta = 0
    metrics.Flush(reset_after=reset_after)
    pending = False
  else:
    pending = True

  return pending, last_flush, time_delta


def _MethodCallRepr(obj, method, args, kwargs):
  """Gives a string representation of |obj|.|method|(*|args|, **|kwargs|)

  Args:
    obj: An object
    method: A method name
    args: A list of arguments
    kwargs: A dict of keyword arguments
  """
  args_strings = (map(repr, args) +
                  [(str(k) + '=' + repr(v))
                   for (k, v) in kwargs.iteritems()])
  return '%s.%s(%s)' % (repr(obj), method, ', '.join(args_strings))


def _ConsumeMessages(message_q, setup_args, setup_kwargs):
  """Configures ts_mon and gets metrics from a message queue.

  Args:
    message_q: A multiprocessing.Queue to read metrics from.
    setup_args: Arguments to pass to SetupTsMonGlobalState.
    setup_kwargs: Keyword arguments to pass to SetupTsMonGlobalState.
  """

  last_flush = 0
  pending = False

  # If our parent dies, finish flushing before exiting.
  reset_after = []
  if parallel.ExitWithParent(signal.SIGHUP):
    signal.signal(signal.SIGHUP,
                  lambda _sig, _stack: _WaitToFlush(last_flush,
                                                    reset_after=reset_after))

  # Configure ts-mon, but don't start up a sending thread.
  setup_kwargs['auto_flush'] = False
  SetupTsMonGlobalState(*setup_args, **setup_kwargs)

  message = message_q.get()
  while message:
    try:
      cls = getattr(metrics, message.metric_name)
      metric = cls(*message.metric_args, **message.metric_kwargs)
      if message.reset_after:
        reset_after.append(metric)
      getattr(metric, message.method)(
          *message.method_args,
          **message.method_kwargs)
    except Exception:
      logging.exception('Caught an exception while running %s',
                        _MethodCallRepr(message.metric_name,
                                        message.method,
                                        message.method_args,
                                        message.method_kwargs))

    pending, last_flush, time_delta = _FlushIfReady(True, last_flush,
                                                    reset_after=reset_after)

    try:
      # Only wait until the next flush time if we have pending metrics.
      timeout = FLUSH_INTERVAL - time_delta if pending else None
      message = message_q.get(timeout=timeout)
    except Queue.Empty:
      # We had pending metrics, but we didn't get a new message. Flush and wait
      # indefinitely.
      pending, last_flush, _ = _FlushIfReady(pending, last_flush,
                                             reset_after=reset_after)
      # Wait as long as we need to for the next metric.
      message = message_q.get()

  if pending:
    _WaitToFlush(last_flush, reset_after=reset_after)
