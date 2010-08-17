#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Docbuilder for extension docs."""

import os
import os.path
import shutil
import sys
import time
import urllib

from subprocess import Popen, PIPE
from optparse import OptionParser

_script_path = os.path.realpath(__file__)
_build_dir = os.path.dirname(_script_path)
_base_dir = os.path.normpath(_build_dir + "/..")
_static_dir = _base_dir + "/static"
_js_dir = _base_dir + "/js"
_template_dir = _base_dir + "/template"
_samples_dir = _base_dir + "/examples"
_extension_api_dir = os.path.normpath(_base_dir + "/../api")

_extension_api_json = _extension_api_dir + "/extension_api.json"
_api_template_html = _template_dir + "/api_template.html"
_page_shell_html = _template_dir + "/page_shell.html"
_generator_html = _build_dir + "/generator.html"
_samples_json = _base_dir + "/samples.json"

_expected_output_preamble = "#BEGIN"
_expected_output_postamble = "#END"

# HACK! This is required because we can only depend on python 2.4 and
# the calling environment may not be setup to set the PYTHONPATH
sys.path.append(os.path.normpath(_base_dir +
                                   "/../../../../third_party"))
import simplejson as json
from directory import Sample
from directory import ApiManifest
from directory import SamplesManifest

def RenderPages(names, test_shell):
  """
  Calls test_shell --layout-tests .../generator.html?<names> and writes the
  results to .../docs/<name>.html
  """
  if not names:
    raise Exception("RenderPage called with empty names param")

  generator_url = "file:" + urllib.pathname2url(_generator_html)
  generator_url += "?" + ",".join(names)

  # Start with a fresh copy of page shell for each file.
  # Save the current contents so that we can look for changes later.
  originals = {}
  for name in names:
    input_file = _base_dir + "/" + name + ".html"

    if (os.path.isfile(input_file)):
      originals[name] = open(input_file, 'rb').read()
      os.remove(input_file)
    else:
      originals[name] = ""

    shutil.copy(_page_shell_html, input_file)

  # Run test_shell and capture result
  test_shell_timeout = 1000 * 60 * 5  # five minutes
  p = Popen(
      [test_shell, "--layout-tests", "--time-out-ms=%s" % test_shell_timeout,
          generator_url],
      stdout=PIPE)

  # The remaining output will be the content of the generated pages.
  output = p.stdout.read()

  # Parse out just the JSON part.
  begin = output.find(_expected_output_preamble)
  end = output.rfind(_expected_output_postamble)

  if (begin < 0 or end < 0):
    raise Exception ("test_shell returned invalid output:\n\n" + output)

  begin += len(_expected_output_preamble)

  try:
    output_parsed = json.loads(output[begin:end])
  except ValueError, msg:
   raise Exception("Could not parse test_shell output as JSON. Error: " + msg +
                   "\n\nOutput was:\n" + output)

  changed_files = []
  for name in names:
    result = output_parsed[name].encode("utf8") + '\n'

    # Remove CRs that are appearing from captured test_shell output.
    result = result.replace('\r', '')

    # Remove page_shell
    input_file = _base_dir + "/" + name + ".html"
    os.remove(input_file)

    # Write output
    open(input_file, 'wb').write(result)
    if (originals[name] and result != originals[name]):
      changed_files.append(input_file)

  return changed_files


def FindTestShell():
  # This is hacky. It is used to guess the location of the test_shell
  chrome_dir = os.path.normpath(_base_dir + "/../../../")
  src_dir = os.path.normpath(chrome_dir + "/../")

  search_locations = []

  if (sys.platform in ('cygwin', 'win32')):
    home_dir = os.path.normpath(os.getenv("HOMEDRIVE") + os.getenv("HOMEPATH"))
    search_locations.append(chrome_dir + "/Release/test_shell.exe")
    search_locations.append(chrome_dir + "/Debug/test_shell.exe")
    search_locations.append(home_dir + "/bin/test_shell/" +
                            "test_shell.exe")

  if (sys.platform in ('linux', 'linux2')):
    search_locations.append(src_dir + "/sconsbuild/Release/test_shell")
    search_locations.append(src_dir + "/out/Release/test_shell")
    search_locations.append(src_dir + "/sconsbuild/Debug/test_shell")
    search_locations.append(src_dir + "/out/Debug/test_shell")
    search_locations.append(os.getenv("HOME") + "/bin/test_shell/test_shell")

  if (sys.platform == 'darwin'):
    search_locations.append(src_dir +
        "/xcodebuild/Release/TestShell.app/Contents/MacOS/TestShell")
    search_locations.append(src_dir +
        "/xcodebuild/Debug/TestShell.app/Contents/MacOS/TestShell")
    search_locations.append(os.getenv("HOME") + "/bin/test_shell/" +
                            "TestShell.app/Contents/MacOS/TestShell")

  for loc in search_locations:
    if os.path.isfile(loc):
      return loc

  raise Exception("Could not find test_shell executable\n" +
                  "**test_shell may need to be built**\n" +
                  "Searched: \n" + "\n".join(search_locations) + "\n" +
                  "To specify a path to test_shell use --test-shell-path")

def GetStaticFileNames():
  static_files = os.listdir(_static_dir)
  return set(os.path.splitext(file_name)[0]
             for file_name in static_files
             if file_name.endswith(".html") and not file_name.startswith("."))

def main():
  # Prevent windows from using cygwin python.
  if (sys.platform == "cygwin"):
    sys.exit("Building docs not supported for cygwin python. Please run the "
             "build.sh script instead, which uses depot_tools python.")

  parser = OptionParser()
  parser.add_option("--test-shell-path", dest="test_shell_path")
  parser.add_option("--page-name", dest="page_name")
  (options, args) = parser.parse_args()

  if (options.test_shell_path and os.path.isfile(options.test_shell_path)):
    test_shell = options.test_shell_path
  else:
    test_shell = FindTestShell()

  # Load the manifest of existing API Methods
  api_manifest = ApiManifest(_extension_api_json)

  # Read static file names
  static_names = GetStaticFileNames()

  # Read module names
  module_names = api_manifest.getModuleNames()

  # All pages to generate
  page_names = static_names | module_names

  # Allow the user to render a single page if they want
  if options.page_name:
    if options.page_name in page_names:
      page_names = [options.page_name]
    else:
      raise Exception("--page-name argument must be one of %s." %
                      ', '.join(sorted(page_names)))

  # Render a manifest file containing metadata about all the extension samples
  samples_manifest = SamplesManifest(_samples_dir, _base_dir, api_manifest)
  samples_manifest.writeToFile(_samples_json)

  modified_files = RenderPages(page_names, test_shell)

  if (len(modified_files) == 0):
    print "Output files match existing files. No changes made."
  else:
    print ("ATTENTION: EXTENSION DOCS HAVE CHANGED\n" +
           "The following files have been modified and should be checked\n" +
           "into source control (ideally in the same changelist as the\n" +
           "underlying files that resulting in their changing).")
    for f in modified_files:
      print f

  # Hack. Sleep here, otherwise windows doesn't properly close the debug.log
  # and the os.remove will fail with a "Permission denied".
  time.sleep(1)
  debug_log = os.path.normpath(_build_dir + "/" + "debug.log")
  if (os.path.isfile(debug_log)):
    os.remove(debug_log)

  if 'EX_OK' in dir(os):
    return os.EX_OK
  else:
    return 0

if __name__ == '__main__':
  sys.exit(main())
