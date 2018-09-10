# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script only works on Windows with Intel CPU. Intel Power Gadget needs
to be installed on the machine before this script works. The software can be
downloaded from:
  https://software.intel.com/en-us/articles/intel-power-gadget-20
"""

from gpu_tests import gpu_integration_test
from gpu_tests import ipg_utils
from gpu_tests.gpu_test_expectations import GpuTestExpectations

import os
import sys

# There are no expectations for power_measurement
class PowerMeasurementExpectations(GpuTestExpectations):
  def SetExpectations(self):
    pass

class PowerMeasurementIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls):
    return 'power'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    parser.add_option("--duration", default=60, type="int",
                      help="specify how many seconds Intel Power Gadget "
                      "measures. By default, 60 seconds is selected.")
    parser.add_option("--delay", default=10, type="int",
                      help="specify how many seconds we skip in the data "
                      "Intel Power Gadget collects. This time is for starting "
                      "video play, switching to fullscreen mode, etc. "
                      "By default, 10 seconds is selected.")
    parser.add_option("--resolution", default=100, type="int",
                      help="specify how often Intel Power Gadget samples "
                      "data in milliseconds. By default, 100 ms is selected.")
    parser.add_option("--url",
                      help="specify the webpage URL the browser launches with.")
    parser.add_option("--fullscreen", action="store_true", default=False,
                      help="specify if the browser goes to fullscreen mode "
                      "automatically, specifically if there is a single video "
                      "element in the page, switch it to fullsrceen mode.")
    parser.add_option("--logdir",
                      help="Speficy where the Intel Power Gadget log file "
                      "should be stored. If specified, the log file name will "
                      "include a timestamp. If not specified, the log file "
                      "will be PowerLog.csv at the current dir and will be "
                      "overwritten at next run.")
    parser.add_option("--repeat", default=1, type="int",
                      help="specify how many times to repreat the measurement. "
                      "By default, measure only once. If measure more than "
                      "once, between each measurement, browser restarts.")
    parser.add_option("--outliers", default=0, type="int",
                      help="if a test is repeated multiples and outliers is "
                      "set to N, then N smallest results and N largest results "
                      "are discarded before computing mean and stdev.")

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('url', options.url, (options.repeat, options.outliers,
                                options.fullscreen, options.logdir,
                                options.duration, options.delay,
                                options.resolution))

  @classmethod
  def SetUpProcess(cls):
    super(cls, PowerMeasurementIntegrationTest).SetUpProcess()
    cls.CustomizeBrowserArgs([
      '--autoplay-policy=no-user-gesture-required'
    ])
    cls.StartBrowser()

  def RunActualGpuTest(self, test_path, *args):
    ipg_path = ipg_utils.LocateIPG()
    if not ipg_path:
      self.fail("Fail to locate Intel Power Gadget")

    repeat = args[0]
    outliers = args[1]
    fullscreen = args[2]
    ipg_logdir = args[3]
    ipg_duration = args[4]
    ipg_delay = args[5]
    ipg_resolution = args[6]

    print ""
    print "Total iterations: ", repeat
    logfiles = []
    for iteration in range(repeat):
      run_label = "Iteration_%d" % iteration
      print run_label
      if test_path:
        self.tab.action_runner.Navigate(test_path)

      if fullscreen:
        # TODO(zmo): implement fullscreen mode.
        pass

      logfile = None
      if ipg_logdir:
        if not os.path.isdir(ipg_logdir):
          self.fail("Folder " + ipg_logdir + " doesn't exist")
        logfile = ipg_utils.GenerateIPGLogFilename(log_dir=ipg_logdir,
                                                   timestamp=True)
      ipg_utils.RunIPG(ipg_duration + ipg_delay, ipg_resolution, logfile)
      logfiles.append(logfile)

      if repeat > 1 and iteration < repeat - 1:
        self.StopBrowser()
        self.StartBrowser()

    if repeat == 1:
      results = ipg_utils.AnalyzeIPGLogFile(logfiles[0], ipg_delay)
      print "Results: ", results
    else:
      json_path = None
      if ipg_logdir:
        json_path = os.path.join(ipg_logdir, "output.json")
        print "Results saved in ", json_path

      summary = ipg_utils.ProcessResultsFromMultipleIPGRuns(
        logfiles, ipg_delay, outliers, json_path)
      print 'Summary: ', summary

  @classmethod
  def _CreateExpectations(cls):
    return PowerMeasurementExpectations()

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
