# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for inframon's command-line flag based configuration."""

from __future__ import print_function

import argparse

from chromite.lib import cros_logging as logging

try:
  from infra_libs.ts_mon import config
  import googleapiclient.discovery
except (ImportError, RuntimeError) as e:
  config = None
  logging.warning('Failed to import ts_mon, monitoring is disabled: %s', e)


def SetupTsMonGlobalState(service_name):
  """Uses a dummy argument parser to get the default behavior from ts-mon.

  Args:
    service_name: the name of the task we are sending metrics from.
  """
  if not config:
    return

  # google-api-client has too much noisey logging.
  googleapiclient.discovery.logger.setLevel(logging.WARNING)
  parser = argparse.ArgumentParser()
  config.add_argparse_options(parser)
  try:
    config.process_argparse_options(parser.parse_args(args=[
        '--ts-mon-target-type', 'task',
        '--ts-mon-task-service-name', service_name,
        '--ts-mon-task-job-name', service_name,
    ]))
  except Exception as e:
    logging.warning('Failed to configure ts_mon, monitoring is disabled: %s', e,
                    exc_info=True)
