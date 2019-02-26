# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import sys
import tempfile
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'gyp'))

from util import build_utils
from util import md5_check
import bundletool

# List of valid modes for GenerateBundleApks()
BUILD_APKS_MODES = ('default', 'universal', 'system', 'system_compressed')

_ALL_ABIS = ['armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64']


def _CreateMinimalDeviceSpec(bundle_path, sdk_version):
  # Could also use "bundletool dump resources", but reading directly is faster.
  if not sdk_version:
    with zipfile.ZipFile(bundle_path) as f:
      manifest_data = f.read('base/manifest/AndroidManifest.xml')
    sdk_version = int(
        re.search(r'minSdkVersion.*?(\d+)', manifest_data).group(1))

  # Measure with one language split installed. Use Hindi because it is popular.
  # resource_size.py looks for splits/base-hi.apk.
  # Note: English is always included since it's in base-master.apk.
  locales = ['hi']
  # Setting sdkVersion=minSdkVersion prevents multiple per-minSdkVersion .apk
  # files from being created within the .apks file.
  return {
      'screenDensity': 1000,  # Ignored since we don't split on density.
      'sdkVersion': sdk_version,
      'supportedAbis': _ALL_ABIS,  # Our .aab files are already split on abi.
      'supportedLocales': locales,
  }


def GenerateBundleApks(bundle_path,
                       bundle_apks_path,
                       aapt2_path,
                       keystore_path,
                       keystore_password,
                       keystore_alias,
                       mode=None,
                       minimal=False,
                       minimal_sdk_version=None,
                       check_for_noop=True):
  """Generate an .apks archive from a an app bundle if needed.

  Args:
    bundle_path: Input bundle file path.
    bundle_apks_path: Output bundle .apks archive path. Name must end with
      '.apks' or this operation will fail.
    aapt2_path: Path to aapt2 build tool.
    keystore_path: Path to keystore.
    keystore_password: Keystore password, as a string.
    keystore_alias: Keystore signing key alias.
    mode: Build mode, which must be either None or one of BUILD_APKS_MODES.
    minimal: Create the minimal set of apks possible (english-only).
    minimal_sdk_version: When minimal=True, use this sdkVersion.
    check_for_noop: Use md5_check to short-circuit when inputs have not changed.
  """
  device_spec = None
  if minimal:
    device_spec = _CreateMinimalDeviceSpec(bundle_path, minimal_sdk_version)

  def rebuild():
    logging.info('Building %s', bundle_apks_path)
    with tempfile.NamedTemporaryFile(suffix='.json') as spec_file, \
        build_utils.AtomicOutput(bundle_apks_path, only_if_changed=False) as f:
      cmd_args = [
          'build-apks',
          '--aapt2=%s' % aapt2_path,
          '--output=%s' % f.name,
          '--bundle=%s' % bundle_path,
          '--ks=%s' % keystore_path,
          '--ks-pass=pass:%s' % keystore_password,
          '--ks-key-alias=%s' % keystore_alias,
          '--overwrite',
      ]
      if device_spec:
        json.dump(device_spec, spec_file)
        spec_file.flush()
        cmd_args += ['--device-spec=' + spec_file.name]
      if mode is not None:
        if mode not in BUILD_APKS_MODES:
          raise Exception('Invalid mode parameter %s (should be in %s)' %
                          (mode, BUILD_APKS_MODES))
        cmd_args += ['--mode=' + mode]
      bundletool.RunBundleTool(cmd_args)

  if check_for_noop:
    # NOTE: BUNDLETOOL_JAR_PATH is added to input_strings, rather than
    # input_paths, to speed up MD5 computations by about 400ms (the .jar file
    # contains thousands of class files which are checked independently,
    # resulting in an .md5.stamp of more than 60000 lines!).
    input_paths = [bundle_path, aapt2_path, keystore_path]
    input_strings = [
        keystore_password,
        keystore_alias,
        bundletool.BUNDLETOOL_JAR_PATH,
        # NOTE: BUNDLETOOL_VERSION is already part of BUNDLETOOL_JAR_PATH, but
        # it's simpler to assume that this may not be the case in the future.
        bundletool.BUNDLETOOL_VERSION,
        device_spec,
    ]
    if mode is not None:
      input_strings.append(mode)

    md5_check.CallAndRecordIfStale(
        rebuild,
        input_paths=input_paths,
        input_strings=input_strings,
        output_paths=[bundle_apks_path])
  else:
    rebuild()
