# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A library for Chrome GPU test code."""
import os
import sys

def Init():
  chromium_src_dir = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..'))

  telemetry_path = os.path.join(chromium_src_dir, 'tools', 'telemetry')
  if telemetry_path not in sys.path:
    sys.path.append(telemetry_path)

  catapult_base_path = os.path.join(
      chromium_src_dir, 'third_party', 'catapult', 'catapult_base')
  if catapult_base_path not in sys.path:
    sys.path.append(catapult_base_path)

Init()
