#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Publishes ChromeVox to the webstore.
  Given an unpacked extension, compresses and sends to the Chrome webstore.

  Releasing to the webstore should involve the following manual steps before
      running this script:
    1. clean the output directory.
    2. make a release build.
    3. run manual smoke tests.
    4. run automated ChromeVox tests.
'''

import chromevox_webstore_util
import generate_manifest
import json
import optparse
import os
import sys
import tempfile
from zipfile import ZipFile

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CHROME_SOURCE_DIR = os.path.normpath(
    os.path.join(
        _SCRIPT_DIR, *[os.path.pardir] * 6))

sys.path.insert(
    0, os.path.join(_CHROME_SOURCE_DIR, 'build', 'util'))
import version

# A list of files (or directories) to exclude from the webstore build.
EXCLUDE_PATHS = [
    'cvox2/background/',
    'manifest.json',
    'manifest_guest.json',
    ]


def CreateOptionParser():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog <extension_path> <client_secret>'
  parser.add_option('-p', '--publish', action='store_true',
                    help='publish the extension')
  return parser


def GetVersion():
  '''Returns the chrome version string.'''
  filename = os.path.join(_CHROME_SOURCE_DIR, 'chrome', 'VERSION')
  values = version.fetch_values([filename])
  return version.subst_template('@MAJOR@.@MINOR@.@BUILD@.@PATCH@', values)


def MakeManifest():
  '''Create a manifest for the webstore.

  Returns:
    Temporary file with generated manifest.
  '''
  new_file = tempfile.NamedTemporaryFile(mode='w+a', bufsize=0)
  in_file_name = os.path.join(_SCRIPT_DIR, os.path.pardir,
                              'manifest.json.jinja2')
  context = {
    'is_chromevox_classic': '1',
    'is_guest_manifest': '0',
    'is_js_compressed': '1',
    'set_version': GetVersion()
  }
  generate_manifest.processJinjaTemplate(in_file_name, new_file.name, context)
  return new_file


def RunInteractivePrompt(client_secret, output_path):
  input = ''
  while True:
    print 'u upload'
    print 'g get upload status'
    print 't publish trusted tester'
    print 'p publish public'
    print 'q quit'
    input = raw_input('Please select an option: ')
    input = input.strip()
    if input == 'g':
      print ('Upload status: %s' %
             chromevox_webstore_util.GetUploadStatus(client_secret).read())
    elif input == 'u':
      print ('Uploaded with status: %s' %
          chromevox_webstore_util.PostUpload(output_path.name, client_secret))
    elif input == 't':
      print ('Published to trusted testers with status: %s' %
          chromevox_webstore_util.PostPublishTrustedTesters(
              client_secret).read())
    elif input == 'p':
      print ('Published to public with status: %s' %
             chromevox_webstore_util.PostPublish(client_secret).read())
    elif input == 'q':
      sys.exit()
    else:
      print 'Unrecognized option: %s' % input

def main():
  options, args = CreateOptionParser().parse_args()
  if len(args) != 2:
    print 'Expected exactly two arguments'
    print str(args)
    sys.exit(1)

  extension_path = args[0]
  client_secret = args[1]
  output_path = tempfile.NamedTemporaryFile()

  with ZipFile(output_path, 'w') as zip:
    for root, dirs, files in os.walk(extension_path):
      rel_path = os.path.join(os.path.relpath(root, extension_path), '')
      if rel_path in EXCLUDE_PATHS:
        continue

      for extension_file in files:
        if extension_file in EXCLUDE_PATHS:
          continue

        zip.write(os.path.join(root, extension_file),
                  os.path.join(rel_path, extension_file))
    manifest_file = MakeManifest()
    zip.write(manifest_file.name, 'manifest.json')
  print 'Created ChromeVox zip file in %s' % output_path.name
  print 'Please run manual smoke tests before proceeding.'
  if options.publish:
    print('Uploading...%s' %
        chromevox_webstore_util.PostUpload(output_path.name, client_secret))
    print('publishing...%s' %
        chromevox_webstore_util.PostPublish(client_secret).read())
  else:
    RunInteractivePrompt(client_secret, output_path)


if __name__ == '__main__':
  main()
