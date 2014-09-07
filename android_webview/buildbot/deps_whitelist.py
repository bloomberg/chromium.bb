#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
"""Logic to generate lists of DEPS used by various parts of
the android_webview continuous integration (buildbot) infrastructure.

Note: The root Chromium project (which is not explicitly listed here)
has a couple of third_party libraries checked in directly into it. This means
that the list of third parties present in this file is not a comprehensive
list of third party android_webview dependencies.
"""

import argparse
import json
import logging
import os
import sys

# Add android_webview/tools to path to get at known_issues.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'tools'))
import known_issues


class DepsWhitelist(object):
  def __init__(self):
    # If a new DEPS entry is needed for the AOSP bot to compile please add it
    # here first.
    # This is a staging area for deps that are accepted by the android_webview
    # team and are in the process of having the required branches being created
    # in the Android tree.
    self._compile_but_not_snapshot_dependencies = [
    ]

    # Dependencies that need to be merged into the Android tree.
    self._snapshot_into_android_dependencies = [
      'sdch/open-vcdiff',
      'testing/gtest',
      'third_party/WebKit',
      'third_party/angle',
      'third_party/boringssl/src',
      'third_party/brotli/src',
      ('third_party/eyesfree/src/android/java/src/com/googlecode/eyesfree/'
       'braille'),
      'third_party/freetype',
      'third_party/icu',
      'third_party/leveldatabase/src',
      'third_party/libaddressinput/src',
      'third_party/libjingle/source/talk',
      'third_party/libjpeg_turbo',
      'third_party/libphonenumber/src/phonenumbers',
      'third_party/libphonenumber/src/resources',
      'third_party/libsrtp',
      'third_party/libvpx',
      'third_party/libyuv',
      'third_party/mesa/src',
      'third_party/openmax_dl',
      'third_party/opus/src',
      'third_party/ots',
      'third_party/sfntly/cpp/src',
      'third_party/skia',
      'third_party/smhasher/src',
      'third_party/usrsctp/usrsctplib',
      'third_party/webrtc',
      'third_party/yasm/source/patched-yasm',
      'tools/grit',
      'tools/gyp',
      'v8',
    ]

    # We can save some time by not rsyncing code we don't use.
    self._prune_from_rsync_build = [
        'third_party/WebKit/LayoutTests',
    ]

    # Dependencies required to build android_webview.
    self._compile_dependencies = (self._snapshot_into_android_dependencies +
                                  self._compile_but_not_snapshot_dependencies)

    # Dependencies required to run android_webview tests but not required to
    # compile.
    self._test_data_dependencies = [
      'chrome/test/data/perf/third_party/octane',
    ]

  @staticmethod
  def _read_deps_file(deps_file_path):
    class FileImplStub(object):
      """Stub for the File syntax."""
      def __init__(self, file_location):
        pass

      @staticmethod
      def GetPath():
        return ''

      @staticmethod
      def GetFilename():
        return ''

      @staticmethod
      def GetRevision():
        return None

    def from_stub(__, _=None):
      """Stub for the From syntax."""
      return ''

    class VarImpl(object):
      def __init__(self, custom_vars, local_scope):
        self._custom_vars = custom_vars
        self._local_scope = local_scope

      def Lookup(self, var_name):
        """Implements the Var syntax."""
        if var_name in self._custom_vars:
          return self._custom_vars[var_name]
        elif var_name in self._local_scope.get("vars", {}):
          return self._local_scope["vars"][var_name]
        raise Exception("Var is not defined: %s" % var_name)

    local_scope = {}
    var = VarImpl({}, local_scope)
    global_scope = {
        'File': FileImplStub,
        'From': from_stub,
        'Var': var.Lookup,
        'deps_os': {},
    }
    execfile(deps_file_path, global_scope, local_scope)
    deps = local_scope.get('deps', {})
    deps_os = local_scope.get('deps_os', {})
    for os_specific_deps in deps_os.itervalues():
      deps.update(os_specific_deps)
    return deps.keys()

  def _get_known_issues(self):
    issues = []
    for root, paths in known_issues.KNOWN_INCOMPATIBLE.items():
      for path in paths:
        issues.append(os.path.normpath(os.path.join(root, path)))
    return issues

  def _make_gclient_blacklist(self, deps_file_path, whitelisted_deps):
    """Calculates the list of deps that need to be excluded from the deps_file
    so that the only deps left are the one in the whitelist."""
    all_deps = self._read_deps_file(deps_file_path)
    # The list of deps read from the DEPS file are prefixed with the source
    # tree root, which is 'src' for Chromium.
    def prepend_root(path):
      return os.path.join('src', path)
    whitelisted_deps = map(prepend_root, whitelisted_deps)
    deps_blacklist = set(all_deps).difference(set(whitelisted_deps))
    return dict(map(lambda(x): (x, None), deps_blacklist))

  def _make_blacklist(self, deps_file_path, whitelisted_deps):
    """Calculates the list of paths we should exclude """
    all_deps = self._read_deps_file(deps_file_path)
    def remove_src_prefix(path):
      return path.replace('src/', '', 1)
    all_deps = map(remove_src_prefix, all_deps)
    # Ignore all deps except those whitelisted.
    blacklist = set(all_deps).difference(whitelisted_deps)
    # Ignore the 'known issues'. Typically these are the licence incompatible
    # things checked directly into Chromium.
    blacklist = blacklist.union(self._get_known_issues())
    # Ignore any other non-deps, non-licence paths we don't like.
    blacklist = blacklist.union(self._prune_from_rsync_build)
    return list(blacklist)

  def get_deps_for_android_build(self, deps_file_path):
    """This is used to calculate the custom_deps list for the Android bot.
    """
    if not deps_file_path:
      raise Exception('You need to specify a DEPS file path.')
    return self._make_gclient_blacklist(deps_file_path,
                                        self._compile_dependencies)

  def get_deps_for_android_build_and_test(self, deps_file_path):
    """This is used to calculate the custom_deps list for the Android perf bot.
    """
    if not deps_file_path:
      raise Exception('You need to specify a DEPS file path.')
    return self._make_gclient_blacklist(deps_file_path,
                                        self._compile_dependencies +
                                        self._test_data_dependencies)

  def get_blacklist_for_android_rsync_build(self, deps_file_path):
    """Calculates the list of paths we should exclude when building Android
    either because of license compatibility or because they are large and
    uneeded.
    """
    if not deps_file_path:
      raise Exception('You need to specify a DEPS file path.')
    return self._make_blacklist(deps_file_path, self._compile_dependencies)

  def get_deps_for_android_merge(self, _):
    """Calculates the list of deps that need to be merged into the Android tree
    in order to build the C++ and Java android_webview code."""
    return self._snapshot_into_android_dependencies

  def get_deps_for_license_check(self, _):
    """Calculates the list of deps that need to be checked for Android license
    compatibility"""
    return self._compile_dependencies


  def execute_method(self, method_name, deps_file_path):
    methods = {
      'android_build': self.get_deps_for_android_build,
      'android_build_and_test':
        self.get_deps_for_android_build_and_test,
      'android_merge': self.get_deps_for_android_merge,
      'license_check': self.get_deps_for_license_check,
      'android_rsync_build': self.get_blacklist_for_android_rsync_build,
    }
    if not method_name in methods:
      raise Exception('Method name %s is not valid. Valid choices are %s' %
                      (method_name, methods.keys()))
    return methods[method_name](deps_file_path)

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--method', help='Method to use to fetch from whitelist.',
                      required=True)
  parser.add_argument('--path-to-deps', help='Path to DEPS file.')
  parser.add_argument('--output-json', help='Name of file to write output to.')
  parser.add_argument('verbose', action='store_true', default=False)
  opts = parser.parse_args()

  logging.getLogger().setLevel(logging.DEBUG if opts.verbose else logging.WARN)

  deps_whitelist = DepsWhitelist()
  blacklist = deps_whitelist.execute_method(opts.method, opts.path_to_deps)

  if (opts.output_json):
    output_dict = {
        'blacklist' : blacklist
    }
    with open(opts.output_json, 'w') as output_json_file:
      json.dump(output_dict, output_json_file)
  else:
    print blacklist

  return 0


if __name__ == '__main__':
  sys.exit(main())
