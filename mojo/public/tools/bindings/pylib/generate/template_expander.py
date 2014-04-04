# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Based on:
# http://src.chromium.org/viewvc/blink/trunk/Source/build/scripts/template_expander.py

import inspect
import os
import sys

_current_dir = os.path.dirname(os.path.realpath(__file__))
# jinja2 is in the third_party directory (current directory is
# mojo/public/tools/bindings/pylib/generate).
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(1, os.path.join(_current_dir, os.pardir, os.pardir, os.pardir,
                                os.pardir, os.pardir, os.pardir, 'third_party'))
import jinja2


def ApplyTemplate(base_dir, path_to_template, params, filters=None):
  template_directory, template_name = os.path.split(path_to_template)
  path_to_templates = os.path.join(base_dir, template_directory)
  loader = jinja2.FileSystemLoader([path_to_templates])
  jinja_env = jinja2.Environment(loader=loader, keep_trailing_newline=True)
  if filters:
    jinja_env.filters.update(filters)
  template = jinja_env.get_template(template_name)
  return template.render(params)


def UseJinja(path_to_template, filters=None):
  # Get the directory of our caller's file.
  base_dir = os.path.dirname(inspect.getfile(sys._getframe(1)))
  def RealDecorator(generator):
    def GeneratorInternal(*args, **kwargs):
      parameters = generator(*args, **kwargs)
      return ApplyTemplate(base_dir, path_to_template, parameters,
                           filters=filters)
    GeneratorInternal.func_name = generator.func_name
    return GeneratorInternal
  return RealDecorator
