#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes a build_config file.

The build_config file for a target is a json file containing information about
how to build that target based on the target's dependencies. This includes
things like: the javac classpath, the list of android resources dependencies,
etc. It also includes the information needed to create the build_config for
other target's that depend on that one.

There are several different types of build_configs:
  android_library: An android library containing java code.

Android build scripts should not refer to the build_config directly, and the
build specification should instead pass information in using the special
file-arg syntax (see build_utils.py:ExpandFileArgs). That syntax allows passing
of values in a json dict in a file and looks like this:
  --python-arg=@(build_config_path):javac:classpath

Note: If paths to input files are passed in this way, it is important that:
  1. inputs/deps of the action ensure that the files are available the first
  time the action runs.
  2. Either (a) or (b)
    a. inputs/deps ensure that the action runs whenever one of the files changes
    b. the files are added to the action's depfile
"""

import optparse
import os
import sys

from util import build_utils

def main(argv):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--build-config', help='Path to build_config output.')
  parser.add_option('--type', help='Type of this target.')
  parser.add_option(
      '--possible-deps-configs',
      help='List of paths for dependency\'s build_config files. Some '
      'dependencies may not write build_config files. Missing build_config '
      'files are handled differently based on the type of this target.')

  # android_library options
  parser.add_option('--jar-path', help='Path to target\'s jar output.')

  options, args = parser.parse_args(argv)

  if args:
    parser.error('No positional arguments should be given.')

  required_options = ('build_config', 'type')
  build_utils.CheckOptions(options, parser, required_options)

  if options.type == 'android_library':
    required_options = ('jar_path',)
    build_utils.CheckOptions(options, parser, required_options)

  deps_configs = build_utils.ParseGypList(options.possible_deps_configs)
  for c in deps_configs:
    if not os.path.exists(c):
      # Currently we only allow deps to things that write build_config files.
      raise Exception('Unknown dep type: ' + c)

  deps_configs = [build_utils.ReadJson(c) for c in deps_configs]

  if options.type == 'android_library':
    javac_classpath = [c['outputs']['jar_path'] for c in deps_configs]
    config = {
      'outputs': {
        'jar_path': options.jar_path
      },
      'javac': {
        'classpath': javac_classpath
      }
    }
  else:
    raise Exception('Unknown type: ' + options.type)

  build_utils.WriteJson(config, options.build_config)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        build_utils.GetPythonDependencies())


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
