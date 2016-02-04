#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A collator for Mojo Application Manifests """

import argparse
import json
import shutil
import sys
import urlparse

def ParseJSONFile(filename):
  with open(filename) as json_file:
    try:
      return json.load(json_file)
    except ValueError:
      print "%s is not a valid JSON document" % filename
      return None

def main():
  parser = argparse.ArgumentParser(
      description="Collate Mojo application manifests.")
  parser.add_argument("--parent")
  parser.add_argument("--output")
  parser.add_argument("--application-name")
  args, children = parser.parse_known_args()

  parent = ParseJSONFile(args.parent)
  if parent == None:
    return 1

  parsed = urlparse.urlparse(parent['url'])
  if args.application_name != parsed.hostname:
    raise ValueError("Application name '%s' specified in build file does not " \
                     "match application name '%s' specified in manifest." %
                     (args.application_name, parsed.hostname))

  applications = []
  for child in children:
    application = ParseJSONFile(child)
    if application == None:
      return 1
    applications.append(application)

  if len(applications) > 0:
    parent['applications'] = applications

  with open(args.output, 'w') as output_file:
    json.dump(parent, output_file)

  return 0

if __name__ == "__main__":
  sys.exit(main())
