# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for inframon's command-line flag based configuration."""

from __future__ import print_function

import argparse

from chromite.lib import cros_logging as logging

from infra_libs.ts_mon import config
import googleapiclient.discovery


# google-api-client has too much noisey logging.
googleapiclient.discovery.logger.setLevel(logging.WARNING)


def SetupTsMonGlobalState():
  """Uses a dummy argument parser to get the default behavior from ts-mon."""
  parser = argparse.ArgumentParser()
  config.add_argparse_options(parser)
  config.process_argparse_options(parser.parse_args(args=[]))
