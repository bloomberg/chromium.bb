#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Script to help uploading and downloading the Google Play services client
library to and from a Google Cloud storage.
'''

import argparse
import collections
import logging
import os
import re
import shutil
import sys
import tempfile
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
from devil.utils import cmd_helper
from play_services import utils
from pylib import constants
from pylib.utils import logging_utils

sys.path.append(os.path.join(constants.DIR_SOURCE_ROOT, 'build'))
import find_depot_tools  # pylint: disable=import-error,unused-import
import breakpad
import download_from_google_storage
import upload_to_google_storage


# Directory where the SHA1 files for the zip and the license are stored
# It should be managed by git to provided information about new versions.
SHA1_DIRECTORY = os.path.join(constants.DIR_SOURCE_ROOT, 'build', 'android',
                              'play_services')

# Default bucket used for storing the files.
GMS_CLOUD_STORAGE = 'chrome-sdk-extras'

# Path to the default configuration file. It exposes the currently installed
# version of the library in a human readable way.
CONFIG_DEFAULT_PATH = os.path.join(constants.DIR_SOURCE_ROOT, 'build',
                                   'android', 'play_services', 'config.json')

LICENSE_FILE_NAME = 'LICENSE'
LIBRARY_FILE_NAME = 'google_play_services_library.zip'
GMS_PACKAGE_ID = 'extra-google-google_play_services'  # used by sdk manager

LICENSE_PATTERN = re.compile(r'^Pkg\.License=(?P<text>.*)$', re.MULTILINE)


def Main():
  parser = argparse.ArgumentParser(
      description=__doc__ + 'Please see the subcommand help for more details.',
      formatter_class=utils.DefaultsRawHelpFormatter)
  subparsers = parser.add_subparsers(title='commands')

  # Download arguments
  parser_download = subparsers.add_parser(
      'download',
      help='download the library from the cloud storage',
      description=Download.__doc__,
      formatter_class=utils.DefaultsRawHelpFormatter)
  parser_download.add_argument('-f', '--force',
                               action='store_true',
                               help=('run even if the local version is '
                                     'already up to date'))
  parser_download.set_defaults(func=Download)
  AddCommonArguments(parser_download)

  # SDK Update arguments
  parser_sdk = subparsers.add_parser(
      'sdk',
      help='update the local sdk using the Android SDK Manager',
      description=UpdateSdk.__doc__,
      formatter_class=utils.DefaultsRawHelpFormatter)
  parser_sdk.add_argument('--sdk-root',
                          help=('base path to the Android SDK tools to use to '
                                'update the library'),
                          default=constants.ANDROID_SDK_ROOT)
  parser_sdk.add_argument('-v', '--verbose',
                          action='store_true',
                          help='print debug information')
  parser_sdk.set_defaults(func=UpdateSdk)

  # Upload arguments
  parser_upload = subparsers.add_parser(
      'upload',
      help='upload the library to the cloud storage',
      description=Upload.__doc__,
      formatter_class=utils.DefaultsRawHelpFormatter)
  parser_upload.add_argument('-f', '--force',
                             action='store_true',
                             help=('run even if the checked in version is '
                                   'already up to date'))
  parser_upload.add_argument('--sdk-root',
                             help=('base path to the Android SDK tools to use '
                                   'to update the library'),
                             default=constants.ANDROID_SDK_ROOT)
  parser_upload.add_argument('--skip-git',
                             action='store_true',
                             help="don't commit the changes at the end")
  parser_upload.set_defaults(func=Upload)
  AddCommonArguments(parser_upload)

  args = parser.parse_args()
  if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
  logging_utils.ColorStreamHandler.MakeDefault()
  return args.func(args)


def AddCommonArguments(parser):
  '''
  Defines the common arguments on subparser rather than the main one. This
  allows to put arguments after the command: `foo.py upload --debug --force`
  instead of `foo.py --debug upload --force`
  '''

  parser.add_argument('--bucket',
                      help='name of the bucket where the files are stored',
                      default=GMS_CLOUD_STORAGE)
  parser.add_argument('--config',
                      help='JSON Configuration file',
                      default=CONFIG_DEFAULT_PATH)
  parser.add_argument('--dry-run',
                      action='store_true',
                      help=('run the script in dry run mode. Files will be '
                            'copied to a local directory instead of the cloud '
                            'storage. The bucket name will be as path to that '
                            'directory relative to the repository root.'))
  parser.add_argument('-v', '--verbose',
                      action='store_true',
                      help='print debug information')


def Download(args):
  '''
  Downloads the Google Play services client library from a Google Cloud Storage
  bucket and installs it to
  //third_party/android_tools/sdk/extras/google/google_play_services.

  A license check will be made, and the user might have to accept the license
  if that has not been done before.
  '''

  paths = _InitPaths(constants.ANDROID_SDK_ROOT)

  new_lib_zip_sha1 = os.path.join(SHA1_DIRECTORY, LIBRARY_FILE_NAME + '.sha1')
  old_lib_zip_sha1 = os.path.join(paths.package, LIBRARY_FILE_NAME + '.sha1')

  logging.debug('Comparing library hashes: %s and %s', new_lib_zip_sha1,
                old_lib_zip_sha1)
  if utils.FileEquals(new_lib_zip_sha1, old_lib_zip_sha1) and not args.force:
    logging.debug('The Google Play services library is up to date.')
    return 0

  config = utils.ConfigParser(args.config)
  bucket_path = _VerifyBucketPathFormat(args.bucket,
                                        config.version_number,
                                        args.dry_run)

  tmp_root = tempfile.mkdtemp()
  try:
    if not os.environ.get('CHROME_HEADLESS'):
      if not os.path.isdir(paths.package):
        os.makedirs(paths.package)

      # download license file from bucket/{version_number}/license.sha1
      new_license = os.path.join(tmp_root, LICENSE_FILE_NAME)
      old_license = os.path.join(paths.package, LICENSE_FILE_NAME)

      license_sha1 = os.path.join(SHA1_DIRECTORY, LICENSE_FILE_NAME + '.sha1')
      _DownloadFromBucket(bucket_path, license_sha1, new_license,
                          args.verbose, args.dry_run)
      if not _CheckLicenseAgreement(new_license, old_license):
        logging.warning('Your version of the Google Play services library is '
                        'not up to date. You might run into issues building '
                        'or running the app. Please run `%s download` to '
                        'retry downloading it.', __file__)
        return 0

    new_lib_zip = os.path.join(tmp_root, LIBRARY_FILE_NAME)
    _DownloadFromBucket(bucket_path, new_lib_zip_sha1, new_lib_zip,
                        args.verbose, args.dry_run)

    # We remove only the library itself. Users having a SDK manager installed
    # library before will keep the documentation and samples from it.
    shutil.rmtree(paths.lib, ignore_errors=True)
    os.makedirs(paths.lib)

    logging.debug('Extracting the library to %s', paths.lib)
    with zipfile.ZipFile(new_lib_zip, "r") as new_lib_zip_file:
      new_lib_zip_file.extractall(paths.lib)

    logging.debug('Copying %s to %s', new_license, old_license)
    shutil.copy(new_license, old_license)

    logging.debug('Copying %s to %s', new_lib_zip_sha1, old_lib_zip_sha1)
    shutil.copy(new_lib_zip_sha1, old_lib_zip_sha1)

  finally:
    shutil.rmtree(tmp_root)

  return 0


def UpdateSdk(args):
  '''
  Uses the Android SDK Manager to update or download the local Google Play
  services library. Its usual installation path is
  //third_party/android_tools/sdk/extras/google/google_play_services
  '''

  # This should function should not run on bots and could fail for many user
  # and setup related reasons. Also, exceptions here are not caught, so we
  # disable breakpad to avoid spamming the logs.
  breakpad.IS_ENABLED = False

  sdk_manager = os.path.join(args.sdk_root, 'tools', 'android')
  cmd = [sdk_manager, 'update', 'sdk', '--no-ui', '--filter', GMS_PACKAGE_ID]
  cmd_helper.Call(cmd)
  # If no update is needed, it still returns successfully so we just do nothing

  return 0


def Upload(args):
  '''
  Uploads the local Google Play services client library to a Google Cloud
  storage bucket.

  By default, a local commit will be made at the end of the operation.
  '''

  # This should function should not run on bots and could fail for many user
  # and setup related reasons. Also, exceptions here are not caught, so we
  # disable breakpad to avoid spamming the logs.
  breakpad.IS_ENABLED = False

  paths = _InitPaths(args.sdk_root)

  if not args.skip_git and utils.IsRepoDirty(constants.DIR_SOURCE_ROOT):
    logging.error('The repo is dirty. Please commit or stash your changes.')
    return -1

  config = utils.ConfigParser(args.config)

  version_xml = os.path.join(paths.lib, 'res', 'values', 'version.xml')
  new_version_number = utils.GetVersionNumberFromLibraryResources(version_xml)
  logging.debug('comparing versions: new=%d, old=%s',
                new_version_number, config.version_number)
  if new_version_number <= config.version_number and not args.force:
    logging.info('The checked in version of the library is already the latest '
                 'one. No update needed. Please rerun with --force to skip '
                 'this check.')
    return 0

  tmp_root = tempfile.mkdtemp()
  try:
    new_lib_zip = os.path.join(tmp_root, LIBRARY_FILE_NAME)
    new_license = os.path.join(tmp_root, LICENSE_FILE_NAME)

    # need to strip '.zip' from the file name here
    shutil.make_archive(new_lib_zip[:-4], 'zip', paths.lib)
    _ExtractLicenseFile(new_license, paths.package)

    bucket_path = _VerifyBucketPathFormat(args.bucket, new_version_number,
                                          args.dry_run)
    files_to_upload = [new_lib_zip, new_license]
    logging.debug('Uploading %s to %s', files_to_upload, bucket_path)
    _UploadToBucket(bucket_path, files_to_upload, args.dry_run)

    new_lib_zip_sha1 = os.path.join(SHA1_DIRECTORY,
                                    LIBRARY_FILE_NAME + '.sha1')
    new_license_sha1 = os.path.join(SHA1_DIRECTORY,
                                    LICENSE_FILE_NAME + '.sha1')
    shutil.copy(new_lib_zip + '.sha1', new_lib_zip_sha1)
    shutil.copy(new_license + '.sha1', new_license_sha1)
  finally:
    shutil.rmtree(tmp_root)

  config.UpdateVersionNumber(new_version_number)

  if not args.skip_git:
    commit_message = ('Update the Google Play services dependency to %s\n'
                      '\n') % new_version_number
    utils.MakeLocalCommit(constants.DIR_SOURCE_ROOT,
                          [new_lib_zip_sha1, new_license_sha1, config.path],
                          commit_message)

  return 0


def _InitPaths(sdk_root):
  '''
  Initializes the different paths to be used in the update process.
  '''

  PlayServicesPaths = collections.namedtuple('PlayServicesPaths', [
      # Android SDK root path
      'sdk_root',

      # Path to the Google Play services package in the SDK manager sense,
      # where it installs the source.properties file
      'package',

      # Path to the Google Play services library itself (jar and res)
      'lib',
  ])

  sdk_play_services_package_dir = os.path.join('extras', 'google',
                                               'google_play_services')
  sdk_play_services_lib_dir = os.path.join(sdk_play_services_package_dir,
                                           'libproject',
                                           'google-play-services_lib')

  return PlayServicesPaths(
      sdk_root=sdk_root,
      package=os.path.join(sdk_root, sdk_play_services_package_dir),
      lib=os.path.join(sdk_root, sdk_play_services_lib_dir))


def _DownloadFromBucket(bucket_path, sha1_file, destination, verbose,
                        is_dry_run):
  '''Downloads the file designated by the provided sha1 from a cloud bucket.'''

  download_from_google_storage.download_from_google_storage(
      input_filename=sha1_file,
      base_url=bucket_path,
      gsutil=_InitGsutil(is_dry_run),
      num_threads=1,
      directory=None,
      recursive=False,
      force=False,
      output=destination,
      ignore_errors=False,
      sha1_file=sha1_file,
      verbose=verbose,
      auto_platform=True,
      extract=False)


def _UploadToBucket(bucket_path, files_to_upload, is_dry_run):
  '''Uploads the files designated by the provided paths to a cloud bucket. '''

  upload_to_google_storage.upload_to_google_storage(
      input_filenames=files_to_upload,
      base_url=bucket_path,
      gsutil=_InitGsutil(is_dry_run),
      force=False,
      use_md5=False,
      num_threads=1,
      skip_hashing=False,
      gzip=None)


def _InitGsutil(is_dry_run):
  '''Initialize the Gsutil object as regular or dummy version for dry runs. '''

  if is_dry_run:
    return DummyGsutil()
  else:
    return download_from_google_storage.Gsutil(
        download_from_google_storage.GSUTIL_DEFAULT_PATH)


def _ExtractLicenseFile(license_path, play_services_package_dir):
  prop_file_path = os.path.join(play_services_package_dir, 'source.properties')
  with open(prop_file_path, 'r') as prop_file:
    prop_file_content = prop_file.read()

  match = LICENSE_PATTERN.search(prop_file_content)
  if not match:
    raise AttributeError('The license was not found in ' +
                         os.path.abspath(prop_file_path))

  with open(license_path, 'w') as license_file:
    license_file.write(match.group('text'))


def _CheckLicenseAgreement(expected_license_path, actual_license_path):
  '''
  Checks that the new license is the one already accepted by the user. If it
  isn't, it prompts the user to accept it. Returns whether the expected license
  has been accepted.
  '''

  if utils.FileEquals(expected_license_path, actual_license_path):
    return True

  with open(expected_license_path) as license_file:
    # The output is buffered when running as part of gclient hooks. We split
    # the text here and flush is explicitly to avoid having part of it dropped
    # out.
    # Note: text contains *escaped* new lines, so we split by '\\n', not '\n'.
    for license_part in license_file.read().split('\\n'):
      # Uses plain print rather than logging to make sure this is not formatted
      # by the logger.
      print license_part
      sys.stdout.flush()

  # Need to put the prompt on a separate line otherwise the gclient hook buffer
  # only prints it after we received an input.
  print 'Do you accept the license? [y/n]: '
  sys.stdout.flush()
  return raw_input('> ') in ('Y', 'y')


def _VerifyBucketPathFormat(bucket_name, version_number, is_dry_run):
  '''
  Formats and checks the download/upload path depending on whether we are
  running in dry run mode or not. Returns a supposedly safe path to use with
  Gsutil.
  '''

  if is_dry_run:
    bucket_path = os.path.abspath(os.path.join(bucket_name,
                                               str(version_number)))
    if not os.path.isdir(bucket_path):
      os.makedirs(bucket_path)
  else:
    if bucket_name.startswith('gs://'):
      # We enforce the syntax without gs:// for consistency with the standalone
      # download/upload scripts and to make dry run transition easier.
      raise AttributeError('Please provide the bucket name without the gs:// '
                           'prefix (e.g. %s)' % GMS_CLOUD_STORAGE)
    bucket_path = 'gs://%s/%d' % (bucket_name, version_number)

  return bucket_path


class DummyGsutil(download_from_google_storage.Gsutil):
  '''
  Class that replaces Gsutil to use a local directory instead of an online
  bucket. It relies on the fact that Gsutil commands are very similar to shell
  ones, so for the ones used here (ls, cp), it works to just use them with a
  local directory.
  '''

  def __init__(self):
    super(DummyGsutil, self).__init__(
        download_from_google_storage.GSUTIL_DEFAULT_PATH)

  def call(self, *args):
    logging.debug('Calling command "%s"', str(args))
    return cmd_helper.GetCmdStatusOutputAndError(args)

  def check_call(self, *args):
    logging.debug('Calling command "%s"', str(args))
    return cmd_helper.GetCmdStatusOutputAndError(args)


if __name__ == '__main__':
  sys.exit(Main())
