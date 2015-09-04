#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import shutil
import re
import sys
import textwrap

from util import build_utils
from util import md5_check

import jar

sys.path.append(build_utils.COLORAMA_ROOT)
import colorama


def ColorJavacOutput(output):
  fileline_prefix = r'(?P<fileline>(?P<file>[-.\w/\\]+.java):(?P<line>[0-9]+):)'
  warning_re = re.compile(
      fileline_prefix + r'(?P<full_message> warning: (?P<message>.*))$')
  error_re = re.compile(
      fileline_prefix + r'(?P<full_message> (?P<message>.*))$')
  marker_re = re.compile(r'\s*(?P<marker>\^)\s*$')

  warning_color = ['full_message', colorama.Fore.YELLOW + colorama.Style.DIM]
  error_color = ['full_message', colorama.Fore.MAGENTA + colorama.Style.BRIGHT]
  marker_color = ['marker',  colorama.Fore.BLUE + colorama.Style.BRIGHT]

  def Colorize(line, regex, color):
    match = regex.match(line)
    start = match.start(color[0])
    end = match.end(color[0])
    return (line[:start]
            + color[1] + line[start:end]
            + colorama.Fore.RESET + colorama.Style.RESET_ALL
            + line[end:])

  def ApplyColor(line):
    if warning_re.match(line):
      line = Colorize(line, warning_re, warning_color)
    elif error_re.match(line):
      line = Colorize(line, error_re, error_color)
    elif marker_re.match(line):
      line = Colorize(line, marker_re, marker_color)
    return line

  return '\n'.join(map(ApplyColor, output.split('\n')))


ERRORPRONE_OPTIONS = [
  # These crash on lots of targets.
  '-Xep:ParameterPackage:OFF',
  '-Xep:OverridesGuiceInjectableMethod:OFF',
  '-Xep:OverridesJavaxInjectableMethod:OFF',
]


def _FilterJavaFiles(paths, filters):
  return [f for f in paths
          if not filters or build_utils.MatchesGlob(f, filters)]


_MAX_MANIFEST_LINE_LEN = 72


def _CreateManifest(manifest_path, classpath, main_class=None,
                    manifest_entries=None):
  """Creates a manifest file with the given parameters.

  This generates a manifest file that compiles with the spec found at
  http://docs.oracle.com/javase/7/docs/technotes/guides/jar/jar.html#JAR_Manifest

  Args:
    manifest_path: The path to the manifest file that should be created.
    classpath: The JAR files that should be listed on the manifest file's
      classpath.
    main_class: If present, the class containing the main() function.
    manifest_entries: If present, a list of (key, value) pairs to add to
      the manifest.

  """
  output = ['Manifest-Version: 1.0']
  if main_class:
    output.append('Main-Class: %s' % main_class)
  if manifest_entries:
    for k, v in manifest_entries:
      output.append('%s: %s' % (k, v))
  if classpath:
    sanitized_paths = []
    for path in classpath:
      sanitized_paths.append(os.path.basename(path.strip('"')))
    output.append('Class-Path: %s' % ' '.join(sanitized_paths))
  output.append('Created-By: ')
  output.append('')

  wrapper = textwrap.TextWrapper(break_long_words=True,
                                 drop_whitespace=False,
                                 subsequent_indent=' ',
                                 width=_MAX_MANIFEST_LINE_LEN - 2)
  output = '\r\n'.join(w for l in output for w in wrapper.wrap(l))

  with open(manifest_path, 'w') as f:
    f.write(output)


def _ParseOptions(argv):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

  parser.add_option(
      '--src-gendirs',
      help='Directories containing generated java files.')
  parser.add_option(
      '--java-srcjars',
      action='append',
      default=[],
      help='List of srcjars to include in compilation.')
  parser.add_option(
      '--bootclasspath',
      action='append',
      default=[],
      help='Boot classpath for javac. If this is specified multiple times, '
      'they will all be appended to construct the classpath.')
  parser.add_option(
      '--classpath',
      action='append',
      help='Classpath for javac. If this is specified multiple times, they '
      'will all be appended to construct the classpath.')
  parser.add_option(
      '--use-ijars',
      action='store_true',
      help='Whether to use interface jars (.interface.jar) when compiling')
  parser.add_option(
      '--javac-includes',
      default='',
      help='A list of file patterns. If provided, only java files that match'
      'one of the patterns will be compiled.')
  parser.add_option(
      '--jar-excluded-classes',
      default='',
      help='List of .class file patterns to exclude from the jar.')

  parser.add_option(
      '--chromium-code',
      type='int',
      help='Whether code being compiled should be built with stricter '
      'warnings for chromium code.')

  parser.add_option(
      '--use-errorprone-path',
      help='Use the Errorprone compiler at this path.')

  parser.add_option('--jar-path', help='Jar output path.')
  parser.add_option(
      '--main-class',
      help='The class containing the main method.')
  parser.add_option(
      '--manifest-entry',
      action='append',
      help='Key:value pairs to add to the .jar manifest.')

  parser.add_option('--stamp', help='Path to touch on success.')

  options, args = parser.parse_args(argv)
  build_utils.CheckOptions(options, parser, required=('jar_path',))

  bootclasspath = []
  for arg in options.bootclasspath:
    bootclasspath += build_utils.ParseGypList(arg)
  options.bootclasspath = bootclasspath

  classpath = []
  for arg in options.classpath:
    classpath += build_utils.ParseGypList(arg)
  options.classpath = classpath

  java_srcjars = []
  for arg in options.java_srcjars:
    java_srcjars += build_utils.ParseGypList(arg)
  options.java_srcjars = java_srcjars

  if options.src_gendirs:
    options.src_gendirs = build_utils.ParseGypList(options.src_gendirs)

  options.javac_includes = build_utils.ParseGypList(options.javac_includes)
  options.jar_excluded_classes = (
      build_utils.ParseGypList(options.jar_excluded_classes))
  return options, args


def main(argv):
  colorama.init()

  argv = build_utils.ExpandFileArgs(argv)
  options, java_files = _ParseOptions(argv)

  if options.src_gendirs:
    java_files += build_utils.FindInDirectories(options.src_gendirs, '*.java')

  java_files = _FilterJavaFiles(java_files, options.javac_includes)

  runtime_classpath = options.classpath
  compile_classpath = runtime_classpath
  if options.use_ijars:
    ijar_re = re.compile(r'\.jar$')
    compile_classpath = (
        [ijar_re.sub('.interface.jar', p) for p in runtime_classpath])

  javac_args = [
      '-g',
      # Chromium only allows UTF8 source files.  Being explicit avoids
      # javac pulling a default encoding from the user's environment.
      '-encoding', 'UTF-8',
      '-classpath', ':'.join(compile_classpath),
      ]

  if options.bootclasspath:
    javac_args.extend([
        '-bootclasspath', ':'.join(options.bootclasspath),
        '-source', '1.7',
        '-target', '1.7',
        ])

  if options.chromium_code:
    # TODO(aurimas): re-enable '-Xlint:deprecation' checks once they are fixed.
    javac_args.extend(['-Xlint:unchecked'])
  else:
    # XDignore.symbol.file makes javac compile against rt.jar instead of
    # ct.sym. This means that using a java internal package/class will not
    # trigger a compile warning or error.
    javac_args.extend(['-XDignore.symbol.file'])

  javac_cmd = ['javac']
  if options.use_errorprone_path:
    javac_cmd = [options.use_errorprone_path] + ERRORPRONE_OPTIONS

  # Compute the list of paths that when changed, we need to rebuild.
  input_paths = options.bootclasspath + options.java_srcjars + java_files
  # TODO(agrieve): Remove this .TOC heuristic once GYP is no more.
  if not options.use_ijars:
    for path in compile_classpath:
      if os.path.exists(path + '.TOC'):
        input_paths.append(path + '.TOC')
      else:
        input_paths.append(path)
  python_deps = build_utils.GetPythonDependencies()

  def OnStaleMd5():
    with build_utils.TempDir() as temp_dir:
      if options.java_srcjars:
        java_dir = os.path.join(temp_dir, 'java')
        os.makedirs(java_dir)
        for srcjar in options.java_srcjars:
          build_utils.ExtractAll(srcjar, path=java_dir, pattern='*.java')
        jar_srcs = build_utils.FindInDirectory(java_dir, '*.java')
        java_files.extend(_FilterJavaFiles(jar_srcs, options.javac_includes))

      classes_dir = os.path.join(temp_dir, 'classes')
      os.makedirs(classes_dir)

      if java_files:
        # Don't include the output directory in the initial set of args since it
        # being in a temp dir makes it unstable (breaks md5 stamping).
        cmd = javac_cmd + javac_args + ['-d', classes_dir] + java_files

        build_utils.CheckOutput(
            cmd,
            print_stdout=options.chromium_code,
            stderr_filter=ColorJavacOutput)

      if options.main_class or options.manifest_entry:
        entries = []
        if options.manifest_entry:
          entries = [e.split(':') for e in options.manifest_entry]
        manifest_file = os.path.join(temp_dir, 'manifest')
        _CreateManifest(manifest_file, runtime_classpath, options.main_class,
                        entries)
      else:
        manifest_file = None
      jar.JarDirectory(classes_dir,
                       options.jar_excluded_classes,
                       options.jar_path,
                       manifest_file=manifest_file)

    if options.stamp:
      build_utils.Touch(options.stamp)

    if options.depfile:
      build_utils.WriteDepfile(options.depfile, input_paths + python_deps)


  # List python deps in input_strings rather than input_paths since the contents
  # of them does not change what gets written to the depsfile.
  md5_check.CallAndRecordIfStale(
      OnStaleMd5,
      record_path=options.jar_path + '.javac.md5.stamp',
      input_paths=input_paths,
      input_strings=javac_cmd + javac_args + python_deps,
      force=not os.path.exists(options.jar_path))



if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))


