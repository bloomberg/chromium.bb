# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configures devil for use in chromium."""

import logging
import os
import sys

from pylib.constants import host_paths

if host_paths.DEVIL_PATH not in sys.path:
  sys.path.append(host_paths.DEVIL_PATH)

from devil import devil_env


def Initialize(output_directory=None, custom_deps=None):
  """Initializes devil with chromium's binaries and third-party libraries.

  This includes:
    - Libraries:
      - the android SDK ("android_sdk")
      - pymock ("pymock")
    - Build products:
      - host & device forwarder binaries
          ("forwarder_device" and "forwarder_host")
      - host & device md5sum binaries ("md5sum_device" and "md5sum_host")

  Args:
    output_directory: An optional path to the output directory. If not set,
      no built dependencies are configured.
    custom_deps: An optional dictionary specifying custom dependencies.
      This should be of the form:

        {
          'dependency_name': {
            'platform': 'path',
            ...
          },
          ...
        }
  """
  config_files = None
  if output_directory:
    generated_config_file = os.path.abspath(os.path.join(
        output_directory, 'gen', 'devil_chromium.json'))
    if os.path.exists(generated_config_file):
      config_files = [generated_config_file]
    else:
      logging.warning('%s not found in output directory.',
                      generated_config_file)

  custom_config = {
    'config_type': 'BaseConfig',
    'dependencies': {},
  }
  if custom_deps:
    custom_config['dependencies'].update(custom_deps)

  devil_env.config.Initialize(
      configs=[custom_config], config_files=config_files)

