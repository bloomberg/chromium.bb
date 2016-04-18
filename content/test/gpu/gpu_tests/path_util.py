# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(
      os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, os.pardir))


def GetGpuTestDir():
  return os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def AddDirToPathIfNeeded(*path_parts):
  path = os.path.abspath(os.path.join(*path_parts))
  if os.path.isdir(path) and path not in sys.path:
    sys.path.append(path)


def SetupTelemetryPaths():
  chromium_src_dir = GetChromiumSrcDir()

  perf_path = os.path.join(chromium_src_dir, 'tools', 'perf')
  absolute_perf_path = os.path.abspath(perf_path)

  sys.path.append(absolute_perf_path)
  from chrome_telemetry_build import chromium_config

  telemetry_path = chromium_config.GetTelemetryDir()
  if telemetry_path not in sys.path:
    sys.path.append(telemetry_path)

  catapult_base_path = os.path.join(
      chromium_src_dir, 'third_party', 'catapult', 'catapult_base')
  if catapult_base_path not in sys.path:
    sys.path.append(catapult_base_path)
