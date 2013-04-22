# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

import easy_template

class LandingPage(object):
  def __init__(self):
    self.section_list = ['Tools', 'API', 'Concepts']
    self.section_map = collections.defaultdict(list)

  def GeneratePage(self, template_path):
    with open(template_path) as template_file:
      template = template_file.read()
    template_dict = { 'section_map': self.section_map }
    return easy_template.RunTemplateString(template, template_dict)

  def AddDesc(self, desc):
    group = desc['GROUP']
    assert group in self.section_list
    self.section_map[group].append(desc)
