# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def RunTests(tests, runner_factory):
  """Runs a set of java tests on the host.

  Return:
    A tuple containing the results & the exit code.
  """
  def run(t):
    runner = runner_factory(None, None)
    runner.SetUp()
    result = runner.RunTest(t)
    runner.TearDown()
    return result == 0

  return (None, 0 if all(run(t) for t in tests) else 1)

