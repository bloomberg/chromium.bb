#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Expands the ShellApk's AndroidManifest.xml using Mustache template engine."""

import codecs
import argparse
import os
import sys

# Import motemplate from third_party/motemplate
sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             os.pardir, os.pardir, 'third_party', 'motemplate'))
import motemplate
sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             os.pardir, os.pardir, 'build', 'android', 'gyp',
                             'util'))
import build_utils


def _ParseVariables(variables_arg, error_func):
  variables = {}
  for v in build_utils.ParseGnList(variables_arg):
    if '=' not in v:
      error_func('--variables argument must contain "=": ' + v)
    name, _, value = v.partition('=')
    variables[name] = value
  return variables


'''
Rewrite conditional sections to be 'Verted Sections' as defined in handlerbars.
This allows variables to be used in the AndroidManifest to conditionally define
include segments.
'''
def _RewriteConditionals(template_text):
  return template_text.replace('{{#', '{{?')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--template', required=True,
                      help='The template file to process.')
  parser.add_argument('--output', required=True,
                      help='The output file to generate.')
  parser.add_argument('--variables', help='Variables to be made available in '
                      'the template processing environment, as a GN list '
                      '(e.g. --variables "channel=beta mstone=39")', default='')
  options = parser.parse_args()

  variables = _ParseVariables(options.variables, parser.error)
  with open(options.template, 'r') as f:
    template = motemplate.Motemplate(_RewriteConditionals(f.read()))
    render = template.render(variables)
    with codecs.open(options.output, 'w', 'utf-8') as output_file:
      output_file.write(render.text)


if __name__ == '__main__':
  main()
