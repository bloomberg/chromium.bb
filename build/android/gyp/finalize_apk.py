#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Signs and zipaligns APK.

"""

import optparse
import shutil
import sys
import tempfile

from util import build_utils

def RenameLibInApk(rezip_path, in_zip_file, out_zip_file):
  rename_cmd = [
      rezip_path,
      'rename',
      in_zip_file,
      out_zip_file,
    ]
  build_utils.CheckOutput(rename_cmd)


def SignApk(key_path, key_name, key_passwd, unsigned_path, signed_path):
  shutil.copy(unsigned_path, signed_path)
  sign_cmd = [
      'jarsigner',
      '-sigalg', 'MD5withRSA',
      '-digestalg', 'SHA1',
      '-keystore', key_path,
      '-storepass', key_passwd,
      signed_path,
      key_name,
    ]
  build_utils.CheckOutput(sign_cmd)


def AlignApk(zipalign_path, unaligned_path, final_path):
  align_cmd = [
      zipalign_path,
      '-f', '4',  # 4 bytes
      unaligned_path,
      final_path,
      ]
  build_utils.CheckOutput(align_cmd)


def UncompressLibAndPageAlignInApk(rezip_path, in_zip_file, out_zip_file):
  rename_cmd = [
      rezip_path,
      'inflatealign',
      in_zip_file,
      out_zip_file,
    ]
  build_utils.CheckOutput(rename_cmd)


def DropDataDescriptorsInApk(rezip_path, in_zip_file, out_zip_file):
  rename_cmd = [
      rezip_path,
      'dropdescriptors',
      in_zip_file,
      out_zip_file,
    ]
  build_utils.CheckOutput(rename_cmd)


def main():
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

  parser.add_option('--zipalign-path', help='Path to the zipalign tool.')
  parser.add_option('--rezip-path', help='Path to the rezip executable.')
  parser.add_option('--unsigned-apk-path', help='Path to input unsigned APK.')
  parser.add_option('--final-apk-path',
      help='Path to output signed and aligned APK.')
  parser.add_option('--key-path', help='Path to keystore for signing.')
  parser.add_option('--key-passwd', help='Keystore password')
  parser.add_option('--key-name', help='Keystore name')
  parser.add_option('--stamp', help='Path to touch on success.')
  parser.add_option('--load-library-from-zip-file', type='int',
      help='If non-zero, build the APK such that the library can be loaded ' +
           'directly from the zip file using the crazy linker. The library ' +
           'will be renamed, uncompressed and page aligned.')

  options, _ = parser.parse_args()

  with tempfile.NamedTemporaryFile() as signed_apk_path_tmp, \
      tempfile.NamedTemporaryFile() as apk_to_sign_tmp, \
      tempfile.NamedTemporaryFile() as apk_without_descriptors_tmp, \
      tempfile.NamedTemporaryFile() as aligned_apk_tmp:

    if options.load_library_from_zip_file:
      # We alter the name of the library so that the Android Package Manager
      # does not extract it into a separate file. This must be done before
      # signing, as the filename is part of the signed manifest.
      apk_to_sign = apk_to_sign_tmp.name
      RenameLibInApk(options.rezip_path, options.unsigned_apk_path, apk_to_sign)
    else:
      apk_to_sign = options.unsigned_apk_path

    signed_apk_path = signed_apk_path_tmp.name
    SignApk(options.key_path, options.key_name, options.key_passwd,
            apk_to_sign, signed_apk_path)

    if options.load_library_from_zip_file:
      # Signing adds data descriptors to the APK. These are redundant
      # information. We remove them as otherwise they can cause a
      # miscalculation in the page alignment.
      apk_to_align = apk_without_descriptors_tmp.name
      DropDataDescriptorsInApk(
          options.rezip_path, signed_apk_path, apk_to_align)
      aligned_apk = aligned_apk_tmp.name
    else:
      apk_to_align = signed_apk_path
      aligned_apk = options.final_apk_path

    # Align uncompress items to 4 bytes
    AlignApk(options.zipalign_path, apk_to_align, aligned_apk)

    if options.load_library_from_zip_file:
      # Uncompress the library and make sure that it is page aligned.
      UncompressLibAndPageAlignInApk(
          options.rezip_path, aligned_apk, options.final_apk_path)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile, build_utils.GetPythonDependencies())

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main())
