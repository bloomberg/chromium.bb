# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Based on:
# http://src.chromium.org/viewvc/blink/trunk/Source/build/scripts/template_expander.py

import os
import sys

_current_dir = os.path.dirname(os.path.realpath(__file__))
# jinja2 is in the third_party directory.
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(1, os.path.join(_current_dir,
                                os.pardir,
                                os.pardir,
                                os.pardir,
                                os.pardir,
                                'third_party'))
import jinja2


def ApplyTemplate(path_to_template, params, filters=None):
  template_directory, template_name = os.path.split(path_to_template)
  path_to_templates = os.path.join(_current_dir, template_directory)
  loader = jinja2.FileSystemLoader([path_to_templates])
  jinja_env = jinja2.Environment(loader=loader, keep_trailing_newline=True)
  if filters:
    jinja_env.filters.update(filters)
  template = jinja_env.get_template(template_name)
  return template.render(params)


def UseJinja(path_to_template, filters=None):
  def RealDecorator(generator):
    def GeneratorInternal(*args, **kwargs):
      parameters = generator(*args, **kwargs)
      return ApplyTemplate(path_to_template, parameters, filters=filters)
    GeneratorInternal.func_name = generator.func_name
    return GeneratorInternal
  return RealDecorator
