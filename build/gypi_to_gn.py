# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Converts a given gypi file to a python scope and writes the result to stdout.

It is assumed that the file contains a toplevel dictionary, and this script
will return that dictionary as a GN "scope" (see example below). This script
does not know anything about GYP and it will not expand variables or execute
conditions (it will check for the presence of a "conditions" value in the
dictionary and will abort if it is present). It also does not support nested
dictionaries.

Say your_file.gypi looked like this:
  {
     'sources': [ 'a.cc', 'b.cc' ],
     'defines': [ 'ENABLE_DOOM_MELON' ],
  }

You would call it like this:
  gypi_values = exec_script("//build/gypi_to_gn.py",
                            [ rebase_path("your_file.gypi") ],
                            "scope",
                            [ "your_file.gypi" ])

Notes:
 - The rebase_path call converts the gypi file from being relative to the
   current build file to being system absolute for calling the script, which
   will have a different current directory than this file.

 - The "scope" parameter tells GN to interpret the result as a series of GN
   variable assignments.

 - The last file argument to exec_script tells GN that the given file is a
   dependency of the build so Ninja can automatically re-run GN if the file
   changes.

Read the values into a target like this:
  component("mycomponent") {
    sources = gypi_values.sources
    defines = gypi_values.defines
  }

Sometimes your .gypi file will include paths relative to a different
directory than the current .gn file. In this case, you can rebase them to
be relative to the current directory.
  sources = rebase_path(gypi_values.sources, ".",
                        "//path/gypi/input/values/are/relative/to")

This script will tolerate a 'variables' in the toplevel dictionary or not. If
the toplevel dictionary just contains one item called 'variables', it will be
collapsed away and the result will be the contents of that dictinoary. Some
.gypi files are written with or without this, depending on how they expect to
be embedded into a .gyp file.

This script also has the ability to replace certain substrings in the input.
Generally this is used to emulate GYP variable expansion. If you passed the
argument "--replace=<(foo)=bar" then all instances of "<(foo)" in strings in
the input will be replaced with "bar":

  gypi_values = exec_script("//build/gypi_to_gn.py",
                            [ rebase_path("your_file.gypi"),
                              "--replace=<(foo)=bar"],
                            "scope",
                            [ "your_file.gypi" ])

"""

import gn_helpers
from optparse import OptionParser
import sys

def LoadPythonDictionary(path):
  file_string = open(path).read()
  try:
    file_data = eval(file_string, {'__builtins__': None}, None)
  except SyntaxError, e:
    e.filename = path
    raise
  except Exception, e:
    raise Exception("Unexpected error while reading %s: %s" % (path, str(e)))

  assert isinstance(file_data, dict), "%s does not eval to a dictionary" % path
  assert 'conditions' not in file_data, \
      "The file %s has conditions in it, these aren't supported." % path

  # If the contents of the root is a dictionary with exactly one kee
  # "variables", promote the contents of that to the top level. Some .gypi
  # files contain this and some don't depending on how they expect to be
  # embedded in a .gyp file. We don't actually care either way so collapse it
  # away.
  if len(file_data) == 1 and 'variables' in file_data:
    return file_data['variables']

  return file_data


def ReplaceSubstrings(values, search_for, replace_with):
  """Recursively replaces substrings in a value.

  Replaces all substrings of the "search_for" with "repace_with" for all
  strings occurring in "values". This is done by recursively iterating into
  lists as well as the keys and values of dictionaries."""
  if isinstance(values, str):
    return values.replace(search_for, replace_with)

  if isinstance(values, list):
    return [ReplaceSubstrings(v, search_for, replace_with) for v in values]

  if isinstance(values, dict):
    # For dictionaries, do the search for both the key and values.
    result = {}
    for key, value in values.items():
      new_key = ReplaceSubstrings(key, search_for, replace_with)
      new_value = ReplaceSubstrings(value, search_for, replace_with)
      result[new_key] = new_value
    return result

  # Assume everything else is unchanged.
  return values

def main():
  parser = OptionParser()
  parser.add_option("-r", "--replace", action="append",
    help="Replaces substrings. If passed a=b, replaces all substrs a with b.")
  (options, args) = parser.parse_args()

  if len(args) != 1:
    raise Exception("Need one argument which is the .gypi file to read.")

  data = LoadPythonDictionary(args[0])
  if options.replace:
    # Do replacements for all specified patterns.
    for replace in options.replace:
      split = replace.split('=')
      # Allow "foo=" to replace with nothing.
      if len(split) == 1:
        split.append('')
      assert len(split) == 2, "Replacement must be of the form 'key=value'."
      data = ReplaceSubstrings(data, split[0], split[1])

  print gn_helpers.ToGNString(data)

if __name__ == '__main__':
  try:
    main()
  except Exception, e:
    print str(e)
    sys.exit(1)
