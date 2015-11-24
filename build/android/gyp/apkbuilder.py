#!/usr/bin/env python
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Adds the code parts to a resource APK."""

import argparse
import itertools
import os
import shutil
import sys
import zipfile

from util import build_utils


# Taken from aapt's Package.cpp:
_NO_COMPRESS_EXTENSIONS = ('.jpg', '.jpeg', '.png', '.gif', '.wav', '.mp2',
                           '.mp3', '.ogg', '.aac', '.mpg', '.mpeg', '.mid',
                           '.midi', '.smf', '.jet', '.rtttl', '.imy', '.xmf',
                           '.mp4', '.m4a', '.m4v', '.3gp', '.3gpp', '.3g2',
                           '.3gpp2', '.amr', '.awb', '.wma', '.wmv')


def _ParseArgs(args):
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--assets',
                      help='GYP-list of files to add as assets in the form '
                           '"srcPath:zipPath", where ":zipPath" is optional.',
                      default='[]')
  parser.add_argument('--write-asset-list',
                      action='store_true',
                      help='Whether to create an assets/assets_list file.')
  parser.add_argument('--uncompressed-assets',
                      help='Same as --assets, except disables compression.',
                      default='[]')
  parser.add_argument('--resource-apk',
                      help='An .ap_ file built using aapt',
                      required=True)
  parser.add_argument('--output-apk',
                      help='Path to the output file',
                      required=True)
  parser.add_argument('--dex-file',
                      help='Path to the classes.dex to use')
  # TODO(agrieve): Switch this to be a list of files rather than a directory.
  parser.add_argument('--native-libs-dir',
                      help='Directory containing native libraries to include',
                      default=[])
  parser.add_argument('--android-abi',
                      help='Android architecture to use for native libraries')
  parser.add_argument('--native-lib-placeholders',
                      help='GYP-list of native library placeholders to add.',
                      default='[]')
  parser.add_argument('--emma-device-jar',
                      help='Path to emma_device.jar to include.')
  options = parser.parse_args(args)
  options.assets = build_utils.ParseGypList(options.assets)
  options.uncompressed_assets = build_utils.ParseGypList(
      options.uncompressed_assets)
  options.native_lib_placeholders = build_utils.ParseGypList(
      options.native_lib_placeholders)

  if not options.android_abi and (options.native_libs_dir or
                                  options.native_lib_placeholders):
    raise Exception('Must specify --android-abi with --native-libs-dir')
  return options


def _ListSubPaths(path):
  """Returns a list of full paths to all files in the given path."""
  return [os.path.join(path, name) for name in os.listdir(path)]


def _SplitAssetPath(path):
  """Returns (src, dest) given an asset path in the form src[:dest]."""
  path_parts = path.split(':')
  src_path = path_parts[0]
  if len(path_parts) > 1:
    dest_path = path_parts[1]
  else:
    dest_path = os.path.basename(src_path)
  return src_path, dest_path


def _AddAssets(apk, paths, disable_compression=False):
  """Adds the given paths to the apk.

  Args:
    apk: ZipFile to write to.
    paths: List of paths (with optional :zipPath suffix) to add.
    disable_compression: Whether to disable compression.
  """
  # Group all uncompressed assets together in the hope that it will increase
  # locality of mmap'ed files.
  for target_compress in (False, True):
    for path in paths:
      src_path, dest_path = _SplitAssetPath(path)

      compress = not disable_compression and (
          os.path.splitext(src_path)[1] not in _NO_COMPRESS_EXTENSIONS)
      if target_compress == compress:
        apk_path = 'assets/' + dest_path
        try:
          apk.getinfo(apk_path)
          # Should never happen since write_build_config.py handles merging.
          raise Exception('Multiple targets specified the asset path: %s' %
                          apk_path)
        except KeyError:
          build_utils.AddToZipHermetic(apk, apk_path, src_path=src_path,
                                       compress=compress)


def _CreateAssetsList(paths):
  """Returns a newline-separated list of asset paths for the given paths."""
  return '\n'.join(_SplitAssetPath(p)[1] for p in sorted(paths)) + '\n'


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  native_libs = []
  if options.native_libs_dir:
    native_libs = _ListSubPaths(options.native_libs_dir)

  input_paths = [options.resource_apk, __file__] + native_libs
  if options.dex_file:
    input_paths.append(options.dex_file)

  if options.emma_device_jar:
    input_paths.append(options.emma_device_jar)

  input_strings = [options.android_abi, options.native_lib_placeholders]

  for path in itertools.chain(options.assets, options.uncompressed_assets):
    src_path, dest_path = _SplitAssetPath(path)
    input_paths.append(src_path)
    input_strings.append(dest_path)

  def on_stale_md5():
    tmp_apk = options.output_apk + '.tmp'
    try:
      # Use a temp file to avoid creating an output if anything goes wrong.
      shutil.copyfile(options.resource_apk, tmp_apk)

      # TODO(agrieve): It would be more efficient to combine this step
      # with finalize_apk(), which sometimes aligns and uncompresses the
      # native libraries.
      with zipfile.ZipFile(tmp_apk, 'a', zipfile.ZIP_DEFLATED) as apk:
        if options.write_asset_list:
          data = _CreateAssetsList(
              itertools.chain(options.assets, options.uncompressed_assets))
          build_utils.AddToZipHermetic(apk, 'assets/assets_list', data=data)

        _AddAssets(apk, options.assets, disable_compression=False)
        _AddAssets(apk, options.uncompressed_assets, disable_compression=True)

        for path in native_libs:
          basename = os.path.basename(path)
          apk_path = 'lib/%s/%s' % (options.android_abi, basename)
          build_utils.AddToZipHermetic(apk, apk_path, src_path=path)

        for name in options.native_lib_placeholders:
          # Make it non-empty so that its checksum is non-zero and is not
          # ignored by md5_check.
          apk_path = 'lib/%s/%s.so' % (options.android_abi, name)
          build_utils.AddToZipHermetic(apk, apk_path, data=':)')

        if options.dex_file and options.dex_file.endswith('.zip'):
          with zipfile.ZipFile(options.dex_file, 'r') as dex_zip:
            for dex in (d for d in dex_zip.namelist() if d.endswith('.dex')):
              build_utils.AddToZipHermetic(apk, dex, data=dex_zip.read(dex))
        elif options.dex_file:
          build_utils.AddToZipHermetic(apk, 'classes.dex',
                                       src_path=options.dex_file)

        if options.emma_device_jar:
          # Add EMMA Java resources to APK.
          with zipfile.ZipFile(options.emma_device_jar, 'r') as emma_device_jar:
            for apk_path in emma_device_jar.namelist():
              apk_path_lower = apk_path.lower()
              if apk_path_lower.startswith('meta-inf/'):
                continue

              if apk_path_lower.endswith('/'):
                continue

              if apk_path_lower.endswith('.class'):
                continue

              build_utils.AddToZipHermetic(apk, apk_path,
                                           data=emma_device_jar.read(apk_path))

      shutil.move(tmp_apk, options.output_apk)
    finally:
      if os.path.exists(tmp_apk):
        os.unlink(tmp_apk)

  build_utils.CallAndWriteDepfileIfStale(
      on_stale_md5,
      options,
      input_paths=input_paths,
      input_strings=input_strings,
      output_paths=[options.output_apk])


if __name__ == '__main__':
  main(sys.argv[1:])
