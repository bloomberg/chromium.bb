# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class of performance evaluators.

Evaluator should run specific tests on DUT to evaluate performance.
Evaluator should define methods: Evaluate() and CheckLastEvaluate().
Evaluator base class sets up data members: base_dir and report_base_dir. The
latter one is derived from base_dir, and it is created if it doesn't exist yet.
"""

from __future__ import print_function

import os

from chromite.cros_bisect import common
from chromite.lib import osutils


class Evaluator(common.OptionsChecker):
  """Base class of performance evaluator."""

  REQUIRED_ARGS = ('base_dir', )

  def __init__(self, options):
    """Constructor.

    Args:
      options: An argparse.Namespace to hold command line arguments. Should
        contain:
        * base_dir: Parent directory of repository folder
    """
    super(Evaluator, self).__init__(options)
    self.base_dir = options.base_dir
    self.report_base_dir = os.path.join(self.base_dir, 'reports')
    osutils.SafeMakedirs(self.report_base_dir)

  def Evaluate(self, remote, build_label, repeat):
    """Evaluates performance.

    Args:
      remote: DUT to evaluate (refer lib.commandline.Device).
      build_label: Build label used for part of report filename and log message.
      repeat: run the test N times.

    Returns:
      Score object. Empty Score object if it fails to evaluate.
    """

  def CheckLastEvaluate(self, build_label, repeat=1):
    """Checks if previous evaluate report is available.

    Args:
      build_label: Build label used for part of report filename and log message.
      repeat: Run test for N times. Default 1.

    Returns:
      Score object stores a list of autotest running results if report
      available and reuse_eval is set.
      Score() otherwise.
    """
