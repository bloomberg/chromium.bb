#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Docbuilder for extension docs."""

import os
import os.path
import shutil
import sys

from subprocess import Popen, PIPE
from optparse import OptionParser

_script_path = os.path.realpath(__file__)
_build_dir = os.path.dirname(_script_path)
_base_dir = os.path.normpath(_build_dir + "/..")
_static_dir = _base_dir + "/static"
_js_dir = _base_dir + "/js"
_template_dir = _base_dir + "/template"
_extension_api_dir = os.path.normpath(_base_dir + "/../api")

_extension_api_json = _extension_api_dir + "/extension_api.json"
_api_template_html = _template_dir + "/api_template.html"
_page_shell_html = _template_dir + "/page_shell.html"
_generator_html = _build_dir + "/generator.html"

_expected_output_preamble = "<!DOCTYPE html>"

# HACK! This is required because we can only depend on python 2.4 and
# the calling environment may not be setup to set the PYTHONPATH
sys.path.append(os.path.normpath(_base_dir +
                                   "/../../../../third_party"))
import simplejson as json

def RenderPage(name, test_shell):
  """
  Calls test_shell --layout-tests .../generator.html?<name> and writes the
  result to .../docs/<name>.html
  """
  
  if not name:
    raise Exception("RenderPage called with empty name");

  generator_url = _generator_html + "?" + name
  input_file = _base_dir + "/" + name + ".html"

  # Copy page_shell to destination output and move aside original, if it exists.
  original = None;
  if (os.path.isfile(input_file)):
    original = open(input_file, 'r').read()
    os.remove(input_file)

  shutil.copy(_page_shell_html, input_file)

  # Run test_shell and capture result
  p = Popen([test_shell, "--layout-tests", generator_url], shell=True,
      stdout=PIPE)

  # first output line is url that was processed by test_shell
  p.stdout.readline()

  # second output line is the layoutTestShell.dumpText() value.
  result = p.stdout.readline()  

  # Remove page_shell
  os.remove(input_file)

  if (not result.startswith(_expected_output_preamble)):
    if (original):
      open(input_file).write(original);
    raise Exception("test_shell returned unexpected output: " + result);

  # Write output
  open(input_file, 'w').write(result);
  if (original and result == original):
    return None
  else:
    return input_file

def FindTestShell():
  # This is hackey. It is used to guess the location of the test_shell
  chrome_dir = os.path.normpath(_base_dir + "/../../../")
  src_dir = os.path.normpath(chrome_dir + "/../")

  search_locations = []    
    
  if (sys.platform in ('cygwin', 'win32')):
    search_locations.append(chrome_dir + "/Debug/test_shell.exe")
    search_locations.append(chrome_dir + "/Release/test_shell.exe")

  if (sys.platform in ('linux', 'linux2')):
    search_locations.append(src_dir + "/sconsbuild/Debug/test_shell")
    search_locations.append(src_dir + "/out/Debug/test_shell")
    search_locations.append(src_dir + "/sconsbuild/Release/test_shell")
    search_locations.append(src_dir + "/out/Release/test_shell")

  if (sys.platform == 'darwin'):
    search_locations.append(src_dir +
        "/xcodebuild/Debug/TestShell.app/Contents/MacOS/TestShell")
    search_locations.append(src_dir +
        "/xcodebuild/Release/TestShell.app/Contents/MacOS/TestShell")

  for loc in search_locations:
    if os.path.isfile(loc):
      return loc

  raise Exception ("Could not find test_shell executable\n" +
                   "**test_shell may need to be built**\n" +
                   "Searched: \n" + "\n".join(search_locations) + "\n" + 
                   "To specify a path to test_shell use --test-shell-path")
  
def GetAPIModuleNames():
  contents = open(_extension_api_json, 'r').read();
  extension_api = json.loads(contents, encoding="ASCII")
  return set( module['namespace'].encode() for module in extension_api)

def GetStaticFileNames():
  static_files = os.listdir(_static_dir)
  return set(os.path.splitext(file)[0]
             for file in static_files
             if file.endswith(".html"))

def main():
  parser = OptionParser()
  parser.add_option("--test-shell-path", dest="test_shell_path")
  (options, args) = parser.parse_args()

  if (options.test_shell_path and os.path.isfile(options.test_shell_path)):
    test_shell = options.test_shell_path
  else:
    test_shell = FindTestShell()

  # Read static file names 
  static_names = GetStaticFileNames()

  # Read module names 
  module_names = GetAPIModuleNames()

  # All pages to generate
  page_names = static_names | module_names

  modified_files = []
  for page in page_names:
    modified_file = RenderPage(page, test_shell)
    if (modified_file):
      modified_files.append(modified_file)
      
  if (len(modified_files) == 0):
    print "Output files match existing files. No changes made."
  else:
    print ("ATTENTION: EXTENSION DOCS HAVE CHANGED\n" +
           "The following files have been modified and should be checked\n" +
           "into source control (ideally in the same changelist as the\n" +
           "underlying files that resulting in their changing).")
    for f in modified_files:
      print f;

  return 0;

if __name__ == '__main__':
  sys.exit(main())