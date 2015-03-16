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

import write_ordered_libraries


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

  # android_resources options
  parser.add_option('--srcjar', help='Path to target\'s resources srcjar.')
  parser.add_option('--resources-zip', help='Path to target\'s resources zip.')
  parser.add_option('--package-name',
      help='Java package name for these resources.')
  parser.add_option('--android-manifest', help='Path to android manifest.')

  # java library options
  parser.add_option('--jar-path', help='Path to target\'s jar output.')
  parser.add_option('--supports-android', action='store_true',
      help='Whether this library supports running on the Android platform.')
  parser.add_option('--requires-android', action='store_true',
      help='Whether this library requires running on the Android platform.')
  parser.add_option('--bypass-platform-checks', action='store_true',
      help='Bypass checks for support/require Android platform.')

  # android library options
  parser.add_option('--dex-path', help='Path to target\'s dex output.')

  # native library options
  parser.add_option('--native-libs', help='List of top-level native libs.')
  parser.add_option('--readelf-path', help='Path to toolchain\'s readelf.')

  options, args = parser.parse_args(argv)

  if args:
    parser.error('No positional arguments should be given.')


  if not options.type in [
      'java_library', 'android_resources', 'android_apk', 'deps_dex']:
    raise Exception('Unknown type: <%s>' % options.type)

  required_options = ['build_config'] + {
      'java_library': ['jar_path'],
      'android_resources': ['resources_zip'],
      'android_apk': ['jar_path', 'dex_path', 'resources_zip'],
      'deps_dex': ['dex_path']
    }[options.type]

  if options.native_libs:
    required_options.append('readelf_path')

  build_utils.CheckOptions(options, parser, required_options)

  if options.type == 'java_library':
    if options.supports_android and not options.dex_path:
      raise Exception('java_library that supports Android requires a dex path.')

    if options.requires_android and not options.supports_android:
      raise Exception(
          '--supports-android is required when using --requires-android')

  possible_deps_config_paths = build_utils.ParseGypList(
      options.possible_deps_configs)

  allow_unknown_deps = options.type == 'android_apk'
  unknown_deps = [
      c for c in possible_deps_config_paths if not os.path.exists(c)]
  if unknown_deps and not allow_unknown_deps:
    raise Exception('Unknown deps: ' + str(unknown_deps))

  direct_deps_config_paths = [
      c for c in possible_deps_config_paths if not c in unknown_deps]
  all_deps_config_paths = GetAllDepsConfigsInOrder(direct_deps_config_paths)

  direct_deps_configs = [GetDepConfig(p) for p in direct_deps_config_paths]
  all_deps_configs = [GetDepConfig(p) for p in all_deps_config_paths]

  direct_library_deps = DepsOfType('java_library', direct_deps_configs)
  all_library_deps = DepsOfType('java_library', all_deps_configs)

  direct_resources_deps = DepsOfType('android_resources', direct_deps_configs)
  all_resources_deps = DepsOfType('android_resources', all_deps_configs)
  # Resources should be ordered with the highest-level dependency first so that
  # overrides are done correctly.
  all_resources_deps.reverse()

  # Initialize some common config.
  config = {
    'deps_info': {
      'name': os.path.basename(options.build_config),
      'path': options.build_config,
      'type': options.type,
      'deps_configs': direct_deps_config_paths,
    }
  }
  deps_info = config['deps_info']


  if options.type == 'java_library' and not options.bypass_platform_checks:
    deps_info['requires_android'] = options.requires_android
    deps_info['supports_android'] = options.supports_android

    deps_require_android = (all_resources_deps +
        [d['name'] for d in all_library_deps if d['requires_android']])
    deps_not_support_android = (
        [d['name'] for d in all_library_deps if not d['supports_android']])

    if deps_require_android and not options.requires_android:
      raise Exception('Some deps require building for the Android platform: ' +
          str(deps_require_android))

    if deps_not_support_android and options.supports_android:
      raise Exception('Not all deps support the Android platform: ' +
          str(deps_not_support_android))


  if options.type in ['java_library', 'android_apk']:
    javac_classpath = [c['jar_path'] for c in direct_library_deps]
    java_full_classpath = [c['jar_path'] for c in all_library_deps]
    deps_info['resources_deps'] = [c['path'] for c in all_resources_deps]
    deps_info['jar_path'] = options.jar_path
    if options.type == 'android_apk' or options.supports_android:
      deps_info['dex_path'] = options.dex_path
    config['javac'] = {
      'classpath': javac_classpath,
    }
    config['java'] = {
      'full_classpath': java_full_classpath
    }

  if options.type == 'java_library':
    # Only resources might have srcjars (normal srcjar targets are listed in
    # srcjar_deps). A resource's srcjar contains the R.java file for those
    # resources, and (like Android's default build system) we allow a library to
    # refer to the resources in any of its dependents.
    config['javac']['srcjars'] = [
        c['srcjar'] for c in direct_resources_deps if 'srcjar' in c]

  if options.type == 'android_apk':
    # Apks will get their resources srcjar explicitly passed to the java step.
    config['javac']['srcjars'] = []

  if options.type == 'android_resources':
    deps_info['resources_zip'] = options.resources_zip
    if options.srcjar:
      deps_info['srcjar'] = options.srcjar
    if options.package_name:
      deps_info['package_name'] = options.package_name

  if options.type == 'android_resources' or options.type == 'android_apk':
    config['resources'] = {}
    config['resources']['dependency_zips'] = [
        c['resources_zip'] for c in all_resources_deps]
    config['resources']['extra_package_names'] = []

  if options.type == 'android_apk':
    config['resources']['extra_package_names'] = [
        c['package_name'] for c in all_resources_deps if 'package_name' in c]


  # Dependencies for the final dex file of an apk or a 'deps_dex'.
  if options.type in ['android_apk', 'deps_dex']:
    config['final_dex'] = {}
    dex_config = config['final_dex']
    # TODO(cjhopman): proguard version
    dex_deps_files = [c['dex_path'] for c in all_library_deps]
    dex_config['dependency_dex_files'] = dex_deps_files

  if options.type == 'android_apk':
    config['dist_jar'] = {
      'dependency_jars': [
        c['jar_path'] for c in all_library_deps
      ]
    }

    library_paths = []
    java_libraries_list = []
    if options.native_libs:
      libraries = build_utils.ParseGypList(options.native_libs)
      if libraries:
        libraries_dir = os.path.dirname(libraries[0])
        write_ordered_libraries.SetReadelfPath(options.readelf_path)
        write_ordered_libraries.SetLibraryDirs([libraries_dir])
        all_native_library_deps = (
            write_ordered_libraries.GetSortedTransitiveDependenciesForBinaries(
                libraries))
        # Create a java literal array with the "base" library names:
        # e.g. libfoo.so -> foo
        java_libraries_list = '{%s}' % ','.join(
            ['"%s"' % s[3:-3] for s in all_native_library_deps])
        library_paths = map(
            write_ordered_libraries.FullLibraryPath, all_native_library_deps)

      config['native'] = {
        'libraries': library_paths,
        'java_libraries_list': java_libraries_list
      }

  build_utils.WriteJson(config, options.build_config, only_if_changed=True)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        all_deps_config_paths + build_utils.GetPythonDependencies())


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
