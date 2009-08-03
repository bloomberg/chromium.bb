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
                                   "/../../../../third_party/simplejson"))
import simplejson as json

def RenderPage(name, test_shell):
  """
  Calls test_shell --layout-tests .../generator.html?<name> and writes the
  result to .../docs/<name>.html
  """
  
  if not name:
    raise Exception("RenderPage called with empty name");

  generator_url = _generator_html + "?" + name
  output_file = _base_dir + "/" + name + ".html"

  # Copy page_shell to destination output;
  if (os.path.isfile(output_file)):
    os.remove(output_file)

  shutil.copy(_page_shell_html, output_file)

  # Run test_shell and capture result
  p = Popen([test_shell, "--layout-tests", generator_url], shell=True,
      stdout=PIPE)

  # first output line is url that was processed by test_shell
  p.stdout.readline()

  # second output line is the layoutTestShell.dumpText() value.
  result = p.stdout.readline()  
  if (not result.startswith(_expected_output_preamble)):
    raise Exception("test_shell returned unexpected output: " + result);

  # Rewrite result
  os.remove(output_file)
  open(output_file, 'w').write(result)

def FindTestShell(product_dir):
  search_locations = []

  if (sys.platform in ('cygwin', 'win32')):
    search_locations.append(product_dir + "/test_shell.exe")

  if (sys.platform in ('linux', 'linux2')):
    search_locations.append(product_dir + "/test_shell")

  if (sys.platform == 'darwin'):
    search_locations.append(product_dir +
                            "/TestShell.app/Contents/MacOS/TestShell")

  for loc in search_locations:
    if os.path.isfile(loc):
      return loc

  print ("Failed to find test_shell executable in: \n" +
         "\n".join(search_locations))

  raise Exception('Could not find test_shell executable.')

def GetRelativePath(path):
  return os.path.normpath(path)[len(os.getcwd()) + 1:]
  
def GetAPIModuleNames():
  contents = open(_extension_api_json, 'r').read();
  extension_api = json.loads(contents, encoding="ASCII")
  return set( module['namespace'].encode() for module in extension_api)

def GetStaticFileNames():
  static_files = os.listdir(_static_dir)
  return set(os.path.splitext(file)[0]
             for file in static_files
             if file.endswith(".html"))

def GetInputFiles():
  input_files = [];

  input_files.append(GetRelativePath(_api_template_html));
  input_files.append(GetRelativePath(_page_shell_html));
  input_files.append(GetRelativePath(_generator_html));
  input_files.append(GetRelativePath(_script_path));
  input_files.append(GetRelativePath(_extension_api_json));
  # static files
  input_files += [GetRelativePath(_static_dir + "/" + name + ".html")
                  for name in GetStaticFileNames()]
  # js files
  input_files += [GetRelativePath(_js_dir + "/" + file)
                  for file in os.listdir(_js_dir)]
  return input_files

def GetOutputFiles():
  page_names = GetAPIModuleNames() | GetStaticFileNames()
  return [GetRelativePath(_base_dir + "/" + name + ".html")
          for name in page_names]

def main():
  parser = OptionParser()
  parser.add_option("--list-inputs", action="store_true", dest="list_inputs")
  parser.add_option("--list-outputs", action="store_true", dest="list_outputs")
  parser.add_option("--product-dir", dest="product_dir")

  (options, args) = parser.parse_args()

  # Return input list to gyp build target
  if (options.list_inputs):
    for f in GetInputFiles():
      print f
    return 0;
  # Return output list to gyp build target
  if (options.list_outputs):
    for f in GetOutputFiles():
      print f
    return 0;

  test_shell = FindTestShell(options.product_dir)

  # Read static file names 
  static_names = GetStaticFileNames()

  # Read module names 
  module_names = GetAPIModuleNames()

  # All pages to generate
  page_names = static_names | module_names

  for page in page_names:
    RenderPage(page, test_shell);
  
  return 0;

if __name__ == '__main__':
  sys.exit(main())