#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Common tools for unit-testing writers.'''

import os
import unittest
import policy_template_generator
import template_formatter
import textwrap
import writer_configuration

class WriterUnittestCommon(unittest.TestCase):
  '''Common class for unittesting writers.'''

  # TODO(crbug.com/165412): Combine PrepareTest and GetOutput. In the first step
  # of this bug, the structure was preserved to keep the CL size down.
  def PrepareTest(self, policy_json):
    '''Prepares the data structure of policies.

    Args:
      policy_json: The policy data structure in JSON format.
    '''

    # Evaluate policy_json. For convenience, fix indentation in statements like
    # policy_json = '''
    #   {
    #     ...
    #   }''')
    start_idx = 1 if policy_json[0] == '\n' else 0
    return eval(textwrap.dedent(policy_json[start_idx:]))

  def GetOutput(self, policy_data, env_lang, env_defs, out_type, out_lang):
    '''Generates an output of a writer.

    Args:
      policy_data: The data returned from PrepareTest().
      env_lang: The environment language.
      env_defs: Environment definitions.
      out_type: Type of the output node for which output will be generated.
        This selects the writer.
      out_lang: Language of the output node for which output will be generated.
    '''

    config = writer_configuration.GetConfigurationForBuild(env_defs)
    policy_generator = \
        policy_template_generator.PolicyTemplateGenerator(config, policy_data)
    writer = template_formatter.GetWriter(out_type, config)
    return policy_generator.GetTemplateText(writer)
