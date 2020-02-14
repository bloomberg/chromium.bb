# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple script to search a generated depgraph for package relationships.

This is meant to more quickly and easily explore a board's depgraph because it
can be generated once and reused rather than re-querying portage.

The script gives the ancestors common to all the packages (plus lists any of
the packages themselves that are in all the other packages' dependencies).
The output of that step is simply an ordered list of packages. It also
generates all the pair-wise builds-before relationships.

The depgraph used is the one generated from the
DependencyService/GetBuildDependencyGraph endpoint. Use gen_call_scripts to
easily call it. By default, this script will use the default output of the
gen_call_scripts script for the endpoint. i.e.:

$> cd ~/chromiumos/chromite/api/contrib/
$> ./gen_call_scripts --board=betty
$> ./call_scripts/dependency__get_build_dependency_graph
$> ./depgraph_common_inheritance dev-libs/libxml2 app-text/docbook-xml-dtd
"""

from __future__ import print_function

import copy
import json
import os

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  default_depgraph = os.path.join(
      os.path.dirname(__file__), 'call_scripts',
      'dependency__get_build_dependency_graph_output.json')

  parser.add_argument(
      'packages',
      nargs='+',
      help='The packages to search for. They must be in the "category/package" '
           'format. At least 2 packages are required, but any number can be '
           'given.')
  parser.add_argument(
      '-d',
      '--depgraph',
      type='path',
      default=default_depgraph,
      help='The json output file containing the depgraph. By default uses the '
           'dependency__get_build_dependency_graph call_scripts output file.')
  parser.add_argument(
      '-b',
      '--backtrack',
      type=int,
      default=0,
      help='The number of layers to traverse back up the depgraph from the '
           'packages. 1 will search their immediate parents only, 2 will go '
           'through grandparents, and so on. Use 0 for unlimited, the default.')
  return parser


def _ParseArguments(argv):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  if not os.path.exists(opts.depgraph):
    parser.error('depgraph does not exist.')

  if len(opts.packages) < 2:
    parser.error('Must specify at least 2 packages.')

  opts.Freeze()
  return opts


def main(argv):
  opts = _ParseArguments(argv)

  depgraph = json.loads(osutils.ReadFile(opts.depgraph))

  # Build out the package: dependencies mapping from the depgraph.
  # Key depends on each of the values
  dependencies = {}
  for package_info in depgraph.get('depGraph', {}).get('packageDeps', []):
    pi = package_info['packageInfo']
    package = '%s/%s' % (pi['category'], pi['packageName'])

    packages = [
        '%s/%s' % (p['category'], p['packageName'])
        for p in package_info.get('dependencyPackages', [])
    ]
    dependencies[package] = set(packages)

  logging.info('Found %d packages.', len(dependencies))

  dep_lists = []
  for pkg in opts.packages:
    pkg_deps = set()
    if pkg not in dependencies:
      logging.notice('%s not found in depgraph.', pkg)
      dep_lists.append(pkg_deps)
      continue

    unprocessed = set(dependencies.get(pkg, []))
    # Include the package itself so that it will appear in the list if the
    # other packages depend on it.
    processed = {pkg}
    unlimited = not opts.backtrack
    limit = opts.backtrack if opts.backtrack else None
    while unprocessed and (limit or unlimited):
      parents = []
      for cur in unprocessed:
        if cur in processed:
          continue

        parents.extend(dependencies.get(cur, []))
        processed.add(cur)

      unprocessed = set(parents)
      if not unlimited:
        limit -= 1

    dep_lists.append(processed)

  # Combine them all into a single common list.
  common = copy.deepcopy(dep_lists[0])
  for deps in dep_lists[1:]:
    common &= deps

  if common:
    print('Common package dependencies:')
    print('\n'.join(sorted(list(common))))
  else:
    print('No common dependencies found.')

  # Find guaranteed build ordering pairs.
  # Try to find each package in the parent sets of the other packages.
  before = {pkg: [] for pkg in opts.packages}
  for pkg in opts.packages:
    others = set(opts.packages) - {pkg}
    unprocessed = set(dependencies.get(pkg, []))
    processed = set()
    while others and unprocessed:
      found = set()
      for other in others:
        if other in unprocessed:
          before[other].append(pkg)
          found.add(other)
      others = others - found
      parents = []
      for cur in unprocessed:
        if cur in processed:
          continue
        parents.extend(dependencies.get(cur, []))
        processed.add(cur)
      unprocessed = set(parents)

  # Remove cycles and build strings.
  before_strs = []
  for pkg, built_before in before.items():
    for cur in built_before:
      if pkg in before[cur]:
        continue
      before_strs.append('%s builds before %s' % (pkg, cur))

  print()
  if before_strs:
    print('Build order relations:')
    print('\n'.join(sorted(before_strs)))
  else:
    print('No guaranteed build order relations found.')
