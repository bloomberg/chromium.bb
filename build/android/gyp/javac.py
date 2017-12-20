#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import distutils.spawn
import optparse
import os
import shutil
import re
import sys

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


def _ExtractClassFiles(jar_path, dest_dir, java_files):
  """Extracts all .class files not corresponding to |java_files|."""
  # Two challenges exist here:
  # 1. |java_files| have prefixes that are not represented in the the jar paths.
  # 2. A single .java file results in multiple .class files when it contains
  #    nested classes.
  # Here's an example:
  #   source path: ../../base/android/java/src/org/chromium/Foo.java
  #   jar paths: org/chromium/Foo.class, org/chromium/Foo$Inner.class
  # To extract only .class files not related to the given .java files, we strip
  # off ".class" and "$*.class" and use a substring match against java_files.
  def extract_predicate(path):
    if not path.endswith('.class'):
      return False
    path_without_suffix = re.sub(r'(?:\$|\.)[^/]*class$', '', path)
    partial_java_path = path_without_suffix + '.java'
    return not any(p.endswith(partial_java_path) for p in java_files)

  build_utils.ExtractAll(jar_path, path=dest_dir, predicate=extract_predicate)
  for path in build_utils.FindInDirectory(dest_dir, '*.class'):
    shutil.copystat(jar_path, path)


def _ConvertToJMakeArgs(javac_cmd, pdb_path):
  new_args = ['bin/jmake', '-pdb', pdb_path, '-jcexec', javac_cmd[0]]
  if md5_check.PRINT_EXPLANATIONS:
    new_args.append('-Xtiming')

  do_not_prefix = ('-classpath', '-bootclasspath')
  skip_next = False
  for arg in javac_cmd[1:]:
    if not skip_next and arg not in do_not_prefix:
      arg = '-C' + arg
    new_args.append(arg)
    skip_next = arg in do_not_prefix

  return new_args


def _FixTempPathsInIncrementalMetadata(pdb_path, temp_dir):
  # The .pdb records absolute paths. Fix up paths within /tmp (srcjars).
  if os.path.exists(pdb_path):
    # Although its a binary file, search/replace still seems to work fine.
    with open(pdb_path) as fileobj:
      pdb_data = fileobj.read()
    with open(pdb_path, 'w') as fileobj:
      fileobj.write(re.sub(r'/tmp/[^/]*', temp_dir, pdb_data))


def _CheckPathMatchesClassName(java_file):
  package_name = ''
  class_name = None
  with open(java_file) as f:
    for l in f:
      # Strip unindented comments.
      # Considers a leading * as a continuation of a multi-line comment (our
      # linter doesn't enforce a space before it like there should be).
      l = re.sub(r'^(?://.*|/?\*.*?(?:\*/\s*|$))', '', l)

      m = re.match(r'package\s+(.*?);', l)
      if m and not package_name:
        package_name = m.group(1)

      # Not exactly a proper parser, but works for sources that Chrome uses.
      # In order to not match nested classes, it just checks for lack of indent.
      m = re.match(r'(?:\S.*?)?(?:class|@?interface|enum)\s+(.+?)\b', l)
      if m:
        if class_name:
          raise Exception(('File defines multiple top-level classes:\n    %s\n'
                           'This confuses compiles with '
                           'enable_incremental_javac=true.\n'
                           'classes=%s,%s\n') %
                          (java_file, class_name, m.groups(1)))
        class_name = m.group(1)

  if class_name is None:
    raise Exception('Unable to find a class within %s' % java_file)

  parts = package_name.split('.') + [class_name + '.java']
  expected_path_suffix = os.path.sep.join(parts)
  if not java_file.endswith(expected_path_suffix):
    raise Exception(('Java package+class name do not match its path.\n'
                     'Actual path: %s\nExpected path: %s') %
                    (java_file, expected_path_suffix))


def _OnStaleMd5(changes, options, javac_cmd, java_files, classpath_inputs):
  incremental = options.incremental
  # Don't bother enabling incremental compilation for third_party code, since
  # _CheckPathMatchesClassName() fails on some of it, and it's not really much
  # benefit.
  for java_file in java_files:
    if 'third_party' in java_file:
      incremental = False
    else:
      _CheckPathMatchesClassName(java_file)

  with build_utils.TempDir() as temp_dir:
    srcjars = options.java_srcjars

    classes_dir = os.path.join(temp_dir, 'classes')
    os.makedirs(classes_dir)

    changed_paths = None
    # jmake can handle deleted files, but it's a rare case and it would
    # complicate this script's logic.
    if incremental and changes.AddedOrModifiedOnly():
      changed_paths = set(changes.IterChangedPaths())
      # Do a full compile if classpath has changed.
      # jmake doesn't seem to do this on its own... Might be that ijars mess up
      # its change-detection logic.
      if any(p in changed_paths for p in classpath_inputs):
        changed_paths = None

    if options.incremental:
      pdb_path = options.jar_path + '.pdb'

    if incremental:
      # jmake is a compiler wrapper that figures out the minimal set of .java
      # files that need to be rebuilt given a set of .java files that have
      # changed.
      # jmake determines what files are stale based on timestamps between .java
      # and .class files. Since we use .jars, .srcjars, and md5 checks,
      # timestamp info isn't accurate for this purpose. Rather than use jmake's
      # programatic interface (like we eventually should), we ensure that all
      # .class files are newer than their .java files, and convey to jmake which
      # sources are stale by having their .class files be missing entirely
      # (by not extracting them).
      javac_cmd = _ConvertToJMakeArgs(javac_cmd, pdb_path)
      if srcjars:
        _FixTempPathsInIncrementalMetadata(pdb_path, temp_dir)

    if srcjars:
      java_dir = os.path.join(temp_dir, 'java')
      os.makedirs(java_dir)
      for srcjar in options.java_srcjars:
        if changed_paths:
          changed_paths.update(os.path.join(java_dir, f)
                               for f in changes.IterChangedSubpaths(srcjar))
        build_utils.ExtractAll(srcjar, path=java_dir, pattern='*.java')
      jar_srcs = build_utils.FindInDirectory(java_dir, '*.java')
      java_files.extend(jar_srcs)
      if changed_paths:
        # Set the mtime of all sources to 0 since we use the absence of .class
        # files to tell jmake which files are stale.
        for path in jar_srcs:
          os.utime(path, (0, 0))

    if java_files:
      if changed_paths:
        changed_java_files = [p for p in java_files if p in changed_paths]
        if os.path.exists(options.jar_path):
          _ExtractClassFiles(options.jar_path, classes_dir, changed_java_files)
        # Add the extracted files to the classpath. This is required because
        # when compiling only a subset of files, classes that haven't changed
        # need to be findable.
        try:
          classpath_idx = javac_cmd.index('-classpath')
          javac_cmd[classpath_idx + 1] += ':' + classes_dir
        except ValueError:
          # If there is no class path in the command line then add the arg
          javac_cmd.extend(["-classpath", classes_dir])

      # Can happen when a target goes from having no sources, to having sources.
      # It's created by the call to build_utils.Touch() below.
      if incremental:
        if os.path.exists(pdb_path) and not os.path.getsize(pdb_path):
          os.unlink(pdb_path)

      # Don't include the output directory in the initial set of args since it
      # being in a temp dir makes it unstable (breaks md5 stamping).
      cmd = javac_cmd + ['-d', classes_dir] + java_files

      # JMake prints out some diagnostic logs that we want to ignore.
      # This assumes that all compiler output goes through stderr.
      stdout_filter = lambda s: ''
      if md5_check.PRINT_EXPLANATIONS:
        stdout_filter = None

      attempt_build = lambda: build_utils.CheckOutput(
          cmd,
          print_stdout=options.chromium_code,
          stdout_filter=stdout_filter,
          stderr_filter=ColorJavacOutput)
      try:
        attempt_build()
      except build_utils.CalledProcessError as e:
        # Work-around for a bug in jmake (http://crbug.com/551449).
        if 'project database corrupted' not in e.output:
          raise
        print ('Applying work-around for jmake project database corrupted '
               '(http://crbug.com/551449).')
        os.unlink(pdb_path)
        attempt_build()

    if options.incremental and (not java_files or not incremental):
      # Make sure output exists.
      build_utils.Touch(pdb_path)

    jar.JarDirectory(classes_dir,
                     options.jar_path,
                     provider_configurations=options.provider_configurations,
                     additional_files=options.additional_jar_files)


def _ParseAndFlattenGnLists(gn_lists):
  ret = []
  for arg in gn_lists:
    ret.extend(build_utils.ParseGnList(arg))
  return ret


def _ParseOptions(argv):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

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
      '--java-version',
      help='Java language version to use in -source and -target args to javac.')
  parser.add_option(
      '--classpath',
      action='append',
      help='Classpath to use when annotation processors are present.')
  parser.add_option(
      '--interface-classpath',
      action='append',
      help='Classpath to use when no annotation processors are present.')
  parser.add_option(
      '--incremental',
      action='store_true',
      help='Whether to re-use .class files rather than recompiling them '
           '(when possible).')
  parser.add_option(
      '--processors',
      action='append',
      help='GN list of annotation processor main classes.')
  parser.add_option(
      '--processorpath',
      action='append',
      help='GN list of jars that comprise the classpath used for Annotation '
           'Processors.')
  parser.add_option(
      '--processor-arg',
      dest='processor_args',
      action='append',
      help='key=value arguments for the annotation processors.')
  parser.add_option(
      '--provider-configuration',
      dest='provider_configurations',
      action='append',
      help='File to specify a service provider. Will be included '
           'in the jar under META-INF/services.')
  parser.add_option(
      '--additional-jar-file',
      dest='additional_jar_files',
      action='append',
      help='Additional files to package into jar. By default, only Java .class '
           'files are packaged into the jar. Files should be specified in '
           'format <filename>:<path to be placed in jar>.')
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
      '--javac-arg',
      action='append',
      default=[],
      help='Additional arguments to pass to javac.')

  options, args = parser.parse_args(argv)
  build_utils.CheckOptions(options, parser, required=('jar_path',))

  options.bootclasspath = _ParseAndFlattenGnLists(options.bootclasspath)
  options.classpath = _ParseAndFlattenGnLists(options.classpath)
  options.interface_classpath = _ParseAndFlattenGnLists(
      options.interface_classpath)
  options.processorpath = _ParseAndFlattenGnLists(options.processorpath)
  options.processors = _ParseAndFlattenGnLists(options.processors)
  options.java_srcjars = _ParseAndFlattenGnLists(options.java_srcjars)

  if options.java_version == '1.8' and options.bootclasspath:
    # Android's boot jar doesn't contain all java 8 classes.
    # See: https://github.com/evant/gradle-retrolambda/issues/23.
    # Get the path of the jdk folder by searching for the 'jar' executable. We
    # cannot search for the 'javac' executable because goma provides a custom
    # version of 'javac'.
    jar_path = os.path.realpath(distutils.spawn.find_executable('jar'))
    jdk_dir = os.path.dirname(os.path.dirname(jar_path))
    rt_jar = os.path.join(jdk_dir, 'jre', 'lib', 'rt.jar')
    options.bootclasspath.append(rt_jar)

  additional_jar_files = []
  for arg in options.additional_jar_files or []:
    filepath, jar_filepath = arg.split(':')
    additional_jar_files.append((filepath, jar_filepath))
  options.additional_jar_files = additional_jar_files

  java_files = []
  for arg in args:
    # Interpret a path prefixed with @ as a file containing a list of sources.
    if arg.startswith('@'):
      java_files.extend(build_utils.ReadSourcesList(arg[1:]))
    else:
      java_files.append(arg)

  return options, java_files


def main(argv):
  colorama.init()

  argv = build_utils.ExpandFileArgs(argv)
  options, java_files = _ParseOptions(argv)

  if options.use_errorprone_path:
    javac_path = options.use_errorprone_path
  else:
    javac_path = distutils.spawn.find_executable('javac')
  javac_cmd = [javac_path]

  javac_cmd.extend((
    '-g',
    # Chromium only allows UTF8 source files.  Being explicit avoids
    # javac pulling a default encoding from the user's environment.
    '-encoding', 'UTF-8',
    # Prevent compiler from compiling .java files not listed as inputs.
    # See: http://blog.ltgt.net/most-build-tools-misuse-javac/
    '-sourcepath', ':',
  ))

  if options.java_version:
    javac_cmd.extend([
      '-source', options.java_version,
      '-target', options.java_version,
    ])

  if options.chromium_code:
    javac_cmd.extend(['-Xlint:unchecked', '-Xlint:deprecation'])
  else:
    # XDignore.symbol.file makes javac compile against rt.jar instead of
    # ct.sym. This means that using a java internal package/class will not
    # trigger a compile warning or error.
    javac_cmd.extend(['-XDignore.symbol.file'])

  if options.processors:
    javac_cmd.extend(['-processor', ','.join(options.processors)])

  if options.bootclasspath:
    javac_cmd.extend(['-bootclasspath', ':'.join(options.bootclasspath)])

  # Annotation processors crash when given interface jars.
  active_classpath = (
      options.classpath if options.processors else options.interface_classpath)
  if active_classpath:
    javac_cmd.extend(['-classpath', ':'.join(active_classpath)])

  if options.processorpath:
    javac_cmd.extend(['-processorpath', ':'.join(options.processorpath)])
  if options.processor_args:
    for arg in options.processor_args:
      javac_cmd.extend(['-A%s' % arg])

  javac_cmd.extend(options.javac_arg)

  classpath_inputs = (options.bootclasspath + options.interface_classpath +
                      options.processorpath)
  # GN already knows of java_files, so listing them just make things worse when
  # they change.
  depfile_deps = [javac_path] + classpath_inputs + options.java_srcjars
  input_paths = depfile_deps + java_files

  output_paths = [
      options.jar_path,
  ]
  if options.incremental:
    output_paths.append(options.jar_path + '.pdb')

  # An escape hatch to be able to check if incremental compiles are causing
  # problems.
  force = int(os.environ.get('DISABLE_INCREMENTAL_JAVAC', 0))

  # List python deps in input_strings rather than input_paths since the contents
  # of them does not change what gets written to the depsfile.
  build_utils.CallAndWriteDepfileIfStale(
      lambda changes: _OnStaleMd5(changes, options, javac_cmd, java_files,
                                  classpath_inputs),
      options,
      depfile_deps=depfile_deps,
      input_paths=input_paths,
      input_strings=javac_cmd,
      output_paths=output_paths,
      force=force,
      pass_changes=True)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
