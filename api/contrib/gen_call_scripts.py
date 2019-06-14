# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate build API caller scripts.

Creates executable scripts of the form service__method for each Build API
endpoint. The scripts can be called without arguments. They instead make
assumptions about the input and output json file names and locations.

The system supports checking in example files which are automatically copied
in to the input file location. The example files should be along side this
script and of the form "service__method_example_input.json". When not found,
it will write out an empty json dict to the input file.
"""

from __future__ import print_function

import os
import re
import shutil

from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.scripts import build_api

_SCRIPT_TEMPLATE_FILE = os.path.join(os.path.dirname(__file__),
                                     'call_templates', 'script_template')
SCRIPT_TEMPLATE = osutils.ReadFile(_SCRIPT_TEMPLATE_FILE)
EXAMPLES_PATH = os.path.join(os.path.dirname(__file__), 'call_templates')
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), 'call_scripts')


def _camel_to_snake(string):
  # Transform FooBARBaz into FooBAR_Baz. Avoids making Foo_B_A_R_Baz.
  sub1 = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', string)
  # Transform FooBAR_Baz into Foo_BAR_Baz.
  sub2 = re.sub('([a-z0-9])([A-Z])', r'\1_\2', sub1)
  # Lower case to get foo_bar_baz.
  return sub2.lower()


def _get_services():
  """Get all service: [method, ...] mappings.

  Parses chromite.api.FooService/Method to foo: [method, ...].

  Returns:
    dict
  """
  router = build_api.Router()
  build_api.RegisterServices(router)

  return router.ListMethods()


def get_services():
  services = {}
  for service_method in _get_services():
    service, method = service_method.split('/')
    service_parts = service.split('.')
    full_service_name = service_parts[-1]
    service_name = full_service_name.replace('Service', '')

    final_service = _camel_to_snake(service_name)
    final_method = _camel_to_snake(method)

    if not final_service in services:
      services[final_service] = {'name': final_service,
                                 'full_name': full_service_name,
                                 'methods': []}

    services[final_service]['methods'].append({'name': final_method,
                                               'full_name': method})

  return services.values()


def write_script(filename, service, method):
  contents = SCRIPT_TEMPLATE % {'SERVICE': service, 'METHOD': method}
  script_path = os.path.join(OUTPUT_PATH, filename)
  osutils.WriteFile(script_path, contents, makedirs=True)
  os.chmod(script_path, 0o755)


def write_scripts():
  for service_data in get_services():
    for method_data in service_data['methods']:
      filename = '__'.join([service_data['name'], method_data['name']])
      logging.info('Writing %s', filename)
      write_script(filename, service_data['full_name'],
                   method_data['full_name'])

      example_input = os.path.join(EXAMPLES_PATH,
                                   '%s_example_input.json' % filename)
      input_file = os.path.join(OUTPUT_PATH, '%s_input.json' % filename)
      if os.path.exists(input_file) and osutils.ReadFile(input_file) != '{}':
        logging.info('%s exists, skipping.', input_file)
      elif os.path.exists(example_input):
        logging.info('Example %s exists, copying.', example_input)
        shutil.copy(example_input, input_file)
      else:
        logging.info('No input could be found, writing empty input.')
        osutils.WriteFile(input_file, '{}')

def main(_argv):
  write_scripts()
