#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Transforms direct Java class references in Android layout .xml files
according to the specified JarJar rules."""

import optparse
import os
import shutil
import sys
from xml.dom import minidom

from util import build_utils


class JarJarRules(object):
  def __init__(self, jarjar_rules):
    self._rules = []
    for line in jarjar_rules.splitlines():
      rule = line.split()
      if rule[0] != 'rule':
        continue
      _, src, dest = rule
      if src.endswith('**'):
        src_real_name = src[:-2]
      else:
        assert not '*' in src
        src_real_name = src

      if dest.endswith('@0'):
        self._rules.append((src, dest[:-2] + src_real_name))
      elif dest.endswith('@1'):
        assert '**' in src
        self._rules.append((src, dest[:-2]))
      else:
        assert not '@' in dest
        self._rules.append((src, dest))

  def RenameClass(self, class_name):
    for old, new in self._rules:
      if old.endswith('**') and old[:-2] in class_name:
        return class_name.replace(old[:-2], new, 1)
      if '*' not in old and class_name.endswith(old):
        return class_name.replace(old, new, 1)
    return class_name


def RenameNodes(node, rules):
  if node.nodeType == node.ELEMENT_NODE:
    if node.tagName.lower() == 'view' and  node.attributes.has_key('class'):
      node.attributes['class'] = rules.RenameClass(node.attributes['class'])
    else:
      node.tagName = rules.RenameClass(node.tagName)
  for child in node.childNodes:
    RenameNodes(child, rules)


def ProcessLayoutFile(path, rules):
  xmldoc = minidom.parse(path)
  RenameNodes(xmldoc.documentElement, rules)
  with open(path, 'w') as f:
    xmldoc.writexml(f)


def LayoutFilesFilter(src, names):
  if os.path.basename(src).lower() != 'layout':
    return []
  else:
    return filter(lambda n: n.endswith('.xml'), names)


def ProcessResources(options):
  with open(options.rules_path) as f:
    rules = JarJarRules(f.read())

  build_utils.DeleteDirectory(options.output_dir)
  for input_dir in options.input_dir:
    shutil.copytree(input_dir, options.output_dir)

  for root, _dirnames, filenames in os.walk(options.output_dir):
    layout_files = LayoutFilesFilter(root, filenames)
    for layout_file in layout_files:
      ProcessLayoutFile(os.path.join(root, layout_file), rules)


def ParseArgs():
  parser = optparse.OptionParser()
  parser.add_option('--input-dir', action='append',
                    help='Path to the resources folder to process.')
  parser.add_option('--output-dir',
                    help=('Directory to hold processed resources. Note: the ' +
                          'directory will be clobbered on every invocation.'))
  parser.add_option('--rules-path',
                    help='Path to the jarjar rules file.')
  parser.add_option('--stamp', help='Path to touch on success.')

  options, args = parser.parse_args()

  if args:
    parser.error('No positional arguments should be given.')

  # Check that required options have been provided.
  required_options = ('input_dir', 'output_dir', 'rules_path')
  build_utils.CheckOptions(options, parser, required=required_options)

  return options


def main():
  options = ParseArgs()

  ProcessResources(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main())
