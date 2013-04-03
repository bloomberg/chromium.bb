#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes dependency ordered list of native libraries.

The list excludes any Android system libraries, as those are not bundled with
the APK.

This list of libraries is used for several steps of building an APK.
In the component build, the --input-libraries only needs to be the top-level
library (i.e. libcontent_shell_content_view). This will then use readelf to
inspect the shared libraries and determine the full list of (non-system)
libraries that should be included in the APK.
"""

# TODO(cjhopman): See if we can expose the list of library dependencies from
# gyp, rather than calculating it ourselves.
# http://crbug.com/225558

import json
import optparse
import os
import re
import sys

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib import build_utils


_options = None
_libraries_dir = None
_library_re = re.compile(
    '.*NEEDED.*Shared library: \[(?P<library_name>[\w/.]+)\]')


def FullLibraryPath(library_name):
  return '%s/%s' % (_libraries_dir, library_name)


def IsSystemLibrary(library_name):
  # If the library doesn't exist in the libraries directory, assume that it is
  # an Android system library.
  return not os.path.exists(FullLibraryPath(library_name))


def CallReadElf(library_name):
  readelf_cmd = [_options.readelf,
                 '-d',
                 FullLibraryPath(library_name)]
  return build_utils.CheckCallDie(readelf_cmd)


def GetDependencies(library_name):
  elf = CallReadElf(library_name)
  return set(_library_re.findall(elf))


def GetNonSystemDependencies(library_name):
  all_deps = GetDependencies(library_name)
  return set((lib for lib in all_deps if not IsSystemLibrary(lib)))


def GetSortedTransitiveDependencies(libraries):
  """Returns all transitive library dependencies in dependency order."""
  def GraphNode(library):
    return (library, GetNonSystemDependencies(library))

  # First: find all library dependencies.
  unchecked_deps = libraries
  all_deps = set(libraries)
  while unchecked_deps:
    lib = unchecked_deps.pop()
    new_deps = GetNonSystemDependencies(lib).difference(all_deps)
    unchecked_deps.extend(new_deps)
    all_deps = all_deps.union(new_deps)

  # Then: simple, slow topological sort.
  sorted_deps = []
  unsorted_deps = dict(map(GraphNode, all_deps))
  while unsorted_deps:
    for library, dependencies in unsorted_deps.items():
      if not dependencies.intersection(unsorted_deps.keys()):
        sorted_deps.append(library)
        del unsorted_deps[library]

  return sorted_deps


def main(argv):
  parser = optparse.OptionParser()

  parser.add_option('--input-libraries',
      help='A list of top-level input libraries.')
  parser.add_option('--readelf', help='Path to the readelf binary.')
  parser.add_option('--output', help='Path to the generated .json file.')
  parser.add_option('--stamp', help='Path to touch on success.')

  global _options
  _options, _ = parser.parse_args()

  libraries = build_utils.ParseGypList(_options.input_libraries)
  global _libraries_dir
  _libraries_dir = os.path.dirname(libraries[0])
  libraries = [os.path.basename(lib) for lib in libraries]

  libraries = GetSortedTransitiveDependencies(libraries)

  with open(_options.output, 'w') as outfile:
    json.dump(libraries, outfile)

  if _options.stamp:
    build_utils.Touch(_options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))


