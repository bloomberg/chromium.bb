#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys
import tempfile

REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))
DOCLAVA_DIR = os.path.join(REPOSITORY_ROOT, 'buildtools', 'android', 'doclava')
SDK_DIR = os.path.join(REPOSITORY_ROOT, 'third_party', 'android_tools', 'sdk')

sys.path.append(os.path.join(REPOSITORY_ROOT, 'build/android/gyp/util'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'net/tools/net_docs'))
import build_utils
import net_docs
from markdown.postprocessors import Postprocessor
from markdown.extensions import Extension


class CronetPostprocessor(Postprocessor):
  def run(self, text):
    return text.replace('@Override', '&commat;Override')


class CronetExtension(Extension):
  def extendMarkdown(self, md, md_globals):
    md.postprocessors.add('CronetPostprocessor',
                          CronetPostprocessor(md), '_end')


def GenerateJavadoc(options, src_dir):
  output_dir = os.path.abspath(os.path.join(options.output_dir, 'javadoc'))
  working_dir = os.path.join(options.input_dir, 'android', 'api')
  overview_file = os.path.abspath(options.overview_file)

  build_utils.DeleteDirectory(output_dir)
  build_utils.MakeDirectory(output_dir)
  javadoc_cmd = [
    'javadoc',
    '-d', output_dir,
    '-overview', overview_file,
    '-doclet', 'com.google.doclava.Doclava',
    '-docletpath',
    '%s:%s' % (os.path.join(DOCLAVA_DIR, 'jsilver.jar'),
               os.path.join(DOCLAVA_DIR, 'doclava.jar')),
    '-title', 'Cronet API',
    '-federate', 'Android', 'https://developer.android.com/',
    '-federationapi', 'Android', os.path.join(DOCLAVA_DIR, 'current.txt'),
    '-bootclasspath',
    '%s:%s' % (os.path.join(SDK_DIR, 'platforms', 'android-26', 'android.jar'),
               os.path.join(SDK_DIR, 'extras', 'android', 'support',
                            'annotations', 'android-support-annotations.jar')),
  ]
  for subdir, _, files in os.walk(src_dir):
    for filename in files:
      if filename.endswith(".java"):
        javadoc_cmd += [os.path.join(subdir, filename)]
  try:
    build_utils.CheckOutput(javadoc_cmd, cwd=working_dir,
        fail_func=lambda ret, stderr: (ret != 0 or not stderr is ''))
  except build_utils.CalledProcessError:
    build_utils.DeleteDirectory(output_dir)
    raise


def main():
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--output-dir', help='Directory to put javadoc')
  parser.add_option('--input-dir', help='Root of cronet source')
  parser.add_option('--input-src-jar', help='Cronet api source jar')
  parser.add_option('--overview-file', help='Path of the overview page')
  parser.add_option('--readme-file', help='Path of the README.md')
  parser.add_option('--stamp', help='Path to touch on success.')

  options, _ = parser.parse_args()
  # A temporary directory to put the output of cronet api source jar files.
  unzipped_jar_path = tempfile.mkdtemp(dir=options.output_dir)
  if os.path.exists(options.input_src_jar):
    jar_cmd = ['jar', 'xf', os.path.abspath(options.input_src_jar)]
    build_utils.CheckOutput(jar_cmd, cwd=unzipped_jar_path)
  else:
    raise Exception('Jar file does not exist: %s' % options.input_src_jar)

  net_docs.ProcessDocs([options.readme_file], options.input_dir,
                       options.output_dir, extensions=[CronetExtension()])

  GenerateJavadoc(options, os.path.abspath(unzipped_jar_path))

  if options.stamp:
    build_utils.Touch(options.stamp)
  if options.depfile:
    assert options.stamp
    deps = []
    for root, _, filenames in os.walk(options.input_dir):
      deps.extend(os.path.join(root, f) for f in filenames)
    build_utils.WriteDepfile(options.depfile, options.stamp, deps)
  # Clean up temporary output directory.
  build_utils.DeleteDirectory(unzipped_jar_path)

if __name__ == '__main__':
  sys.exit(main())
