# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configures devil for use in chromium."""

import os

from devil import devil_env


_DEVIL_CONFIG = os.path.abspath(
    os.path.join(os.path.dirname(__file__), 'devil_chromium.json'))

_DEVIL_BUILD_PRODUCT_DEPS = {
  'forwarder_device': {
    'armeabi-v7a': 'forwarder_dist',
    'arm64-v8a': 'forwarder_dist',
  },
  'forwarder_host': {
    'any': 'host_forwarder',
  },
  'md5sum_device': {
    'armeabi-v7a': 'md5sum_dist',
    'arm64-v8a': 'md5sum_dist',
  },
  'md5sum_host': {
    'any': 'md5sum_bin_host',
  },
}


def Initialize(output_directory=None):
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
  """

  devil_dynamic_deps = {}

  if output_directory:
    for dep_name, arch_dict in _DEVIL_BUILD_PRODUCT_DEPS.iteritems():
      devil_dynamic_deps[dep_name] = {}
      for arch, name in arch_dict.iteritems():
        devil_dynamic_deps[dep_name][arch] = os.path.join(
            output_directory, name)

  devil_dynamic_config = {
    'config_type': 'BaseConfig',
    'dependencies': {
      dep_name: {
        'file_info': {
          'android_%s' % arch: {
            'local_paths': [path]
          }
          for arch, path in arch_dict.iteritems()
        }
      }
      for dep_name, arch_dict in devil_dynamic_deps.iteritems()
    }
  }

  devil_env.config.Initialize(
      configs=[devil_dynamic_config], config_files=[_DEVIL_CONFIG])

