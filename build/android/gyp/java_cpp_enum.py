#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import re
import optparse
import os
from string import Template
import sys

from util import build_utils

class EnumDefinition(object):
  def __init__(self, class_name=None, class_package=None, entries=None):
    self.class_name = class_name
    self.class_package = class_package
    self.entries = collections.OrderedDict(entries or [])
    self.prefix_to_strip = ''

  def AppendEntry(self, key, value):
    if key in self.entries:
      raise Exception('Multiple definitions of key %s found.' % key)
    self.entries[key] = value

  def Finalize(self):
    self._Validate()
    self._AssignEntryIndices()
    self._StripPrefix()

  def _Validate(self):
    assert self.class_name
    assert self.class_package
    assert self.entries

  def _AssignEntryIndices(self):
    # Supporting the same set enum value assignments the compiler does is rather
    # complicated, so we limit ourselves to these cases:
    # - all the enum constants have values assigned,
    # - enum constants reference other enum constants or have no value assigned.

    if not all(self.entries.values()):
      index = 0
      for key, value in self.entries.iteritems():
        if not value:
          self.entries[key] = index
          index = index + 1
        elif value in self.entries:
          self.entries[key] = self.entries[value]
        else:
          raise Exception('You can only reference other enum constants unless '
                          'you assign values to all of the constants.')

  def _StripPrefix(self):
    if not self.prefix_to_strip:
      prefix_to_strip = re.sub('(?!^)([A-Z]+)', r'_\1', self.class_name).upper()
      prefix_to_strip += '_'
      if not all([w.startswith(prefix_to_strip) for w in self.entries.keys()]):
        prefix_to_strip = ''
    else:
      prefix_to_strip = self.prefix_to_strip
    entries = ((k.replace(prefix_to_strip, '', 1), v) for (k, v) in
               self.entries.iteritems())
    self.entries = collections.OrderedDict(entries)

class HeaderParser(object):
  single_line_comment_re = re.compile(r'\s*//')
  multi_line_comment_start_re = re.compile(r'\s*/\*')
  enum_start_re = re.compile(r'^\s*enum\s+(\w+)\s+{\s*$')
  enum_line_re = re.compile(r'^\s*(\w+)(\s*\=\s*([^,\n]+))?,?\s*$')
  enum_end_re = re.compile(r'^\s*}\s*;\s*$')
  generator_directive_re = re.compile(
      r'^\s*//\s+GENERATED_JAVA_(\w+)\s*:\s*([\.\w]+)$')

  def __init__(self, lines):
    self._lines = lines
    self._enum_definitions = []
    self._in_enum = False
    self._current_definition = None
    self._generator_directives = {}

  def ParseDefinitions(self):
    for line in self._lines:
      self._ParseLine(line)
    return self._enum_definitions

  def _ParseLine(self, line):
    if not self._in_enum:
      self._ParseRegularLine(line)
    else:
      self._ParseEnumLine(line)

  def _ParseEnumLine(self, line):
    if HeaderParser.single_line_comment_re.match(line):
      return
    if HeaderParser.multi_line_comment_start_re.match(line):
      raise Exception('Multi-line comments in enums are not supported.')
    enum_end = HeaderParser.enum_end_re.match(line)
    enum_entry = HeaderParser.enum_line_re.match(line)
    if enum_end:
      self._ApplyGeneratorDirectives()
      self._current_definition.Finalize()
      self._enum_definitions.append(self._current_definition)
      self._in_enum = False
    elif enum_entry:
      enum_key = enum_entry.groups()[0]
      enum_value = enum_entry.groups()[2]
      self._current_definition.AppendEntry(enum_key, enum_value)

  def _GetCurrentEnumPackageName(self):
    return self._generator_directives.get('ENUM_PACKAGE')

  def _GetCurrentEnumPrefixToStrip(self):
    return self._generator_directives.get('PREFIX_TO_STRIP', '')

  def _ApplyGeneratorDirectives(self):
    current_definition = self._current_definition
    current_definition.class_package = self._GetCurrentEnumPackageName()
    current_definition.prefix_to_strip = self._GetCurrentEnumPrefixToStrip()
    self._generator_directives = {}

  def _ParseRegularLine(self, line):
    enum_start = HeaderParser.enum_start_re.match(line)
    generator_directive = HeaderParser.generator_directive_re.match(line)
    if enum_start:
      if not self._GetCurrentEnumPackageName():
        return
      self._current_definition = EnumDefinition()
      self._current_definition.class_name = enum_start.groups()[0]
      self._in_enum = True
    elif generator_directive:
      directive_name = generator_directive.groups()[0]
      directive_value = generator_directive.groups()[1]
      self._generator_directives[directive_name] = directive_value


def GetScriptName():
  script_components = os.path.abspath(sys.argv[0]).split(os.path.sep)
  build_index = script_components.index('build')
  return os.sep.join(script_components[build_index:])


def DoGenerate(options, source_paths):
  output_paths = []
  for source_path in source_paths:
    enum_definitions = DoParseHeaderFile(source_path)
    for enum_definition in enum_definitions:
      package_path = enum_definition.class_package.replace('.', os.path.sep)
      file_name = enum_definition.class_name + '.java'
      output_path = os.path.join(options.output_dir, package_path, file_name)
      output_paths.append(output_path)
      if not options.print_output_only:
        build_utils.MakeDirectory(os.path.dirname(output_path))
        DoWriteOutput(source_path, output_path, enum_definition)
  return output_paths


def DoParseHeaderFile(path):
  with open(path) as f:
    return HeaderParser(f.readlines()).ParseDefinitions()


def GenerateOutput(source_path, enum_definition):
  template = Template("""
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     ${SCRIPT_NAME}
// From
//     ${SOURCE_PATH}

package ${PACKAGE};

public class ${CLASS_NAME} {
${ENUM_ENTRIES}
}
""")

  enum_template = Template('  public static final int ${NAME} = ${VALUE};')
  enum_entries_string = []
  for enum_name, enum_value in enum_definition.entries.iteritems():
    values = {
        'NAME': enum_name,
        'VALUE': enum_value,
    }
    enum_entries_string.append(enum_template.substitute(values))
  enum_entries_string = '\n'.join(enum_entries_string)

  values = {
      'CLASS_NAME': enum_definition.class_name,
      'ENUM_ENTRIES': enum_entries_string,
      'PACKAGE': enum_definition.class_package,
      'SCRIPT_NAME': GetScriptName(),
      'SOURCE_PATH': source_path,
  }
  return template.substitute(values)


def DoWriteOutput(source_path, output_path, enum_definition):
  with open(output_path, 'w') as out_file:
    out_file.write(GenerateOutput(source_path, enum_definition))

def AssertFilesList(output_paths, assert_files_list):
  actual = set(output_paths)
  expected = set(assert_files_list)
  if not actual == expected:
    need_to_add = list(actual - expected)
    need_to_remove = list(expected - actual)
    raise Exception('Output files list does not match expectations. Please '
                    'add %s and remove %s.' % (need_to_add, need_to_remove))

def DoMain(argv):
  parser = optparse.OptionParser()

  parser.add_option('--assert_file', action="append", default=[],
                    dest="assert_files_list", help='Assert that the given '
                    'file is an output. There can be multiple occurrences of '
                    'this flag.')
  parser.add_option('--output_dir', help='Base path for generated files.')
  parser.add_option('--print_output_only', help='Only print output paths.',
                    action='store_true')

  options, args = parser.parse_args(argv)

  output_paths = DoGenerate(options, args)

  if options.assert_files_list:
    AssertFilesList(output_paths, options.assert_files_list)

  return " ".join(output_paths)

if __name__ == '__main__':
  DoMain(sys.argv[1:])
