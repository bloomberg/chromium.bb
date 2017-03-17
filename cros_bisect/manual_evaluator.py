# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Asks users if the commit is good or bad."""

from __future__ import print_function

import os

from chromite.cros_bisect import common
from chromite.cros_bisect import evaluator
from chromite.lib import cros_build_lib
from chromite.lib import osutils


class ManualEvaluator(evaluator.Evaluator):
  """Manual evaluator."""

  def __init__(self, options):
    super(ManualEvaluator, self).__init__(options)

  def Evaluate(self, unused_remote, build_label, unused_repeat):
    """Prompts user if the build is good or bad.

    Args:
      unused_remote: Unused args.
      build_label: Build label used for part of report filename and log message.
      unused_repeat: Unused args.

    Returns:
      Score([1.0]) if it is a good build. Otherwise, Score([0.0]).
    """
    result_path = os.path.join(self.report_base_dir,
                               'manual_%s.report' % build_label)
    prompt = 'Is %s a good build on the DUT?' % build_label
    is_good = cros_build_lib.BooleanPrompt(prompt=prompt)
    osutils.WriteFile(result_path,
                      '%s %s\n' % (prompt, 'Yes' if is_good else 'No'))

    score = 1.0 if is_good else 0.0
    return common.Score([score])
