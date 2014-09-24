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
import json
import optparse
import os
import sys
import tempfile
from zipfile import ZipFile

# A list of files (or directories) to exclude from the webstore build.
EXCLUDE_PATHS = [
    'cvox2/background/',
    'deps.js',
    'manifest_guest.json',
    'manifest_next.json',
    'manifest_next_guest.json'
    ]


def CreateOptionParser():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog <extension_path> <output_path> <client_secret'
  return parser

def MakeManifestEdits(root, old, new_file):
  '''Customize a manifest for the webstore.

  Args:
    root: The directory containing file.

    old: A json file.

    new_file: a temporary file to place the manifest in.

  Returns:
    File of the new manifest.
  '''
  with open(os.path.join(root, old)) as old_file:
    new_contents = json.loads(old_file.read())
    new_contents.pop('key', '')
    new_file.file.write(json.dumps(new_contents))

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
             chromevox_webstore_util.PostUpload(output_path, client_secret))
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
  _, args = CreateOptionParser().parse_args()
  if len(args) != 3:
    print 'Expected exactly three arguments'
    sys.exit(1)

  extension_path = args[0]
  output_path = args[1]
  client_secret = args[2]

  with ZipFile(output_path, 'w') as zip:
    for root, dirs, files in os.walk(extension_path):
      rel_path = os.path.join(os.path.relpath(root, extension_path), '')
      if rel_path in EXCLUDE_PATHS:
        continue

      for extension_file in files:
        if extension_file in EXCLUDE_PATHS:
          continue
        if extension_file == 'manifest.json':
          new_file = tempfile.NamedTemporaryFile(mode='w+a', bufsize=0)
          MakeManifestEdits(root, extension_file, new_file)
          zip.write(
              new_file.name, os.path.join(rel_path, extension_file))
          continue

        zip.write(os.path.join(root, extension_file),
                  os.path.join(rel_path, extension_file))
  print 'Created ChromeVox zip file in %s' % output_path
  print 'Please run manual smoke tests before proceeding.'
  RunInteractivePrompt(client_secret, output_path)


if __name__ == '__main__':
  main()
