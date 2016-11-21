#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates google.admx and google.adml base files in all output folders.
"""

import grd_helper
import optparse
import os
import sys


def CreateGoogleAdmx(path):
  '''Creates an ADMX template at |path| that defines a Google namespace and a
  Google category.

  Args:
    path: File path to create the template at.
  '''
  f = open(path, 'w')
  f.write("<?xml version=\"1.0\" ?>\n\
<policyDefinitions revision=\"1.0\" schemaVersion=\"1.0\">\n\
  <policyNamespaces>\n\
    <target namespace=\"Google.Policies\" prefix=\"Google\"/>\n\
  </policyNamespaces>\n\
  <resources minRequiredRevision=\"1.0\" />\n\
  <categories>\n\
    <category displayName=\"$(string.google)\" name=\"Cat_Google\"/>\n\
  </categories>\n\
</policyDefinitions>\n")
  f.close()


def CreateGoogleAdml(path):
  '''Creates an ADML template at |path| that defines a single string 'Google'.

  Args:
    path: File path to create the template at.
  '''
  f = open(path, 'w')
  f.write("<?xml version=\"1.0\" ?>\n\
<policyDefinitionResources revision=\"1.0\" schemaVersion=\"1.0\">\n\
  <displayName/>\n\
  <description/>\n\
  <resources>\n\
    <stringTable>\n\
      <string id=\"google\">Google</string>\n\
    </stringTable>\n\
  </resources>\n\
</policyDefinitionResources>\n")
  f.close()


def main(argv):
  """Creates google.admx and google.adml files for every admx resp. adml files
  in the grit outputs.
  Usage: create_google_admx
            --basedir <dir>   Base file path of output files.
            <grid options>    Defined in grd_helper.py.
  """
  parser = optparse.OptionParser()
  parser.add_option("--basedir", dest="basedir")
  grd_helper.add_options(parser)
  options, args = parser.parse_args(argv[1:])

  file_list = grd_helper.get_grd_outputs(options)
  for path in file_list:
    directory, filename = os.path.split(path)
    filename, extension = os.path.splitext(filename)
    google_path = os.path.join(options.basedir, directory, "google" + extension)
    if extension == ".admx":
      CreateGoogleAdmx(google_path)
    elif extension == ".adml":
      CreateGoogleAdml(google_path)

if '__main__' == __name__:
  sys.exit(main(sys.argv))
