#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Utility to update the SDK manifest file in the build_tools directory'''

import optparse
import os
import re
import sdk_update
import string
import subprocess
import sys

HELP='''"Usage: %prog [-b bundle] [options]"

Actions for particular bundles:
  sdk_tools: Upload the most recently built nacl_sdk.zip and sdk_tools.tgz
      files to the server and update the manifest file
  pepper_??: Download the latest pepper builds off the appropriate branch,
      upload these files to the appropriate location on the server, and
      update the manifest file.
  <others>: Only update manifest file -- you'll need to upload the file yourself
'''

# Map option keys to manifest attribute key. Option keys are used to retrieve
# option values from cmd-line options. Manifest attribute keys label the
# corresponding value in the manifest object.
OPTION_KEY_MAP = {
  #  option key         manifest attribute key
    'bundle_desc_url': 'desc_url',
    'bundle_revision': sdk_update.REVISION_KEY,
    'bundle_version':  sdk_update.VERSION_KEY,
    'desc':            'description',
    'recommended':     'recommended',
    'stability':       'stability',
    }
# Map options keys to platform key, as stored in the bundle.
OPTION_KEY_TO_PLATFORM_MAP = {
    'mac_arch_url':    'mac',
    'win_arch_url':    'win',
    'linux_arch_url':  'linux',
    'all_arch_url':    'all',
    }

NACL_SDK_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))

BUILD_TOOLS_OUT = os.path.join(NACL_SDK_ROOT, 'scons-out', 'build', 'obj',
                               'build_tools')

BUNDLE_SDK_TOOLS = 'sdk_tools'
BUNDLE_PEPPER_MATCHER = re.compile('^pepper_([0-9]+)$')
IGNORE_OPTIONS = set(['gsutil', 'manifest_file', 'upload', 'root_url'])


class Error(Exception):
  '''Generic error/exception for update_manifest module'''
  pass


def UpdateBundle(bundle, options):
  ''' Update the bundle per content of the options.

  Args:
    options: options data. Attributes that are used are also deleted from
             options.'''
  # Check, set and consume individual bundle options.
  for option_key, attribute_key in OPTION_KEY_MAP.iteritems():
    option_val = getattr(options, option_key, None)
    if option_val is not None:
      bundle[attribute_key] = option_val
      delattr(options, option_key);
  # Validate what we have so far; we may just avoid going through a lengthy
  # download, just to realize that some other trivial stuff is missing.
  bundle.Validate()
  # Check and consume archive-url options.
  for option_key, host_os in OPTION_KEY_TO_PLATFORM_MAP.iteritems():
    platform_url = getattr(options, option_key, None)
    if platform_url is not None:
      bundle.UpdateArchive(host_os, platform_url)
      delattr(options, option_key);


class UpdateSDKManifest(sdk_update.SDKManifest):
  '''Adds functions to SDKManifest that are only used in update_manifest'''

  def _ValidateBundleName(self, name):
    ''' Verify that name is a valid bundle.

    Args:
      name: the proposed name for the bundle.

    Return:
      True if the name is valid for a bundle, False otherwise.'''
    valid_char_set = '()-_.%s%s' % (string.ascii_letters, string.digits)
    name_len = len(name)
    return (name_len > 0 and all(c in valid_char_set for c in name))

  def _UpdateManifestVersion(self, options):
    ''' Update the manifest version number from the options

    Args:
      options: options data containing an attribute self.manifest_version '''
    version_num = int(options.manifest_version)
    self._manifest_data['manifest_version'] = version_num
    del options.manifest_version

  def _UpdateBundle(self, options):
    ''' Update or setup a bundle from the options.

    Args:
      options: options data containing at least a valid bundle_name
               attribute. Other relevant bundle attributes will also be
               used (and consumed) by this function. '''
    # Get and validate the bundle name
    if not self._ValidateBundleName(options.bundle_name):
      raise Error('Invalid bundle name: "%s"' % options.bundle_name)
    bundle_name = options.bundle_name
    del options.bundle_name
    # Get the corresponding bundle, or create it.
    bundle = self.GetBundle(bundle_name)
    if not bundle:
      bundle = sdk_update.Bundle(bundle_name)
      self.SetBundle(bundle)
    UpdateBundle(bundle, options)

  def _VerifyAllOptionsConsumed(self, options, bundle_name):
    ''' Verify that all the options have been used. Raise an exception if
        any valid option has not been used. Returns True if all options have
        been consumed.

    Args:
      options: the object containing the remaining unused options attributes.
      bundl_name: The name of the bundle, or None if it's missing.'''
    # Any option left in the list should have value = None
    for key, val in options.__dict__.items():
      if val != None and key not in IGNORE_OPTIONS:
        if bundle_name:
          raise Error('Unused option "%s" for bundle "%s"' % (key, bundle_name))
        else:
          raise Error('No bundle name specified')
    return True;

  def UpdateManifest(self, options):
    ''' Update the manifest object with values from the command-line options

    Args:
      options: options object containing attribute for the command-line options.
               Note that all the non-trivial options are consumed.
    '''
    # Go over all the options and update the manifest data accordingly.
    # Valid options are consumed as they are used. This gives us a way to
    # verify that all the options are used.
    if options.manifest_version is not None:
      self._UpdateManifestVersion(options)
    # Keep a copy of bundle_name, which will be consumed by UpdateBundle, for
    # use in _VerifyAllOptionsConsumed below.
    bundle_name = options.bundle_name
    if bundle_name is not None:
      self._UpdateBundle(options)
    self._VerifyAllOptionsConsumed(options, bundle_name)
    self._ValidateManifest()


class GsUtil(object):
  def __init__(self, gsutil):
    '''gsutil is the path to the gsutil executable'''
    self.gsutil = gsutil
    self.root = 'gs://nativeclient-mirror/nacl/nacl_sdk'

  def GetURI(self, path):
    '''Return the full gs:// URI for a given relative path'''
    return '/'.join([self.root, path])

  def Run(self, command):
    '''Runs gsutil with a given argument list and returns exit status'''
    args = [self.gsutil] + command
    print 'GSUtil.Run(%s)' % args
    sys.stdout.flush()
    return subprocess.call(args)

  def CheckIfExists(self, path):
    '''Check whether a given path exists on commondatastorage

    Args:
      path: path relative to SDK root directory on the server

    Returns: True if it exists, False if it does not'''
    # todo(mball): Be a little more intelligent about this check and compare
    # the output strings against expected values
    return self.Run(['ls', self.GetURI(path)]) == 0

  def Copy(self, source, destination):
    '''Copies a given source file to a destination path and makes it readable

    Args:
      source: path to source file on local filesystem
      destination: path to destination, relative to root directory'''
    args = ['cp', '-a', 'public-read', source, self.GetURI(destination)]
    if self.Run(args) != 0:
      raise Error('Unable to copy %s to %s' % (source, destination))


class UpdateSDKManifestFile(sdk_update.SDKManifestFile):
  '''Adds functions to SDKManifestFile that are only used in update_manifest'''

  def __init__(self, options):
    '''Create a new SDKManifest object with default contents.

    If |json_filepath| is specified, and it exists, its contents are loaded and
    used to initialize the internal manifest.

    Args:
      json_filepath: path to json file to read/write, or None to write a new
          manifest file to stdout.
    '''
    # Strip-off all the I/O-based options that do not relate to bundles
    self._json_filepath = options.manifest_file
    self.gsutil = GsUtil(options.gsutil)
    self.options = options
    self._manifest = UpdateSDKManifest()
    if self._json_filepath:
      self._LoadFile()

  def _HandleSDKTools(self):
    '''Handles the sdk_tools bundle'''
    # General sanity checking of parameters
    SDK_TOOLS_FILES = ['sdk_tools.tgz', 'nacl_sdk.zip']
    options = self.options
    if options.bundle_version is None:
      options.bundle_version = sdk_update.MAJOR_REV
    if options.bundle_version != sdk_update.MAJOR_REV:
      raise Error('Specified version (%s) does not match MAJOR_REV (%s)' %
                  (options.bundle_version, sdk_update.MAJOR_REV))
    if options.bundle_revision is None:
      options.bundle_revision = sdk_update.MINOR_REV
    if options.bundle_revision != sdk_update.MINOR_REV:
      raise Error('Specified revision (%s) does not match MINOR_REV (%s)' %
                  (options.bundle_revision, sdk_update.MINOR_REV))
    version = '%s.%s' % (options.bundle_version, options.bundle_revision)
    # Update the remaining options
    if options.desc is None:
      options.desc = ('Native Client SDK Tools, revision %s.%s' %
                      (options.bundle_version, options.bundle_revision))
    options.recommended = options.recommended or 'yes'
    options.stability = options.stability or 'stable'
    if options.upload:
      # Check whether the tools already exist
      for name in SDK_TOOLS_FILES:
        path = '/'.join([version, name])
        if self.gsutil.CheckIfExists(path):
          raise Error('File already exists at %s' % path)
      # Upload the tools files to the server
      for name in SDK_TOOLS_FILES:
        source = os.path.join(BUILD_TOOLS_OUT, name)
        destination = '/'.join([version, name])
        self.gsutil.Copy(source, destination)
      url = '/'.join([options.root_url, version, 'sdk_tools.tgz'])
      options.mac_arch_url = options.mac_arch_url or url
      options.linux_arch_url = options.linux_arch_url or url
      options.win_arch_url = options.win_arch_url or url

  def _HandlePepper(self):
    '''Handles the pepper bundles'''
    options = self.options
    match = BUNDLE_PEPPER_MATCHER.match(options.bundle_name)
    if match is not None:
      options.bundle_version = int(match.group(1))
    if options.bundle_version is None:
      raise Error('Need to specify a bundle version')
    if options.bundle_revision is None:
      raise Error('Need to specify a bundle revision')
    if options.desc is None:
      options.desc = ('Chrome %s bundle, revision %s' %
                      (options.bundle_version, options.bundle_revision))
    root_url = '%s/pepper_%s_%s' % (options.root_url, options.bundle_version,
                                    options.bundle_revision)
    options.mac_arch_url = '/'.join([root_url, 'naclsdk_mac.tgz'])
    options.linux_arch_url = '/'.join([root_url, 'naclsdk_linux.tgz'])
    options.win_arch_url = '/'.join([root_url, 'naclsdk_win.exe'])

  def HandleBundles(self):
    '''Handles known bundles by automatically uploading files'''
    bundle_name = self.options.bundle_name
    if bundle_name == BUNDLE_SDK_TOOLS:
      self._HandleSDKTools()
    elif bundle_name.startswith('pepper'):
      self._HandlePepper()

  def UpdateWithOptions(self):
    ''' Update the manifest file with the given options. Create the manifest
        if it doesn't already exists. Raises an Error if the manifest doesn't
        validate after updating.

    Args:
      options: option data'''
    # UpdateManifest does not know how to deal with file-related options
    self._manifest.UpdateManifest(self.options)
    self.WriteFile()


def main(argv):
  '''Main entry for update_manifest.py'''

  buildtools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  parser = optparse.OptionParser(usage=HELP)

  # Setup options
  parser.add_option(
      '-b', '--bundle-version', dest='bundle_version',
      type='int',
      default=None,
      help='Required: Version number for the bundle.')
  parser.add_option(
      '-B', '--bundle-revision', dest='bundle_revision',
      type='int',
      default=None,
      help='Required: Revision number for the bundle.')
  parser.add_option(
      '-d', '--description', dest='desc',
      default=None,
      help='Required: Description for this bundle.')
  parser.add_option(
      '-f', '--manifest-file', dest='manifest_file',
      default=os.path.join(buildtools_dir, 'json',
                           sdk_update.MANIFEST_FILENAME),
      help='location of manifest file to read and update')
  parser.add_option(
      '-g', '--gsutil', dest='gsutil',
      default='gsutil', help='location of gsutil tool for uploading bundles')
  parser.add_option(
      '-L', '--linux-archive', dest='linux_arch_url',
      default=None,
      help='URL for the Linux archive.')
  parser.add_option(
      '-M', '--mac-archive', dest='mac_arch_url',
      default=None,
      help='URL for the Mac archive.')
  parser.add_option(
      '-n', '--bundle-name', dest='bundle_name',
      default=None,
      help='Required: Name of the bundle.')
  parser.add_option(
      '-r', '--recommended', dest='recommended',
      choices=sdk_update.YES_NO_LITERALS,
      default=None,
      help='Required: whether this bundle is recommended. one of "yes" or "no"')
  parser.add_option(
      '-R', '--root-url', dest='root_url',
      default='http://commondatastorage.googleapis.com/nativeclient-mirror/'
              'nacl/nacl_sdk',
      help='Root url for uploading')
  parser.add_option(
      '-s', '--stability', dest='stability',
      choices=sdk_update.STABILITY_LITERALS,
      default=None,
      help='Required: Stability for this bundle; one of. '
           '"obsolete", "post_stable", "stable", "beta", "dev", "canary".')
  parser.add_option(
      '-u', '--desc-url', dest='bundle_desc_url',
      default=None,
      help='Optional: URL to follow to read additional bundle info.')
  parser.add_option(
      '-U', '--upload', dest='upload', default=False, action='store_true',
      help='Indicates whether to upload bundle to server')
  parser.add_option(
      '-v', '--manifest-version', dest='manifest_version',
      type='int',
      default=None,
      help='Required for new manifest files: '
           'Version number for the manifest.')
  parser.add_option(
      '-W', '--win-archive', dest='win_arch_url',
      default=None,
      help='URL for the Windows archive.')

  # Parse options and arguments and check.
  (options, args) = parser.parse_args(argv)
  if len(args) > 0:
    parser.error('These arguments were not understood: %s' % args)

  manifest_file = UpdateSDKManifestFile(options)
  manifest_file.HandleBundles()
  manifest_file.UpdateWithOptions()

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
