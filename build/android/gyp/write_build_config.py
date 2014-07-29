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
other targets that depend on that one.

There are several different types of build_configs:
  android_library: An android library containing java code.
  android_resources: A target containing android resources.

Android build scripts should not refer to the build_config directly, and the
build specification should instead pass information in using the special
file-arg syntax (see build_utils.py:ExpandFileArgs). That syntax allows passing
of values in a json dict in a file and looks like this:
  --python-arg=@FileArg(build_config_path:javac:classpath)

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

dep_config_cache = {}
def GetDepConfig(path):
  if not path in dep_config_cache:
    dep_config_cache[path] = build_utils.ReadJson(path)['deps_info']
  return dep_config_cache[path]


def DepsOfType(wanted_type, configs):
  return [c for c in configs if c['type'] == wanted_type]


def GetAllDepsConfigsInOrder(deps_config_paths):
  def Deps(path):
    return set(GetDepConfig(path)['deps_configs'])
  return build_utils.GetSortedTransitiveDependencies(deps_config_paths, Deps)


def main(argv):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--build-config', help='Path to build_config output.')
  parser.add_option(
      '--type',
      help='Type of this target (e.g. android_library).')
  parser.add_option(
      '--possible-deps-configs',
      help='List of paths for dependency\'s build_config files. Some '
      'dependencies may not write build_config files. Missing build_config '
      'files are handled differently based on the type of this target.')

  # android_resources/apk options
  parser.add_option('--srcjar', help='Path to target\'s resources srcjar.')
  parser.add_option('--resources-zip', help='Path to target\'s resources zip.')

  # android_library/apk options
  parser.add_option('--jar-path', help='Path to target\'s jar output.')
  parser.add_option('--dex-path', help='Path to target\'s dex output.')

  options, args = parser.parse_args(argv)

  if args:
    parser.error('No positional arguments should be given.')


  if not options.type in [
      'android_library', 'android_resources', 'android_apk']:
    raise Exception('Unknown type: <%s>' % options.type)


  required_options = ['build_config'] + {
      'android_library': ['jar_path', 'dex_path'],
      'android_resources': ['resources_zip'],
      'android_apk': ['jar_path', 'dex_path', 'resources_zip']
    }[options.type]

  build_utils.CheckOptions(options, parser, required_options)

  possible_deps_config_paths = build_utils.ParseGypList(
      options.possible_deps_configs)




  allow_unknown_deps = options.type == 'android_apk'
  unknown_deps = [
      c for c in possible_deps_config_paths if not os.path.exists(c)]
  if unknown_deps and not allow_unknown_deps:
    raise Exception('Unknown deps: ' + unknown_deps)

  direct_deps_config_paths = [
      c for c in possible_deps_config_paths if not c in unknown_deps]
  all_deps_config_paths = GetAllDepsConfigsInOrder(direct_deps_config_paths)

  direct_deps_configs = [GetDepConfig(p) for p in direct_deps_config_paths]
  all_deps_configs = [GetDepConfig(p) for p in all_deps_config_paths]

  direct_library_deps = DepsOfType('android_library', direct_deps_configs)
  all_resources_deps = DepsOfType('android_resources', all_deps_configs)
  all_library_deps = DepsOfType('android_library', all_deps_configs)

  # Initialize some common config.
  config = {
    'deps_info': {
      'path': options.build_config,
      'type': options.type,
      'deps_configs': direct_deps_config_paths,
    }
  }
  deps_info = config['deps_info']

  if options.type in ['android_library', 'android_apk']:
    javac_classpath = [c['jar_path'] for c in direct_library_deps]
    deps_info['resources_deps'] = [c['path'] for c in all_resources_deps]
    deps_info['jar_path'] = options.jar_path
    deps_info['dex_path'] = options.dex_path
    config['javac'] = {
      'classpath': javac_classpath,
    }
    # Only resources might have srcjars (normal srcjar targets are listed in
    # srcjar_deps). A resource's srcjar contains the R.java file for those
    # resources, and (like Android's default build system) we allow a library to
    # refer to the resources in any of its dependents.
    config['javac']['srcjars'] = [
        c['srcjar'] for c in all_resources_deps if 'srcjar' in c]

  if options.type == 'android_resources' or options.type == 'android_apk':
    deps_info['resources_zip'] = options.resources_zip
    if options.srcjar:
      deps_info['srcjar'] = options.srcjar

    config['resources'] = {}
    config['resources']['dependency_zips'] = [
        c['resources_zip'] for c in all_resources_deps]

  if options.type == 'android_apk':
    config['apk_dex'] = {}
    dex_config = config['apk_dex']
    # TODO(cjhopman): proguard version
    dex_deps_files = [c['dex_path'] for c in all_library_deps]
    dex_config['dependency_dex_files'] = dex_deps_files

  build_utils.WriteJson(config, options.build_config, only_if_changed=True)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        all_deps_config_paths + build_utils.GetPythonDependencies())


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
